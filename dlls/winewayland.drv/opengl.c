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

#include <EGL/egl.h>
#include <assert.h>
#include <dlfcn.h>

static void *egl_handle;
static void *opengl_handle;
static EGLDisplay egl_display;
static EGLint egl_version[2];
static struct opengl_funcs egl_funcs;
static char wgl_extensions[4096];

#define DECL_FUNCPTR(f) static __typeof__(f) * p_##f = NULL
DECL_FUNCPTR(eglGetDisplay);
DECL_FUNCPTR(eglGetProcAddress);
DECL_FUNCPTR(eglInitialize);
#undef DECL_FUNCPTR

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
    LOAD_FUNCPTR(eglGetDisplay);
    LOAD_FUNCPTR(eglGetProcAddress);
    LOAD_FUNCPTR(eglInitialize);
#undef LOAD_FUNCPTR

    if (!wayland_gbm_init()) return FALSE;

    egl_display = p_eglGetDisplay((EGLNativeDisplayType) process_gbm_device);
    if (!p_eglInitialize(egl_display, &egl_version[0], &egl_version[1]))
        return FALSE;
    TRACE("display %p version %u.%u\n", egl_display, egl_version[0], egl_version[1]);

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
        .p_wglGetProcAddress = wayland_wglGetProcAddress,
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
