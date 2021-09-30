/*
 * Wayland OpenGL functions
 *
 * Copyright 2000 Lionel Ulmer
 * Copyright 2005 Alex Woods
 * Copyright 2005 Raphael Junqueira
 * Copyright 2006-2009 Roderick Colenbrander
 * Copyright 2006 Tomas Carnecky
 * Copyright 2013 Matteo Bruni
 * Copyright 2012, 2013, 2014, 2017 Alexandre Julliard
 * Copyright 2020 Alexandros Frantzis for Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include "waylanddrv.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(waylanddrv);

#if defined(SONAME_LIBEGL) && (defined(SONAME_LIBGL) || defined(SONAME_LIBGLESV2))

#define GLAPIENTRY /* nothing */
#include "wine/wgl.h"
#undef GLAPIENTRY
#include "wine/wgl_driver.h"

#include "ntuser.h"
#include "winternl.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <assert.h>
#include <dlfcn.h>
#include <stdlib.h>

struct wgl_pixel_format
{
    EGLConfig config;
    EGLint native_visual_id;
};

struct wayland_gl_drawable
{
    struct wl_list  link;
    HWND            hwnd;
    int             format;
    int             width;
    int             height;
    struct wayland_surface *wayland_surface;
    struct gbm_surface *gbm_surface;
    EGLSurface      surface;
    struct wl_event_queue *wl_event_queue;
    struct wl_list  buffer_list;
    int             swap_interval;
    struct wl_callback *throttle_callback;
};

struct wayland_gl_buffer
{
    struct wl_list  link;
    struct wayland_gl_drawable *gl;
    struct gbm_bo *gbm_bo;
    struct gbm_surface *gbm_surface;
    struct wayland_dmabuf_buffer *dmabuf_buffer;
};

struct wgl_context
{
    struct wl_list link;
    EGLConfig  config;
    EGLContext context;
    HWND       draw_hwnd;
    HWND       read_hwnd;
    LONG       refresh;
    BOOL       has_been_current;
    BOOL       sharing;
    int        *attribs;
};

static void *egl_handle;
static void *opengl_handle;
static EGLDisplay egl_display;
static EGLint egl_version[2];
static struct opengl_funcs egl_funcs;
static char wgl_extensions[4096];
static struct wgl_pixel_format *pixel_formats;
static int nb_pixel_formats, nb_onscreen_formats;
static BOOL has_khr_create_context;
static BOOL has_gl_colorspace;

static struct wayland_mutex gl_object_mutex =
{
    PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP, 0, 0, __FILE__ ": gl_object_mutex"
};

static struct wl_list gl_drawables = { &gl_drawables, &gl_drawables };
static struct wl_list gl_contexts = { &gl_contexts, &gl_contexts };

#define DECL_FUNCPTR(f) static __typeof__(f) * p_##f = NULL
DECL_FUNCPTR(eglBindAPI);
DECL_FUNCPTR(eglCreateContext);
DECL_FUNCPTR(eglCreateWindowSurface);
DECL_FUNCPTR(eglDestroyContext);
DECL_FUNCPTR(eglDestroySurface);
DECL_FUNCPTR(eglGetConfigAttrib);
DECL_FUNCPTR(eglGetConfigs);
DECL_FUNCPTR(eglGetDisplay);
DECL_FUNCPTR(eglGetProcAddress);
DECL_FUNCPTR(eglInitialize);
DECL_FUNCPTR(eglMakeCurrent);
DECL_FUNCPTR(eglQueryString);
DECL_FUNCPTR(eglSwapBuffers);
#undef DECL_FUNCPTR

static void (*p_glFinish)(void);
static void (*p_glFlush)(void);

static inline BOOL is_onscreen_pixel_format(int format)
{
    return format > 0 && format <= nb_onscreen_formats;
}

static struct wayland_gl_drawable *wayland_gl_drawable_create(HWND hwnd, int format)
{
    struct wayland_gl_drawable *gl;
    struct wayland_surface *wayland_surface;

    gl = calloc(1, sizeof(*gl));
    if (!gl) return NULL;

    wayland_surface = wayland_surface_for_hwnd_lock(hwnd);

    TRACE("hwnd=%p wayland_surface=%p\n", hwnd, wayland_surface);

    if (wayland_surface)
    {
        BOOL ref_gl = wayland_surface_create_or_ref_glvk(wayland_surface);
        wayland_surface_for_hwnd_unlock(wayland_surface);
        if (!ref_gl) goto err;
    }

    gl->hwnd = hwnd;
    gl->format = format;
    gl->wayland_surface = wayland_surface;
    if (gl->wayland_surface)
    {
        gl->wl_event_queue = wl_display_create_queue(wayland_surface->wayland->wl_display);
        if (!gl->wl_event_queue) goto err;
    }
    wl_list_init(&gl->buffer_list);
    gl->swap_interval = 1;

    wayland_mutex_lock(&gl_object_mutex);
    wl_list_insert(&gl_drawables, &gl->link);
    return gl;

err:
    if (gl)
    {
        if (gl->wayland_surface) wayland_surface_unref_glvk(gl->wayland_surface);
        if (gl->wl_event_queue) wl_event_queue_destroy(gl->wl_event_queue);
        free(gl);
    }
    return NULL;
}

static void wayland_gl_buffer_destroy(struct wayland_gl_buffer *gl_buffer)
{
    TRACE("gl_buffer=%p bo=%p\n", gl_buffer, gl_buffer->gbm_bo);
    wl_list_remove(&gl_buffer->link);
    if (gl_buffer->dmabuf_buffer)
        wayland_dmabuf_buffer_destroy(gl_buffer->dmabuf_buffer);
    gbm_bo_set_user_data(gl_buffer->gbm_bo, NULL, NULL);
    free(gl_buffer);
}

static void wayland_gl_buffer_release(struct wayland_gl_buffer *gl_buffer)
{
    TRACE("gl_buffer=%p bo=%p\n", gl_buffer, gl_buffer->gbm_bo);
    gbm_surface_release_buffer(gl_buffer->gbm_surface, gl_buffer->gbm_bo);
}

static void wayland_gl_drawable_clear_buffers(struct wayland_gl_drawable *gl)
{
    struct wayland_gl_buffer *gl_buffer, *tmp;

    wl_list_for_each_safe(gl_buffer, tmp, &gl->buffer_list, link)
        wayland_gl_buffer_destroy(gl_buffer);
}

static void wayland_destroy_gl_drawable(HWND hwnd)
{
    struct wayland_gl_drawable *gl;

    wayland_mutex_lock(&gl_object_mutex);
    wl_list_for_each(gl, &gl_drawables, link)
    {
        if (gl->hwnd != hwnd) continue;
        wl_list_remove(&gl->link);
        wayland_gl_drawable_clear_buffers(gl);
        if (gl->surface) p_eglDestroySurface(egl_display, gl->surface);
        if (gl->gbm_surface) gbm_surface_destroy(gl->gbm_surface);
        if (gl->wayland_surface)
            wayland_surface_unref_glvk(gl->wayland_surface);
        if (gl->throttle_callback) wl_callback_destroy(gl->throttle_callback);
        if (gl->wl_event_queue) wl_event_queue_destroy(gl->wl_event_queue);
        free(gl);
        break;
    }
    wayland_mutex_unlock(&gl_object_mutex);
}

static struct wayland_gl_drawable *wayland_gl_drawable_get(HWND hwnd)
{
    struct wayland_gl_drawable *gl;

    if (!hwnd) return NULL;

    wayland_mutex_lock(&gl_object_mutex);
    wl_list_for_each(gl, &gl_drawables, link)
    {
        if (gl->hwnd == hwnd) return gl;
    }
    wayland_mutex_unlock(&gl_object_mutex);
    return NULL;
}

static void wayland_gl_drawable_release(struct wayland_gl_drawable *gl)
{
    if (gl) wayland_mutex_unlock(&gl_object_mutex);
}

static BOOL wgl_context_make_current(struct wgl_context *ctx, HWND draw_hwnd, HWND read_hwnd)
{
    BOOL ret;
    struct wayland_gl_drawable *draw_gl = NULL, *read_gl = NULL;

    draw_gl = wayland_gl_drawable_get(draw_hwnd);
    read_gl = wayland_gl_drawable_get(read_hwnd);

    TRACE("%p/%p context %p surface %p/%p\n",
          draw_hwnd, read_hwnd, ctx->context,
          draw_gl ? draw_gl->surface : NULL,
          read_gl ? read_gl->surface : NULL);

    ret = p_eglMakeCurrent(egl_display,
                           draw_gl ? draw_gl->surface : NULL,
                           read_gl ? read_gl->surface : NULL,
                           ctx->context);
    if (ret)
    {
        ctx->draw_hwnd = draw_hwnd;
        ctx->read_hwnd = read_hwnd;
        InterlockedExchange(&ctx->refresh, FALSE);
        ctx->has_been_current = TRUE;
        NtCurrentTeb()->glContext = ctx;
    }

    wayland_gl_drawable_release(read_gl);
    wayland_gl_drawable_release(draw_gl);

    return ret;
}

static struct gbm_surface *wayland_gl_create_gbm_surface(struct wayland_surface *glvk,
                                                         int width, int height,
                                                         uint32_t drm_format)
{
    struct wayland_dmabuf_format_info format_info;
    dev_t render_dev;
    struct wayland_dmabuf_surface_feedback *surface_feedback = glvk ? glvk->surface_feedback : NULL;
    struct gbm_surface *gbm_surface = NULL;

    if (!(render_dev = wayland_gbm_get_render_dev()))
    {
        ERR("Failed to get device's dev_t from GBM device.\n");
        goto out;
    }

    if (surface_feedback)
    {
        wayland_dmabuf_surface_feedback_lock(glvk->surface_feedback);
        if (surface_feedback->feedback)
        {
            if (wayland_dmabuf_feedback_get_format_info(surface_feedback->feedback, drm_format,
                                                        render_dev, &format_info))
            {
                TRACE("Using per-surface feedback format/modifier information\n");
                gbm_surface = wayland_gbm_create_surface(drm_format, width, height,
                                                         format_info.count_modifiers,
                                                         format_info.modifiers,
                                                         format_info.scanoutable);
            }
        }
        else
        {
            /*
             * Compositor supports feedback but we haven't processed surface
             * feedback events yet, so set surface_feedback to NULL to enter
             * the default format info code path below.
             */
            surface_feedback = NULL;
        }

        wayland_dmabuf_surface_feedback_unlock(glvk->surface_feedback);
    }

    if (!surface_feedback)
    {
        struct wayland_dmabuf *dmabuf = &wayland_process_acquire()->dmabuf;

        if (wayland_dmabuf_get_default_format_info(dmabuf, drm_format, render_dev, &format_info))
        {
            TRACE("Using default format/modifier information\n");
            gbm_surface = wayland_gbm_create_surface(drm_format, width, height,
                                                     format_info.count_modifiers,
                                                     format_info.modifiers,
                                                     format_info.scanoutable);
        }

        wayland_process_release();
    }

out:
    return gbm_surface;
}

static void wayland_gl_drawable_update(struct wayland_gl_drawable *gl)
{
    RECT client_rect;

    TRACE("hwnd=%p\n", gl->hwnd);

    wayland_gl_drawable_clear_buffers(gl);
    if (gl->surface) p_eglDestroySurface(egl_display, gl->surface);
    if (gl->gbm_surface) gbm_surface_destroy(gl->gbm_surface);

    NtUserGetClientRect(gl->hwnd, &client_rect);
    gl->width = client_rect.right;
    gl->height = client_rect.bottom;

    gl->gbm_surface =
        wayland_gl_create_gbm_surface(gl->wayland_surface ? gl->wayland_surface->glvk : NULL,
                                      gl->width, gl->height,
                                      pixel_formats[gl->format - 1].native_visual_id);
    if (!gl->gbm_surface)
        ERR("Failed to create GBM surface\n");

    /* First try to create a surface with an SRGB colorspace, if supported. */
    if (has_gl_colorspace)
    {
        EGLint attribs[] = { EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_SRGB, EGL_NONE };
        gl->surface = p_eglCreateWindowSurface(egl_display,
                                               pixel_formats[gl->format - 1].config,
                                               (EGLNativeWindowType) gl->gbm_surface,
                                               attribs);
        if (!gl->surface)
        {
            TRACE("Failed to create EGL surface with SRGB colorspace, "
                  "trying with default colorspace\n");
        }
    }

    /* Try to create a surface with the default colorspace. */
    if (!gl->surface)
    {
        gl->surface = p_eglCreateWindowSurface(egl_display,
                                               pixel_formats[gl->format - 1].config,
                                               (EGLNativeWindowType) gl->gbm_surface,
                                               NULL);
        if (!gl->surface)
            ERR("Failed to create EGL surface\n");
    }

    if (gl->surface)
    {
        struct wgl_context *ctx;

        wl_list_for_each(ctx, &gl_contexts, link)
        {
            if (ctx->draw_hwnd != gl->hwnd && ctx->read_hwnd != gl->hwnd) continue;
            TRACE("hwnd %p refreshing %p %scurrent\n",
                  gl->hwnd, ctx, NtCurrentTeb()->glContext == ctx ? "" : "not ");
            if (NtCurrentTeb()->glContext == ctx)
                wgl_context_make_current(ctx, ctx->draw_hwnd, ctx->read_hwnd);
            else
                InterlockedExchange(&ctx->refresh, TRUE);
        }
    }

    TRACE("hwnd=%p gbm_surface=%p egl_surface=%p\n",
          gl->hwnd, gl->gbm_surface, gl->surface);

    NtUserRedrawWindow(gl->hwnd, NULL, 0, RDW_INVALIDATE | RDW_ERASE);
}

static BOOL wayland_gl_surface_feedback_has_update(struct wayland_gl_drawable *gl)
{
    struct wayland_dmabuf_surface_feedback *surface_feedback =
        gl->wayland_surface ? gl->wayland_surface->glvk->surface_feedback : NULL;
    BOOL ret = FALSE;

    if (surface_feedback)
    {
        wayland_dmabuf_surface_feedback_lock(surface_feedback);
        ret = surface_feedback->surface_needs_update;
        surface_feedback->surface_needs_update = FALSE;
        wayland_dmabuf_surface_feedback_unlock(surface_feedback);
    }

    TRACE("hwnd=%p => %d\n", gl->hwnd, ret);

    return ret;
}

static BOOL wayland_gl_drawable_needs_resize(struct wayland_gl_drawable *gl)
{
    RECT client_rect;
    BOOL ret;

    NtUserGetClientRect(gl->hwnd, &client_rect);

    ret = (client_rect.right > 0 && client_rect.bottom > 0 &&
           (gl->width != client_rect.right || gl->height != client_rect.bottom));

    TRACE("hwnd=%p client=%dx%d gl=%dx%d => %d\n",
          gl->hwnd, (int)client_rect.right, (int)client_rect.bottom,
          gl->width, gl->height, ret);

    return ret;
}

static BOOL wayland_gl_drawable_needs_update(struct wayland_gl_drawable *gl)
{
    return wayland_gl_drawable_needs_resize(gl) || wayland_gl_surface_feedback_has_update(gl);
}

static void gbm_bo_destroy_callback(struct gbm_bo *bo, void *user_data)
{
    struct wayland_gl_buffer *gl_buffer = (struct wayland_gl_buffer *) user_data;
    wayland_gl_buffer_destroy(gl_buffer);
}

static void dmabuf_buffer_release(void *data, struct wl_buffer *buffer)
{
    struct wayland_gl_buffer *gl_buffer = (struct wayland_gl_buffer *) data;

    TRACE("bo=%p\n", gl_buffer->gbm_bo);
    wayland_gl_buffer_release(gl_buffer);
}

static const struct wl_buffer_listener dmabuf_buffer_listener = {
    dmabuf_buffer_release
};

static struct wayland_gl_buffer *wayland_gl_drawable_track_buffer(struct wayland_gl_drawable *gl,
                                                                  struct gbm_bo *bo)
{
    struct wayland_gl_buffer *gl_buffer =
        (struct wayland_gl_buffer *) gbm_bo_get_user_data(bo);

    if (!gl_buffer)
    {
        struct wayland_native_buffer native_buffer;

        gl_buffer = calloc(1, sizeof(*gl_buffer));
        if (!gl_buffer) goto err;

        wl_list_init(&gl_buffer->link);
        gl_buffer->gbm_bo = bo;
        gl_buffer->gbm_surface = gl->gbm_surface;
        if (!wayland_native_buffer_init_gbm(&native_buffer, bo)) goto err;

        if (gl->wayland_surface)
        {
            gl_buffer->dmabuf_buffer =
                wayland_dmabuf_buffer_create_from_native(gl->wayland_surface->wayland,
                                                         &native_buffer);
            wayland_native_buffer_deinit(&native_buffer);
            if (!gl_buffer->dmabuf_buffer) goto err;

            wl_proxy_set_queue((struct wl_proxy *) gl_buffer->dmabuf_buffer->wl_buffer,
                               gl->wl_event_queue);
            wl_buffer_add_listener(gl_buffer->dmabuf_buffer->wl_buffer,
                                   &dmabuf_buffer_listener, gl_buffer);
        }

        gbm_bo_set_user_data(bo, gl_buffer, gbm_bo_destroy_callback);
        wl_list_insert(&gl->buffer_list, &gl_buffer->link);
    }

    return gl_buffer;

err:
    if (gl_buffer) wayland_gl_buffer_destroy(gl_buffer);
    return NULL;
}

static void throttle_callback(void *data, struct wl_callback *callback, uint32_t time)
{
    struct wayland_gl_drawable *draw_gl = data;

    TRACE("hwnd=%p\n", draw_gl->hwnd);
    draw_gl->throttle_callback = NULL;
    wl_callback_destroy(callback);
}

static const struct wl_callback_listener throttle_listener = {
    throttle_callback
};

static BOOL wayland_gl_drawable_commit(struct wayland_gl_drawable *gl,
                                       struct wayland_gl_buffer *gl_buffer)
{
    BOOL committed = FALSE;

    if (!gl->wayland_surface) return FALSE;

    wayland_mutex_lock(&gl->wayland_surface->mutex);
    if (gl->wayland_surface->drawing_allowed)
    {
        struct wl_surface *gl_wl_surface = gl->wayland_surface->glvk->wl_surface;
        wayland_surface_ensure_mapped(gl->wayland_surface);
        wl_surface_attach(gl_wl_surface, gl_buffer->dmabuf_buffer->wl_buffer, 0, 0);
        wl_surface_damage_buffer(gl_wl_surface, 0, 0, INT32_MAX, INT32_MAX);
        if (gl->swap_interval > 0)
        {
            gl->throttle_callback = wl_surface_frame(gl_wl_surface);
            wl_proxy_set_queue((struct wl_proxy *) gl->throttle_callback,
                                gl->wl_event_queue);
            wl_callback_add_listener(gl->throttle_callback, &throttle_listener, gl);
        }
        wl_surface_commit(gl_wl_surface);
        committed = TRUE;
    }
    wayland_mutex_unlock(&gl->wayland_surface->mutex);

    return committed;
}

static UINT get_tick_count_since(UINT start)
{
    UINT now = NtGetTickCount();
    /* Handle tick count wrap around to zero. */
    if (now < start)
        return 0xffffffff - start + now + 1;
    else
        return now - start;
}

static void wayland_gl_drawable_throttle(struct wayland_gl_drawable *gl)
{
    static const UINT timeout = 100;
    UINT start, elapsed;

    if (gl->swap_interval == 0) goto out;

    start = NtGetTickCount();
    elapsed = 0;

    /* The compositor may at any time decide to not display the surface on
     * screen and thus not send any frame events. Until we have a better way to
     * deal with this, wait for a maximum of timeout for the frame event to
     * arrive, in order to avoid blocking the GL thread indefinitely. */
    while (gl->throttle_callback && elapsed < timeout &&
           wayland_dispatch_queue(gl->wl_event_queue, timeout - elapsed) != -1)
    {
        elapsed = get_tick_count_since(start);
    }

out:
    if (gl->throttle_callback)
    {
        wl_callback_destroy(gl->throttle_callback);
        gl->throttle_callback = NULL;
    }
}

static BOOL wgl_context_refresh(struct wgl_context *ctx)
{
    BOOL ret = InterlockedExchange(&ctx->refresh, FALSE);

    if (ret)
    {
        TRACE("refreshing context %p hwnd %p/%p\n",
              ctx->context, ctx->draw_hwnd, ctx->read_hwnd);
        wgl_context_make_current(ctx, ctx->draw_hwnd, ctx->read_hwnd);
        NtUserRedrawWindow(ctx->draw_hwnd, NULL, 0, RDW_INVALIDATE | RDW_ERASE);
    }
    return ret;
}

static BOOL set_pixel_format(HDC hdc, int format, BOOL allow_change)
{
    struct wayland_gl_drawable *gl;
    HWND hwnd = NtUserWindowFromDC(hdc);
    int prev = 0;
    BOOL needs_update = FALSE;

    if (!hwnd || hwnd == NtUserGetDesktopWindow())
    {
        WARN("not a proper window DC %p/%p\n", hdc, hwnd);
        return FALSE;
    }
    if (!is_onscreen_pixel_format(format))
    {
        WARN("Invalid format %d\n", format);
        return FALSE;
    }
    TRACE("%p/%p format %d\n", hdc, hwnd, format);

    if ((gl = wayland_gl_drawable_get(hwnd)))
    {
        prev = gl->format;
        /* If we are changing formats, destroy any existing EGL surface so that
         * it can be recreated by wayland_gl_drawable_update. */
        if (allow_change && gl->format != format)
        {
            gl->format = format;
            needs_update = TRUE;
        }
    }
    else
    {
        gl = wayland_gl_drawable_create(hwnd, format);
        needs_update = TRUE;
    }

    if (gl && needs_update) wayland_gl_drawable_update(gl);

    wayland_gl_drawable_release(gl);

    if (prev && prev != format && !allow_change) return FALSE;
    if (NtUserSetWindowPixelFormat(hwnd, format)) return TRUE;

    wayland_destroy_gl_drawable(hwnd);
    return FALSE;
}

struct egl_attribs
{
    EGLint *data;
    int count;
};

static void egl_attribs_init(struct egl_attribs *attribs)
{
    attribs->data = NULL;
    attribs->count = 0;
}

static void egl_attribs_add(struct egl_attribs *attribs, EGLint name, EGLint value)
{
    EGLint *new_data = realloc(attribs->data,
                               sizeof(*attribs->data) * (attribs->count + 2));
    if (!new_data)
    {
        ERR("Could not allocate memory for EGL attributes!\n");
        return;
    }

    attribs->data = new_data;
    attribs->data[attribs->count] = name;
    attribs->data[attribs->count + 1] = value;
    attribs->count += 2;
}


static void egl_attribs_add_15_khr(struct egl_attribs *attribs, EGLint name, EGLint value)
{
    BOOL has_egl_15 = egl_version[0] == 1 && egl_version[1] >= 5;

    if (!has_egl_15 && !has_khr_create_context)
    {
        WARN("Ignoring EGL context attrib %#x not supported by EGL %d.%d\n",
             name, egl_version[0], egl_version[1]);
        return;
    }

    if (name == EGL_CONTEXT_FLAGS_KHR && has_egl_15)
    {
        egl_attribs_add(attribs, EGL_CONTEXT_OPENGL_DEBUG,
                        (value & WGL_CONTEXT_DEBUG_BIT_ARB) ?
                             EGL_TRUE : EGL_FALSE);
        egl_attribs_add(attribs,
                        EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE,
                        (value & WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB) ?
                            EGL_TRUE : EGL_FALSE);
    }
    else
    {
        egl_attribs_add(attribs, name, value);
    }
}

static EGLint *egl_attribs_steal_finished_data(struct egl_attribs *attribs)
{
    EGLint *data = NULL;

    if (attribs->data)
    {
        data = realloc(attribs->data,
                       sizeof(*attribs->data) * (attribs->count + 1));
        if (!data)
        {
            ERR("Could not allocate memory for EGL attributes!\n");
        }
        else
        {
            data[attribs->count] = EGL_NONE;
            attribs->data = NULL;
            attribs->count = 0;
        }
    }

    return data;
}

static void egl_attribs_deinit(struct egl_attribs *attribs)
{
    free(attribs->data);
    attribs->data = NULL;
    attribs->count = 0;
}

static struct wgl_context *create_context(HDC hdc, struct wgl_context *share,
                                          struct egl_attribs *attribs)
{
    struct wayland_gl_drawable *gl;
    struct wgl_context *ctx;

    if (!(gl = wayland_gl_drawable_get(NtUserWindowFromDC(hdc)))) return NULL;

    ctx = malloc(sizeof(*ctx));
    if (!ctx)
    {
        ERR("Failed to allocate memory for GL context\n");
        goto out;
    }

    ctx->config  = pixel_formats[gl->format - 1].config;
    ctx->attribs = attribs ? egl_attribs_steal_finished_data(attribs) : NULL;
    ctx->context = p_eglCreateContext(egl_display, ctx->config,
                                      share ? share->context : EGL_NO_CONTEXT,
                                      ctx->attribs);
    ctx->draw_hwnd = 0;
    ctx->read_hwnd = 0;
    ctx->refresh = FALSE;
    ctx->has_been_current = FALSE;
    ctx->sharing = FALSE;

    /* The gl_object_mutex, which is locked when we get the gl_drawable,
     * also guards access to gl_contexts, so it's safe to add the entry here. */
    wl_list_insert(&gl_contexts, &ctx->link);

out:
    wayland_gl_drawable_release(gl);

    TRACE("ctx=%p hdc=%p fmt=%d egl_ctx=%p\n",
          ctx, hdc, gl->format, ctx ? ctx->context : NULL);

    return ctx;
}

/***********************************************************************
 *		wayland_wglCopyContext
 */
static BOOL wayland_wglCopyContext(struct wgl_context *src,
                                   struct wgl_context *dst, UINT mask)
{
    FIXME("%p -> %p mask %#x unsupported\n", src, dst, mask);
    return FALSE;
}

/***********************************************************************
 *		wayland_wglCreateContext
 */
static struct wgl_context *wayland_wglCreateContext(HDC hdc)
{
    TRACE("hdc=%p\n", hdc);

    p_eglBindAPI(EGL_OPENGL_API);

    return create_context(hdc, NULL, NULL);
}

/***********************************************************************
 *		wayland_wglCreateContextAttribsARB
 */
static struct wgl_context *wayland_wglCreateContextAttribsARB(HDC hdc,
                                                              struct wgl_context *share,
                                                              const int *attribs)
{
    struct egl_attribs egl_attribs = {0};
    EGLenum api_type = EGL_OPENGL_API;
    EGLenum profile_mask;
    struct wgl_context *ctx;

    egl_attribs_init(&egl_attribs);

    TRACE("hdc=%p share=%p attribs=%p\n", hdc, share, attribs);

    while (attribs && *attribs)
    {
        TRACE("%#x %#x\n", attribs[0], attribs[1]);
        switch (*attribs)
        {
        case WGL_CONTEXT_PROFILE_MASK_ARB:
            profile_mask = 0;
            if (attribs[1] & WGL_CONTEXT_ES2_PROFILE_BIT_EXT)
                api_type = EGL_OPENGL_ES_API;
            if (attribs[1] & WGL_CONTEXT_CORE_PROFILE_BIT_ARB)
                profile_mask |= EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
            if (attribs[1] & WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB)
                profile_mask |= EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT;
            /* If the WGL profile mask doesn't have ES2 as the only set bit,
             * pass the mask to EGL. Note that this will also pass empty
             * WGL masks, in order to elicit the respective EGL error. */
            if (attribs[1] != WGL_CONTEXT_ES2_PROFILE_BIT_EXT)
            {
                egl_attribs_add_15_khr(&egl_attribs,
                                       EGL_CONTEXT_OPENGL_PROFILE_MASK, profile_mask);
            }
            break;
        case WGL_CONTEXT_MAJOR_VERSION_ARB:
            egl_attribs_add(&egl_attribs, EGL_CONTEXT_MAJOR_VERSION, attribs[1]);
            break;
        case WGL_CONTEXT_MINOR_VERSION_ARB:
            egl_attribs_add_15_khr(&egl_attribs, EGL_CONTEXT_MINOR_VERSION, attribs[1]);
            break;
        case WGL_CONTEXT_FLAGS_ARB:
            egl_attribs_add_15_khr(&egl_attribs, EGL_CONTEXT_FLAGS_KHR, attribs[1]);
            break;
        default:
            FIXME("Unhandled attributes: %#x %#x\n", attribs[0], attribs[1]);
        }
        attribs += 2;
    }

    p_eglBindAPI(api_type);

    ctx = create_context(hdc, share, &egl_attribs);

    egl_attribs_deinit(&egl_attribs);

    return ctx;
}

/***********************************************************************
 *		wayland_wglDeleteContext
 */
static BOOL wayland_wglDeleteContext(struct wgl_context *ctx)
{
    wayland_mutex_lock(&gl_object_mutex);
    wl_list_remove(&ctx->link);
    wayland_mutex_unlock(&gl_object_mutex);
    p_eglDestroyContext(egl_display, ctx->context);
    free(ctx->attribs);
    free(ctx);
    return TRUE;
}

/***********************************************************************
 *		wayland_wglMakeContextCurrentARB
 */
static BOOL wayland_wglMakeContextCurrentARB(HDC draw_hdc, HDC read_hdc,
                                             struct wgl_context *ctx)
{
    BOOL ret = FALSE;

    TRACE("draw_hdc=%p read_hdc=%p ctx=%p\n", draw_hdc, read_hdc, ctx);

    if (!ctx)
    {
        p_eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        NtCurrentTeb()->glContext = NULL;
        return TRUE;
    }

    ret = wgl_context_make_current(ctx, NtUserWindowFromDC(draw_hdc), NtUserWindowFromDC(read_hdc));
    if (!ret) RtlSetLastWin32Error(ERROR_INVALID_HANDLE);

    return ret;
}

/***********************************************************************
 *		wayland_wglMakeCurrent
 */
static BOOL wayland_wglMakeCurrent(HDC hdc, struct wgl_context *ctx)
{
    return wayland_wglMakeContextCurrentARB(hdc, hdc, ctx);
}

/***********************************************************************
 *		wayland_wglDescribePixelFormat
 */
static int wayland_wglDescribePixelFormat(HDC hdc, int fmt, UINT size,
                                          PIXELFORMATDESCRIPTOR *pfd)
{
    EGLint val;
    EGLConfig config;

    if (!pfd) return nb_onscreen_formats;
    if (!is_onscreen_pixel_format(fmt)) return 0;
    if (size < sizeof(*pfd)) return 0;
    config = pixel_formats[fmt - 1].config;

    memset(pfd, 0, sizeof(*pfd));
    pfd->nSize = sizeof(*pfd);
    pfd->nVersion = 1;
    pfd->dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER |
                   PFD_SUPPORT_COMPOSITION;
    pfd->iPixelType = PFD_TYPE_RGBA;
    pfd->iLayerType = PFD_MAIN_PLANE;

    p_eglGetConfigAttrib(egl_display, config, EGL_BUFFER_SIZE, &val);
    pfd->cColorBits = val;
    p_eglGetConfigAttrib(egl_display, config, EGL_RED_SIZE, &val);
    pfd->cRedBits = val;
    p_eglGetConfigAttrib(egl_display, config, EGL_GREEN_SIZE, &val);
    pfd->cGreenBits = val;
    p_eglGetConfigAttrib(egl_display, config, EGL_BLUE_SIZE, &val);
    pfd->cBlueBits = val;
    p_eglGetConfigAttrib(egl_display, config, EGL_ALPHA_SIZE, &val);
    pfd->cAlphaBits = val;
    p_eglGetConfigAttrib(egl_display, config, EGL_DEPTH_SIZE, &val);
    pfd->cDepthBits = val;
    p_eglGetConfigAttrib(egl_display, config, EGL_STENCIL_SIZE, &val);
    pfd->cStencilBits = val;

    pfd->cAlphaShift = 0;
    pfd->cBlueShift = pfd->cAlphaShift + pfd->cAlphaBits;
    pfd->cGreenShift = pfd->cBlueShift + pfd->cBlueBits;
    pfd->cRedShift = pfd->cGreenShift + pfd->cGreenBits;

    TRACE("fmt %u color %u %u/%u/%u/%u depth %u stencil %u\n",
           fmt, pfd->cColorBits, pfd->cRedBits, pfd->cGreenBits, pfd->cBlueBits,
           pfd->cAlphaBits, pfd->cDepthBits, pfd->cStencilBits);
    return nb_onscreen_formats;
}

/***********************************************************************
 *		wayland_wglGetPixelFormat
 */
static int wayland_wglGetPixelFormat(HDC hdc)
{
    struct wayland_gl_drawable *gl;
    int ret = 0;

    if ((gl = wayland_gl_drawable_get(NtUserWindowFromDC(hdc))))
    {
        ret = gl->format;
        /* offscreen formats can't be used with traditional WGL calls */
        if (!is_onscreen_pixel_format(ret)) ret = 1;
        wayland_gl_drawable_release(gl);
    }
    return ret;
}

/***********************************************************************
 *		wayland_wglGetProcAddress
 */
static PROC wayland_wglGetProcAddress(LPCSTR name)
{
    PROC ret;
    if (!strncmp(name, "wgl", 3)) return NULL;
    ret = (PROC)p_eglGetProcAddress(name);
    TRACE("%s -> %p\n", name, ret);
    return ret;
}

/***********************************************************************
 *		wayland_wglSetPixelFormat
 */
static BOOL wayland_wglSetPixelFormat(HDC hdc, int format,
                                      const PIXELFORMATDESCRIPTOR *pfd)
{
    return set_pixel_format(hdc, format, FALSE);
}

/***********************************************************************
 *		wayland_wglSetPixelFormatWINE
 */
static BOOL wayland_wglSetPixelFormatWINE(HDC hdc, int format)
{
    return set_pixel_format(hdc, format, TRUE);
}

/***********************************************************************
 *		wayland_wglShareLists
 */
static BOOL wayland_wglShareLists(struct wgl_context *org,
                                  struct wgl_context *dest)
{
    TRACE("(%p, %p)\n", org, dest);

    /* Sharing of display lists works differently in EGL and WGL. In case of
     * EGL it is done at context creation time but in case of EGL it can also
     * be done using wglShareLists.
     *
     * We handle this by creating an EGL context in wglCreateContext /
     * wglCreateContextAttribsARB and when a program requests sharing we
     * recreate the destination context if it hasn't been made current and
     * it hasn't shared display lists before.
     */

    if (dest->has_been_current)
    {
        ERR("Could not share display lists, the hglrc2 context has been current already!\n");
        return FALSE;
    }
    else if (dest->sharing)
    {
        ERR("Could not share display lists because hglrc2 has already shared lists before!\n");
        return FALSE;
    }
    else
    {
        /* Re-create the EGL context and share display lists */
        p_eglDestroyContext(egl_display, dest->context);
        dest->context = p_eglCreateContext(egl_display, dest->config,
                                           org->context, dest->attribs);
        TRACE("re-created EGL context (%p) for WGL context %p (config: %p) "
              "sharing lists with EGL context %p for WGL context %p (config: %p)\n",
              dest->context, dest, dest->config, org->context, org, org->config);
        org->sharing = TRUE;
        dest->sharing = TRUE;
        return TRUE;
    }

    return FALSE;
}

/***********************************************************************
 *		wayland_wglSwapBuffers
 */
static BOOL wayland_wglSwapBuffers(HDC hdc)
{
    struct wgl_context *ctx = NtCurrentTeb()->glContext;
    HWND hwnd = NtUserWindowFromDC(hdc);
    struct wayland_gl_drawable *draw_gl = wayland_gl_drawable_get(hwnd);

    TRACE("hdc %p hwnd %p ctx %p\n", hdc, hwnd, ctx);

    if (draw_gl && wayland_gl_drawable_needs_update(draw_gl))
    {
        wayland_gl_drawable_update(draw_gl);
        goto out;
    }

    if ((!ctx || !wgl_context_refresh(ctx)) && draw_gl && draw_gl->surface)
    {
        struct wayland_gl_buffer *gl_buffer;
        struct gbm_bo *bo;

        wayland_gl_drawable_throttle(draw_gl);

        p_eglSwapBuffers(egl_display, draw_gl->surface);

        bo = gbm_surface_lock_front_buffer(draw_gl->gbm_surface);
        if (!bo)
        {
            ERR("Failed to lock front buffer\n");
            goto out;
        }
        gl_buffer = wayland_gl_drawable_track_buffer(draw_gl, bo);

        if (!wayland_gl_drawable_commit(draw_gl, gl_buffer))
            gbm_surface_release_buffer(gl_buffer->gbm_surface, gl_buffer->gbm_bo);

        /* Wait until we have a free buffer for the application to render into
         * before we continue. */
        if (draw_gl->wayland_surface)
        {
            while (!gbm_surface_has_free_buffers(draw_gl->gbm_surface) &&
                   wayland_dispatch_queue(draw_gl->wl_event_queue, -1) != -1)
            {
                continue;
            }
        }
    }

out:
    wayland_gl_drawable_release(draw_gl);

    return TRUE;
}

static void wayland_glFinish(void)
{
    struct wgl_context *ctx = NtCurrentTeb()->glContext;

    if (!ctx) return;
    TRACE("hwnd %p egl_context %p\n", ctx->draw_hwnd, ctx->context);
    wgl_context_refresh(ctx);
    p_glFinish();
}

static void wayland_glFlush(void)
{
    struct wgl_context *ctx = NtCurrentTeb()->glContext;

    if (!ctx) return;
    TRACE("hwnd %p egl_context %p\n", ctx->draw_hwnd, ctx->context);
    wgl_context_refresh(ctx);
    p_glFlush();
}

/***********************************************************************
 *		wayland_wglGetSwapIntervalEXT
 */
static int wayland_wglGetSwapIntervalEXT(void)
{
    struct wgl_context *ctx = NtCurrentTeb()->glContext;
    struct wayland_gl_drawable *gl;
    int swap_interval;

    if (!(gl = wayland_gl_drawable_get(ctx->draw_hwnd)))
    {
        /* This can't happen because a current WGL context is required to get
         * here. Likely the application is buggy.
         */
        WARN("No GL drawable found, returning swap interval 0\n");
        return 0;
    }

    swap_interval = gl->swap_interval;
    wayland_gl_drawable_release(gl);

    return swap_interval;
}

/***********************************************************************
 *		wayland_wglGetSwapIntervalEXT
 */
static BOOL wayland_wglSwapIntervalEXT(int interval)
{
    struct wgl_context *ctx = NtCurrentTeb()->glContext;
    struct wayland_gl_drawable *gl;

    TRACE("(%d)\n", interval);

    if (interval < 0)
    {
        RtlSetLastWin32Error(ERROR_INVALID_DATA);
        return FALSE;
    }

    if (!(gl = wayland_gl_drawable_get(ctx->draw_hwnd)))
    {
        RtlSetLastWin32Error(ERROR_DC_NOT_FOUND);
        return FALSE;
    }

    gl->swap_interval = interval;

    wayland_gl_drawable_release(gl);

    return TRUE;
}

/***********************************************************************
 *		wayland_wglGetExtensionsStringARB
 */
static const char *wayland_wglGetExtensionsStringARB(HDC hdc)
{
    TRACE("() returning \"%s\"\n", wgl_extensions);
    return wgl_extensions;
}

/***********************************************************************
 *		wayland_wglGetExtensionsStringEXT
 */
static const char *wayland_wglGetExtensionsStringEXT(void)
{
    TRACE("() returning \"%s\"\n", wgl_extensions);
    return wgl_extensions;
}

static void register_extension(const char *ext)
{
    if (wgl_extensions[0]) strcat(wgl_extensions, " ");
    strcat(wgl_extensions, ext);
    TRACE("%s\n", ext);
}

static BOOL has_extension(const char *list, const char *ext)
{
    size_t len = strlen(ext);
    const char *cur = list;

    if (!cur) return FALSE;

    while ((cur = strstr(cur, ext)))
    {
        if ((!cur[len] || cur[len] == ' ') && (cur == list || cur[-1] == ' '))
            return TRUE;
    }

    return FALSE;
}

static void init_extensions(int major, int minor)
{
    void *ptr;
    const char *egl_exts = p_eglQueryString(egl_display, EGL_EXTENSIONS);

    register_extension("WGL_ARB_extensions_string");
    egl_funcs.ext.p_wglGetExtensionsStringARB = wayland_wglGetExtensionsStringARB;

    register_extension("WGL_EXT_extensions_string");
    egl_funcs.ext.p_wglGetExtensionsStringEXT = wayland_wglGetExtensionsStringEXT;

    /* In WineD3D we need the ability to set the pixel format more than once
     * (e.g. after a device reset).  The default wglSetPixelFormat doesn't
     * allow this, so add our own which allows it.
     */
    register_extension("WGL_WINE_pixel_format_passthrough");
    egl_funcs.ext.p_wglSetPixelFormatWINE = wayland_wglSetPixelFormatWINE;

    register_extension("WGL_ARB_make_current_read");
    egl_funcs.ext.p_wglGetCurrentReadDCARB   = (void *)1;  /* never called */
    egl_funcs.ext.p_wglMakeContextCurrentARB = wayland_wglMakeContextCurrentARB;

    register_extension("WGL_ARB_create_context");
    register_extension("WGL_ARB_create_context_profile");
    egl_funcs.ext.p_wglCreateContextAttribsARB = wayland_wglCreateContextAttribsARB;

    if (has_extension(egl_exts, "EGL_KHR_create_context"))
        has_khr_create_context = TRUE;

    register_extension("WGL_EXT_swap_control");
    egl_funcs.ext.p_wglSwapIntervalEXT = wayland_wglSwapIntervalEXT;
    egl_funcs.ext.p_wglGetSwapIntervalEXT = wayland_wglGetSwapIntervalEXT;

    if ((major == 1 && minor >= 5) || has_extension(egl_exts, "EGL_KHR_gl_colorspace"))
    {
        register_extension("WGL_EXT_framebuffer_sRGB");
        has_gl_colorspace = TRUE;
    }

    /* load standard functions and extensions exported from the OpenGL library */

#define USE_GL_FUNC(func) if ((ptr = dlsym(opengl_handle, #func))) egl_funcs.gl.p_##func = ptr;
    ALL_WGL_FUNCS
#undef USE_GL_FUNC

#define LOAD_FUNCPTR(func) egl_funcs.ext.p_##func = dlsym(opengl_handle, #func)
    LOAD_FUNCPTR(glActiveShaderProgram);
    LOAD_FUNCPTR(glActiveTexture);
    LOAD_FUNCPTR(glAttachShader);
    LOAD_FUNCPTR(glBeginQuery);
    LOAD_FUNCPTR(glBeginTransformFeedback);
    LOAD_FUNCPTR(glBindAttribLocation);
    LOAD_FUNCPTR(glBindBuffer);
    LOAD_FUNCPTR(glBindBufferBase);
    LOAD_FUNCPTR(glBindBufferRange);
    LOAD_FUNCPTR(glBindFramebuffer);
    LOAD_FUNCPTR(glBindImageTexture);
    LOAD_FUNCPTR(glBindProgramPipeline);
    LOAD_FUNCPTR(glBindRenderbuffer);
    LOAD_FUNCPTR(glBindSampler);
    LOAD_FUNCPTR(glBindTransformFeedback);
    LOAD_FUNCPTR(glBindVertexArray);
    LOAD_FUNCPTR(glBindVertexBuffer);
    LOAD_FUNCPTR(glBlendBarrierKHR);
    LOAD_FUNCPTR(glBlendColor);
    LOAD_FUNCPTR(glBlendEquation);
    LOAD_FUNCPTR(glBlendEquationSeparate);
    LOAD_FUNCPTR(glBlendFuncSeparate);
    LOAD_FUNCPTR(glBlitFramebuffer);
    LOAD_FUNCPTR(glBufferData);
    LOAD_FUNCPTR(glBufferSubData);
    LOAD_FUNCPTR(glCheckFramebufferStatus);
    LOAD_FUNCPTR(glClearBufferfi);
    LOAD_FUNCPTR(glClearBufferfv);
    LOAD_FUNCPTR(glClearBufferiv);
    LOAD_FUNCPTR(glClearBufferuiv);
    LOAD_FUNCPTR(glClearDepthf);
    LOAD_FUNCPTR(glClientWaitSync);
    LOAD_FUNCPTR(glCompileShader);
    LOAD_FUNCPTR(glCompressedTexImage2D);
    LOAD_FUNCPTR(glCompressedTexImage3D);
    LOAD_FUNCPTR(glCompressedTexSubImage2D);
    LOAD_FUNCPTR(glCompressedTexSubImage3D);
    LOAD_FUNCPTR(glCopyBufferSubData);
    LOAD_FUNCPTR(glCopyTexSubImage3D);
    LOAD_FUNCPTR(glCreateProgram);
    LOAD_FUNCPTR(glCreateShader);
    LOAD_FUNCPTR(glCreateShaderProgramv);
    LOAD_FUNCPTR(glDeleteBuffers);
    LOAD_FUNCPTR(glDeleteFramebuffers);
    LOAD_FUNCPTR(glDeleteProgram);
    LOAD_FUNCPTR(glDeleteProgramPipelines);
    LOAD_FUNCPTR(glDeleteQueries);
    LOAD_FUNCPTR(glDeleteRenderbuffers);
    LOAD_FUNCPTR(glDeleteSamplers);
    LOAD_FUNCPTR(glDeleteShader);
    LOAD_FUNCPTR(glDeleteSync);
    LOAD_FUNCPTR(glDeleteTransformFeedbacks);
    LOAD_FUNCPTR(glDeleteVertexArrays);
    LOAD_FUNCPTR(glDepthRangef);
    LOAD_FUNCPTR(glDetachShader);
    LOAD_FUNCPTR(glDisableVertexAttribArray);
    LOAD_FUNCPTR(glDispatchCompute);
    LOAD_FUNCPTR(glDispatchComputeIndirect);
    LOAD_FUNCPTR(glDrawArraysIndirect);
    LOAD_FUNCPTR(glDrawArraysInstanced);
    LOAD_FUNCPTR(glDrawBuffers);
    LOAD_FUNCPTR(glDrawElementsIndirect);
    LOAD_FUNCPTR(glDrawElementsInstanced);
    LOAD_FUNCPTR(glDrawRangeElements);
    LOAD_FUNCPTR(glEnableVertexAttribArray);
    LOAD_FUNCPTR(glEndQuery);
    LOAD_FUNCPTR(glEndTransformFeedback);
    LOAD_FUNCPTR(glFenceSync);
    LOAD_FUNCPTR(glFlushMappedBufferRange);
    LOAD_FUNCPTR(glFramebufferParameteri);
    LOAD_FUNCPTR(glFramebufferRenderbuffer);
    LOAD_FUNCPTR(glFramebufferTexture2D);
    LOAD_FUNCPTR(glFramebufferTextureEXT);
    LOAD_FUNCPTR(glFramebufferTextureLayer);
    LOAD_FUNCPTR(glGenBuffers);
    LOAD_FUNCPTR(glGenFramebuffers);
    LOAD_FUNCPTR(glGenProgramPipelines);
    LOAD_FUNCPTR(glGenQueries);
    LOAD_FUNCPTR(glGenRenderbuffers);
    LOAD_FUNCPTR(glGenSamplers);
    LOAD_FUNCPTR(glGenTransformFeedbacks);
    LOAD_FUNCPTR(glGenVertexArrays);
    LOAD_FUNCPTR(glGenerateMipmap);
    LOAD_FUNCPTR(glGetActiveAttrib);
    LOAD_FUNCPTR(glGetActiveUniform);
    LOAD_FUNCPTR(glGetActiveUniformBlockName);
    LOAD_FUNCPTR(glGetActiveUniformBlockiv);
    LOAD_FUNCPTR(glGetActiveUniformsiv);
    LOAD_FUNCPTR(glGetAttachedShaders);
    LOAD_FUNCPTR(glGetAttribLocation);
    LOAD_FUNCPTR(glGetBooleani_v);
    LOAD_FUNCPTR(glGetBufferParameteri64v);
    LOAD_FUNCPTR(glGetBufferParameteriv);
    LOAD_FUNCPTR(glGetBufferPointerv);
    LOAD_FUNCPTR(glGetFragDataLocation);
    LOAD_FUNCPTR(glGetFramebufferAttachmentParameteriv);
    LOAD_FUNCPTR(glGetFramebufferParameteriv);
    LOAD_FUNCPTR(glGetInteger64i_v);
    LOAD_FUNCPTR(glGetInteger64v);
    LOAD_FUNCPTR(glGetIntegeri_v);
    LOAD_FUNCPTR(glGetInternalformativ);
    LOAD_FUNCPTR(glGetMultisamplefv);
    LOAD_FUNCPTR(glGetProgramBinary);
    LOAD_FUNCPTR(glGetProgramInfoLog);
    LOAD_FUNCPTR(glGetProgramInterfaceiv);
    LOAD_FUNCPTR(glGetProgramPipelineInfoLog);
    LOAD_FUNCPTR(glGetProgramPipelineiv);
    LOAD_FUNCPTR(glGetProgramResourceIndex);
    LOAD_FUNCPTR(glGetProgramResourceLocation);
    LOAD_FUNCPTR(glGetProgramResourceName);
    LOAD_FUNCPTR(glGetProgramResourceiv);
    LOAD_FUNCPTR(glGetProgramiv);
    LOAD_FUNCPTR(glGetQueryObjectuiv);
    LOAD_FUNCPTR(glGetQueryiv);
    LOAD_FUNCPTR(glGetRenderbufferParameteriv);
    LOAD_FUNCPTR(glGetSamplerParameterfv);
    LOAD_FUNCPTR(glGetSamplerParameteriv);
    LOAD_FUNCPTR(glGetShaderInfoLog);
    LOAD_FUNCPTR(glGetShaderPrecisionFormat);
    LOAD_FUNCPTR(glGetShaderSource);
    LOAD_FUNCPTR(glGetShaderiv);
    LOAD_FUNCPTR(glGetStringi);
    LOAD_FUNCPTR(glGetSynciv);
    LOAD_FUNCPTR(glGetTexParameterIivEXT);
    LOAD_FUNCPTR(glGetTexParameterIuivEXT);
    LOAD_FUNCPTR(glGetTransformFeedbackVarying);
    LOAD_FUNCPTR(glGetUniformBlockIndex);
    LOAD_FUNCPTR(glGetUniformIndices);
    LOAD_FUNCPTR(glGetUniformLocation);
    LOAD_FUNCPTR(glGetUniformfv);
    LOAD_FUNCPTR(glGetUniformiv);
    LOAD_FUNCPTR(glGetUniformuiv);
    LOAD_FUNCPTR(glGetVertexAttribIiv);
    LOAD_FUNCPTR(glGetVertexAttribIuiv);
    LOAD_FUNCPTR(glGetVertexAttribPointerv);
    LOAD_FUNCPTR(glGetVertexAttribfv);
    LOAD_FUNCPTR(glGetVertexAttribiv);
    LOAD_FUNCPTR(glInvalidateFramebuffer);
    LOAD_FUNCPTR(glInvalidateSubFramebuffer);
    LOAD_FUNCPTR(glIsBuffer);
    LOAD_FUNCPTR(glIsFramebuffer);
    LOAD_FUNCPTR(glIsProgram);
    LOAD_FUNCPTR(glIsProgramPipeline);
    LOAD_FUNCPTR(glIsQuery);
    LOAD_FUNCPTR(glIsRenderbuffer);
    LOAD_FUNCPTR(glIsSampler);
    LOAD_FUNCPTR(glIsShader);
    LOAD_FUNCPTR(glIsSync);
    LOAD_FUNCPTR(glIsTransformFeedback);
    LOAD_FUNCPTR(glIsVertexArray);
    LOAD_FUNCPTR(glLinkProgram);
    LOAD_FUNCPTR(glMapBufferRange);
    LOAD_FUNCPTR(glMemoryBarrier);
    LOAD_FUNCPTR(glMemoryBarrierByRegion);
    LOAD_FUNCPTR(glPauseTransformFeedback);
    LOAD_FUNCPTR(glProgramBinary);
    LOAD_FUNCPTR(glProgramParameteri);
    LOAD_FUNCPTR(glProgramUniform1f);
    LOAD_FUNCPTR(glProgramUniform1fv);
    LOAD_FUNCPTR(glProgramUniform1i);
    LOAD_FUNCPTR(glProgramUniform1iv);
    LOAD_FUNCPTR(glProgramUniform1ui);
    LOAD_FUNCPTR(glProgramUniform1uiv);
    LOAD_FUNCPTR(glProgramUniform2f);
    LOAD_FUNCPTR(glProgramUniform2fv);
    LOAD_FUNCPTR(glProgramUniform2i);
    LOAD_FUNCPTR(glProgramUniform2iv);
    LOAD_FUNCPTR(glProgramUniform2ui);
    LOAD_FUNCPTR(glProgramUniform2uiv);
    LOAD_FUNCPTR(glProgramUniform3f);
    LOAD_FUNCPTR(glProgramUniform3fv);
    LOAD_FUNCPTR(glProgramUniform3i);
    LOAD_FUNCPTR(glProgramUniform3iv);
    LOAD_FUNCPTR(glProgramUniform3ui);
    LOAD_FUNCPTR(glProgramUniform3uiv);
    LOAD_FUNCPTR(glProgramUniform4f);
    LOAD_FUNCPTR(glProgramUniform4fv);
    LOAD_FUNCPTR(glProgramUniform4i);
    LOAD_FUNCPTR(glProgramUniform4iv);
    LOAD_FUNCPTR(glProgramUniform4ui);
    LOAD_FUNCPTR(glProgramUniform4uiv);
    LOAD_FUNCPTR(glProgramUniformMatrix2fv);
    LOAD_FUNCPTR(glProgramUniformMatrix2x3fv);
    LOAD_FUNCPTR(glProgramUniformMatrix2x4fv);
    LOAD_FUNCPTR(glProgramUniformMatrix3fv);
    LOAD_FUNCPTR(glProgramUniformMatrix3x2fv);
    LOAD_FUNCPTR(glProgramUniformMatrix3x4fv);
    LOAD_FUNCPTR(glProgramUniformMatrix4fv);
    LOAD_FUNCPTR(glProgramUniformMatrix4x2fv);
    LOAD_FUNCPTR(glProgramUniformMatrix4x3fv);
    LOAD_FUNCPTR(glReleaseShaderCompiler);
    LOAD_FUNCPTR(glRenderbufferStorage);
    LOAD_FUNCPTR(glRenderbufferStorageMultisample);
    LOAD_FUNCPTR(glResumeTransformFeedback);
    LOAD_FUNCPTR(glSampleCoverage);
    LOAD_FUNCPTR(glSampleMaski);
    LOAD_FUNCPTR(glSamplerParameterf);
    LOAD_FUNCPTR(glSamplerParameterfv);
    LOAD_FUNCPTR(glSamplerParameteri);
    LOAD_FUNCPTR(glSamplerParameteriv);
    LOAD_FUNCPTR(glShaderBinary);
    LOAD_FUNCPTR(glShaderSource);
    LOAD_FUNCPTR(glStencilFuncSeparate);
    LOAD_FUNCPTR(glStencilMaskSeparate);
    LOAD_FUNCPTR(glStencilOpSeparate);
    LOAD_FUNCPTR(glTexBufferEXT);
    LOAD_FUNCPTR(glTexImage3D);
    LOAD_FUNCPTR(glTexParameterIivEXT);
    LOAD_FUNCPTR(glTexParameterIuivEXT);
    LOAD_FUNCPTR(glTexStorage2D);
    LOAD_FUNCPTR(glTexStorage2DMultisample);
    LOAD_FUNCPTR(glTexStorage3D);
    LOAD_FUNCPTR(glTexSubImage3D);
    LOAD_FUNCPTR(glTransformFeedbackVaryings);
    LOAD_FUNCPTR(glUniform1f);
    LOAD_FUNCPTR(glUniform1fv);
    LOAD_FUNCPTR(glUniform1i);
    LOAD_FUNCPTR(glUniform1iv);
    LOAD_FUNCPTR(glUniform1ui);
    LOAD_FUNCPTR(glUniform1uiv);
    LOAD_FUNCPTR(glUniform2f);
    LOAD_FUNCPTR(glUniform2fv);
    LOAD_FUNCPTR(glUniform2i);
    LOAD_FUNCPTR(glUniform2iv);
    LOAD_FUNCPTR(glUniform2ui);
    LOAD_FUNCPTR(glUniform2uiv);
    LOAD_FUNCPTR(glUniform3f);
    LOAD_FUNCPTR(glUniform3fv);
    LOAD_FUNCPTR(glUniform3i);
    LOAD_FUNCPTR(glUniform3iv);
    LOAD_FUNCPTR(glUniform3ui);
    LOAD_FUNCPTR(glUniform3uiv);
    LOAD_FUNCPTR(glUniform4f);
    LOAD_FUNCPTR(glUniform4fv);
    LOAD_FUNCPTR(glUniform4i);
    LOAD_FUNCPTR(glUniform4iv);
    LOAD_FUNCPTR(glUniform4ui);
    LOAD_FUNCPTR(glUniform4uiv);
    LOAD_FUNCPTR(glUniformBlockBinding);
    LOAD_FUNCPTR(glUniformMatrix2fv);
    LOAD_FUNCPTR(glUniformMatrix2x3fv);
    LOAD_FUNCPTR(glUniformMatrix2x4fv);
    LOAD_FUNCPTR(glUniformMatrix3fv);
    LOAD_FUNCPTR(glUniformMatrix3x2fv);
    LOAD_FUNCPTR(glUniformMatrix3x4fv);
    LOAD_FUNCPTR(glUniformMatrix4fv);
    LOAD_FUNCPTR(glUniformMatrix4x2fv);
    LOAD_FUNCPTR(glUniformMatrix4x3fv);
    LOAD_FUNCPTR(glUnmapBuffer);
    LOAD_FUNCPTR(glUseProgram);
    LOAD_FUNCPTR(glUseProgramStages);
    LOAD_FUNCPTR(glValidateProgram);
    LOAD_FUNCPTR(glValidateProgramPipeline);
    LOAD_FUNCPTR(glVertexAttrib1f);
    LOAD_FUNCPTR(glVertexAttrib1fv);
    LOAD_FUNCPTR(glVertexAttrib2f);
    LOAD_FUNCPTR(glVertexAttrib2fv);
    LOAD_FUNCPTR(glVertexAttrib3f);
    LOAD_FUNCPTR(glVertexAttrib3fv);
    LOAD_FUNCPTR(glVertexAttrib4f);
    LOAD_FUNCPTR(glVertexAttrib4fv);
    LOAD_FUNCPTR(glVertexAttribBinding);
    LOAD_FUNCPTR(glVertexAttribDivisor);
    LOAD_FUNCPTR(glVertexAttribFormat);
    LOAD_FUNCPTR(glVertexAttribI4i);
    LOAD_FUNCPTR(glVertexAttribI4iv);
    LOAD_FUNCPTR(glVertexAttribI4ui);
    LOAD_FUNCPTR(glVertexAttribI4uiv);
    LOAD_FUNCPTR(glVertexAttribIFormat);
    LOAD_FUNCPTR(glVertexAttribIPointer);
    LOAD_FUNCPTR(glVertexAttribPointer);
    LOAD_FUNCPTR(glVertexBindingDivisor);
    LOAD_FUNCPTR(glWaitSync);
#undef LOAD_FUNCPTR

    /* Redirect some standard OpenGL functions. */

#define REDIRECT(func) \
    do { p_##func = egl_funcs.gl.p_##func; egl_funcs.gl.p_##func = wayland_##func; } while(0)
    REDIRECT(glFinish);
    REDIRECT(glFlush);
#undef REDIRECT
}

static BOOL init_pixel_formats(void)
{
    EGLint count, i, pass;
    EGLConfig *egl_configs = NULL;
    struct wayland_dmabuf *dmabuf = NULL;
    dev_t render_dev;

    p_eglGetConfigs(egl_display, NULL, 0, &count);
    if (!count)
    {
        ERR("eglGetConfigs returned no configs.\n");
        goto err;
    }

    if (!(egl_configs = malloc(count * sizeof(*egl_configs))) ||
        !(pixel_formats = malloc(count * sizeof(*pixel_formats))))
    {
        ERR("Memory allocation failed.\n");
        goto err;
    }
    p_eglGetConfigs(egl_display, egl_configs, count, &count);

    if (!(render_dev = wayland_gbm_get_render_dev()))
    {
        ERR("Failed to get device's dev_t from GBM device.\n");
        goto err;
    }

    dmabuf = &wayland_process_acquire()->dmabuf;
    /* Use two passes: the first pass adds the onscreen formats to the format list,
     * the second offscreen ones. */
    for (pass = 0; pass < 2; pass++)
    {
        for (i = 0; i < count; i++)
        {
            EGLint id, type, visual_id, native, render, color, r, g, b, d, s;

            p_eglGetConfigAttrib(egl_display, egl_configs[i], EGL_SURFACE_TYPE, &type);
            if (!(type & EGL_WINDOW_BIT) == !pass) continue;

            p_eglGetConfigAttrib(egl_display, egl_configs[i], EGL_NATIVE_VISUAL_ID, &visual_id);

            /* Ignore formats not supported by the compositor. */
            if (!wayland_dmabuf_is_format_supported(dmabuf, visual_id, render_dev))
                continue;

            p_eglGetConfigAttrib(egl_display, egl_configs[i], EGL_RENDERABLE_TYPE, &render);
            p_eglGetConfigAttrib(egl_display, egl_configs[i], EGL_CONFIG_ID, &id);
            p_eglGetConfigAttrib(egl_display, egl_configs[i], EGL_NATIVE_RENDERABLE, &native);
            p_eglGetConfigAttrib(egl_display, egl_configs[i], EGL_COLOR_BUFFER_TYPE, &color);
            p_eglGetConfigAttrib(egl_display, egl_configs[i], EGL_RED_SIZE, &r);
            p_eglGetConfigAttrib(egl_display, egl_configs[i], EGL_GREEN_SIZE, &g);
            p_eglGetConfigAttrib(egl_display, egl_configs[i], EGL_BLUE_SIZE, &b);
            p_eglGetConfigAttrib(egl_display, egl_configs[i], EGL_DEPTH_SIZE, &d);
            p_eglGetConfigAttrib(egl_display, egl_configs[i], EGL_STENCIL_SIZE, &s);

            /* Some drivers expose 10 bit components which are not typically what
             * applications want. */
            if (r > 8 || g > 8 || b > 8) continue;

            pixel_formats[nb_pixel_formats].config = egl_configs[i];
            pixel_formats[nb_pixel_formats].native_visual_id = visual_id;
            nb_pixel_formats++;
            TRACE("%u: config %u id %u type %x visual %u native %u render %x "
                  "colortype %u rgb %u,%u,%u depth %u stencil %u\n",
                   nb_pixel_formats, i, id, type, visual_id, native, render,
                   color, r, g, b, d, s);
        }
        if (pass == 0) nb_onscreen_formats = nb_pixel_formats;
    }
    wayland_process_release();
    free(egl_configs);

    return TRUE;

err:
    free(egl_configs);
    free(pixel_formats);

    return FALSE;
}

static BOOL egl_init(void)
{
    static int retval = -1;

    if (retval != -1) return retval;
    retval = 0;

    if (!(egl_handle = dlopen(SONAME_LIBEGL, RTLD_NOW|RTLD_GLOBAL)))
    {
        ERR("failed to load %s: %s\n", SONAME_LIBEGL, dlerror());
        return FALSE;
    }

#ifdef SONAME_LIBGL
    if (!(opengl_handle = dlopen(SONAME_LIBGL, RTLD_NOW|RTLD_GLOBAL)))
        WARN("failed to load %s: %s\n", SONAME_LIBGL, dlerror());
#endif

#ifdef SONAME_LIBGLESV2
    if (!opengl_handle && (!(opengl_handle = dlopen(SONAME_LIBGLESV2, RTLD_NOW|RTLD_GLOBAL))))
        WARN("failed to load %s: %s\n", SONAME_LIBGLESV2, dlerror());
#endif

    if (!opengl_handle)
    {
        ERR("failed to load GL or GLESv2 library\n");
        return FALSE;
    }

#define LOAD_FUNCPTR(func) do { \
        if (!(p_##func = dlsym(egl_handle, #func))) \
        { ERR("can't find symbol %s\n", #func); return FALSE; }    \
    } while(0)
    LOAD_FUNCPTR(eglBindAPI);
    LOAD_FUNCPTR(eglCreateContext);
    LOAD_FUNCPTR(eglCreateWindowSurface);
    LOAD_FUNCPTR(eglDestroyContext);
    LOAD_FUNCPTR(eglDestroySurface);
    LOAD_FUNCPTR(eglGetConfigAttrib);
    LOAD_FUNCPTR(eglGetConfigs);
    LOAD_FUNCPTR(eglGetDisplay);
    LOAD_FUNCPTR(eglGetProcAddress);
    LOAD_FUNCPTR(eglInitialize);
    LOAD_FUNCPTR(eglMakeCurrent);
    LOAD_FUNCPTR(eglQueryString);
    LOAD_FUNCPTR(eglSwapBuffers);
#undef LOAD_FUNCPTR

    if (!wayland_gbm_init()) return FALSE;

    egl_display = p_eglGetDisplay((EGLNativeDisplayType) process_gbm_device);
    if (!p_eglInitialize(egl_display, &egl_version[0], &egl_version[1]))
        return FALSE;
    TRACE("display %p version %u.%u\n", egl_display, egl_version[0], egl_version[1]);

    if (!init_pixel_formats()) return FALSE;

    init_extensions(egl_version[0], egl_version[1]);
    retval = 1;
    return TRUE;
}

/* generate stubs for GL functions that are not exported */

#define USE_GL_FUNC(name) \
static void glstub_##name(void) \
{ \
    ERR(#name " called\n"); \
    assert(0); \
    ExitProcess(1); \
}

ALL_WGL_FUNCS
#undef USE_GL_FUNC

static struct opengl_funcs egl_funcs =
{
    .wgl =
    {
        .p_wglCopyContext = wayland_wglCopyContext,
        .p_wglCreateContext = wayland_wglCreateContext,
        .p_wglDeleteContext = wayland_wglDeleteContext,
        .p_wglDescribePixelFormat = wayland_wglDescribePixelFormat,
        .p_wglGetPixelFormat = wayland_wglGetPixelFormat,
        .p_wglGetProcAddress = wayland_wglGetProcAddress,
        .p_wglMakeCurrent = wayland_wglMakeCurrent,
        .p_wglSetPixelFormat = wayland_wglSetPixelFormat,
        .p_wglShareLists = wayland_wglShareLists,
        .p_wglSwapBuffers = wayland_wglSwapBuffers,
    },
#define USE_GL_FUNC(name) (void *)glstub_##name,
    .gl = { ALL_WGL_FUNCS }
#undef USE_GL_FUNC
};

/**********************************************************************
 *           WAYLAND_wine_get_wgl_driver
 */
struct opengl_funcs *WAYLAND_wine_get_wgl_driver(UINT version)
{
    if (version != WINE_WGL_DRIVER_VERSION)
    {
        ERR("version mismatch, opengl32 wants %u but driver has %u\n",
            version, WINE_WGL_DRIVER_VERSION);
        return NULL;
    }
    if (!egl_init()) return NULL;
    return &egl_funcs;
}

/***********************************************************************
 *		wayland_update_gl_drawable_surface
 */
void wayland_update_gl_drawable_surface(HWND hwnd, struct wayland_surface *wayland_surface)
{
    struct wayland_gl_drawable *gl;

    if ((gl = wayland_gl_drawable_get(hwnd)))
    {
        if (gl->wayland_surface)
            wayland_surface_unref_glvk(gl->wayland_surface);

        gl->wayland_surface = wayland_surface;
        if (gl->wayland_surface)
            wayland_surface_create_or_ref_glvk(gl->wayland_surface);

        wayland_gl_drawable_release(gl);
    }
}

#else /* No GL */

struct opengl_funcs *WAYLAND_wine_get_wgl_driver(UINT version)
{
    ERR("Wine Wayland was built without OpenGL support.\n");
    return NULL;
}

void wayland_update_gl_drawable_surface(HWND hwnd, struct wayland_surface *wayland_surface)
{
}

#endif
