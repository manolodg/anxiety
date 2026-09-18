#ifdef ANXIETY_BACKEND_OPENGL

#include "GLSwapchain.h"
#include "Logger.h"

// Includes específicos de plataforma para el intercambio de buffers
#if defined(_WIN32)
#  include <windows.h>
// wglSwapBuffers se declara en wingdi.h (incluido transitivamente por windows.h)
#elif defined(__APPLE__)
#  import <AppKit/AppKit.h>
#elif defined(__linux__) && !defined(__ANDROID__)
// GLX se carga dinámicamente para evitar una dependencia dura en tiempo de enlazado.
// En plataformas sin GLX (p. ej. solo Wayland), present() recurre a glFlush().
#  include <dlfcn.h>
#  include <cstdio>
#  include <cstdint>
#endif

namespace anxiety::rendering::backend::opengl {
    // Constructor --------------------------------------------------------------------------------
    GLSwapchain::GLSwapchain(const rhi::SwapchainDesc& desc) : m_extent(desc.extent), m_format(desc.format), m_vsync(desc.vsync), m_native_window(desc.native_window_handle) {
        // Handle sentinela: id == 1 se corresponde con el framebuffer 0 por defecto de GL. El pool
        // de recursos del device empieza en id == 2, así que 1 queda reservado permanentemente.
        m_backbuffer = rhi::TextureHandle{ 1 };

#if defined(_WIN32)
        if (m_native_window) {
            m_hdc = static_cast<void*>(::GetDC(static_cast<HWND>(m_native_window)));
            if (!m_hdc) LOG_WARNING("GLSwapchain", "GetDC devolvió null — puede que present() no intercambie los buffers correctamente.");
        }

        // Respeta el vsync mediante wglSwapIntervalEXT si está disponible
        using PFNWGLSWAPINTERVALEXTPROC = BOOL(WINAPI*)(int);
        auto wglSwapIntervalEXT_fn = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(wglGetProcAddress("wglSwapIntervalEXT"));
        if (wglSwapIntervalEXT_fn) wglSwapIntervalEXT_fn(m_vsync ? 1 : 0);
#elif defined(__APPLE__)
        // Respeta el vsync en el contexto que GLDevice activó para esta ventana.
        @autoreleasepool{
            if (NSOpenGLContext* ctx = [NSOpenGLContext currentContext]) {
                GLint swap_interval = m_vsync ? 1 : 0;
                [ctx setValues : &swap_interval forParameter : NSOpenGLContextParameterSwapInterval] ;
            }
        }
#endif

        LOGF_INFO("GLSwapchain", "GLSwapchain creado {}x{} vsync={}.", desc.extent.width, desc.extent.height, m_vsync);
    }

    // present ------------------------------------------------------------------------------------
    void GLSwapchain::present() {
#if defined(_WIN32)
        if (m_hdc) {
            ::SwapBuffers(static_cast<HDC>(m_hdc));
        } else {
            glFlush();
        }

#elif defined(__linux__) && !defined(__ANDROID__)
        // Intenta llamar a glXSwapBuffers mediante dlsym para no tener una dependencia dura de GLX.
        using glXSwapBuffersFn       = void(*)(void* /*Display*/, unsigned long /*GLXDrawable*/);
        using glXGetCurrentDisplayFn = void* (*)();

        static glXSwapBuffersFn       s_glX_swap_buffers        = nullptr;
        static glXGetCurrentDisplayFn s_glX_get_current_display = nullptr;
        static bool                   s_tried                   = false;

        if (!s_tried) {
            s_tried = true;
            void* libGL = dlopen("libGL.so.1", RTLD_LAZY | RTLD_NOLOAD);
            if (!libGL) libGL = dlopen("libGL.so", RTLD_LAZY);
            if (libGL) {
                s_glX_swap_buffers = reinterpret_cast<glXSwapBuffersFn>(dlsym(libGL, "glXSwapBuffers"));
                s_glX_get_current_display = reinterpret_cast<glXGetCurrentDisplayFn>(dlsym(libGL, "glXGetCurrentDisplay"));
            }
        }

        // GLDevice mantiene su contexto GLX activo durante toda la vida del device, así que el
        // Display que abrió puede recuperarse aquí sin que GLSwapchain necesite almacenarlo él mismo.
        void* display = s_glX_get_current_display ? s_glX_get_current_display() : nullptr;

        if (s_glX_swap_buffers && display && m_native_window) {
            s_glX_swap_buffers(display, reinterpret_cast<uintptr_t>(m_native_window));
        }
        else {
            glFlush();
        }

#elif defined(__APPLE__)
        @autoreleasepool{
            if (NSOpenGLContext* ctx = [NSOpenGLContext currentContext]) {
                [ctx flushBuffer] ;
            }
 else {
  glFlush();
}
        }

#else
        // Plataforma desconocida — se hace flush como mejor esfuerzo
        glFlush();
#endif
    }

    // resize -------------------------------------------------------------------------------------
    void GLSwapchain::resize(anxiety::rendering::rhi::Extent2D new_extent) {
        m_extent = new_extent;
        // OpenGL de escritorio: el framebuffer por defecto sigue automáticamente el tamaño de la
        // ventana en cuanto la plataforma redimensiona su superficie. No hace falta ninguna acción explícita aquí.
        glViewport(0, 0, static_cast<GLsizei>(new_extent.width), static_cast<GLsizei>(new_extent.height));
        LOGF_INFO("GLSwapchain", "Redimensionado a {}x{}.", new_extent.width, new_extent.height);
    }
} // namespace anxiety::rendering::backend::opengl

#endif // ANXIETY_BACKEND_OPENGL
