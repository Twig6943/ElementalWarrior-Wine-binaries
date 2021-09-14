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
};

struct wgl_context
{
    struct wl_list link;
    EGLConfig  config;
    EGLContext context;
    HWND       draw_hwnd;
    HWND       read_hwnd;
};

static void *egl_handle;
static void *opengl_handle;
static EGLDisplay egl_display;
static EGLint egl_version[2];
static struct opengl_funcs egl_funcs;
static char wgl_extensions[4096];
static struct wgl_pixel_format *pixel_formats;
static int nb_pixel_formats, nb_onscreen_formats;

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
#undef DECL_FUNCPTR

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

    wayland_mutex_lock(&gl_object_mutex);
    wl_list_insert(&gl_drawables, &gl->link);
    return gl;

err:
    if (gl)
    {
        if (gl->wayland_surface) wayland_surface_unref_glvk(gl->wayland_surface);
        free(gl);
    }
    return NULL;
}

static void wayland_destroy_gl_drawable(HWND hwnd)
{
    struct wayland_gl_drawable *gl;

    wayland_mutex_lock(&gl_object_mutex);
    wl_list_for_each(gl, &gl_drawables, link)
    {
        if (gl->hwnd != hwnd) continue;
        wl_list_remove(&gl->link);
        if (gl->surface) p_eglDestroySurface(egl_display, gl->surface);
        if (gl->gbm_surface) gbm_surface_destroy(gl->gbm_surface);
        if (gl->wayland_surface)
            wayland_surface_unref_glvk(gl->wayland_surface);
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

    gl->surface = p_eglCreateWindowSurface(egl_display, pixel_formats[gl->format - 1].config,
                                           (EGLNativeWindowType) gl->gbm_surface, NULL);
    if (!gl->surface)
        ERR("Failed to create EGL surface\n");

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
        }
    }

    TRACE("hwnd=%p gbm_surface=%p egl_surface=%p\n",
          gl->hwnd, gl->gbm_surface, gl->surface);

    NtUserRedrawWindow(gl->hwnd, NULL, 0, RDW_INVALIDATE | RDW_ERASE);
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

static struct wgl_context *create_context(HDC hdc)
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
    ctx->context = p_eglCreateContext(egl_display, ctx->config,
                                      EGL_NO_CONTEXT,
                                      NULL);
    ctx->draw_hwnd = 0;
    ctx->read_hwnd = 0;

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

    return create_context(hdc);
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

static void init_extensions(void)
{
    void *ptr;

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
#undef LOAD_FUNCPTR

    if (!wayland_gbm_init()) return FALSE;

    egl_display = p_eglGetDisplay((EGLNativeDisplayType) process_gbm_device);
    if (!p_eglInitialize(egl_display, &egl_version[0], &egl_version[1]))
        return FALSE;
    TRACE("display %p version %u.%u\n", egl_display, egl_version[0], egl_version[1]);

    if (!init_pixel_formats()) return FALSE;

    init_extensions();
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
        .p_wglGetProcAddress = wayland_wglGetProcAddress,
        .p_wglMakeCurrent = wayland_wglMakeCurrent,
        .p_wglSetPixelFormat = wayland_wglSetPixelFormat,
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

#else /* No GL */

struct opengl_funcs *WAYLAND_wine_get_wgl_driver(UINT version)
{
    ERR("Wine Wayland was built without OpenGL support.\n");
    return NULL;
}

#endif
