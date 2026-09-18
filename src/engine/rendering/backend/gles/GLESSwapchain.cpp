#ifdef ANXIETY_BACKEND_GLES

#include "GLESSwapchain.h"
#include "Logger.h"

#include <EGL/eglext.h>

namespace anxiety::rendering::backend::gles {
    // Constructor --------------------------------------------------------------------------------
    GLESSwapchain::GLESSwapchain(const rhi::SwapchainDesc& desc) : m_extent(desc.extent), m_format(desc.format), m_vsync(desc.vsync) {
        // 1. Obtiene el display de EGL ------------------------------------------------------------------
        m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (m_display == EGL_NO_DISPLAY) {
            LOG_ERROR("GLESSwapchain", "eglGetDisplay(EGL_DEFAULT_DISPLAY) falló.");
            return;
        }

        // 2. Inicializa EGL ----------------------------------------------------------------------
        EGLint major = 0, minor = 0;
        if (!eglInitialize(m_display, &major, &minor)) {
            LOGF_ERROR("GLESSwapchain", "eglInitialize falló: 0x{:04X}.", eglGetError());
            return;
        }
        LOGF_INFO("GLESSwapchain", "Versión de EGL {}.{}.", major, minor);

        // 3. Elige la configuración ----------------------------------------------------------------
        // Solicita una surface RGBA8 + depth24 para GLES 3.x
        const EGLint config_attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,  // GLES 3.x (vía extensión KHR si hace falta)
            EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
            EGL_RED_SIZE,        8,
            EGL_GREEN_SIZE,      8,
            EGL_BLUE_SIZE,       8,
            EGL_ALPHA_SIZE,      8,
            EGL_DEPTH_SIZE,      24,
            EGL_STENCIL_SIZE,    8,
            EGL_NONE
        };

        EGLConfig config = nullptr;
        EGLint    num_configs = 0;
        if (!eglChooseConfig(m_display, config_attribs, &config, 1, &num_configs) || num_configs == 0) {
            // Reintenta sin los bits de stencil y alpha — algunos drivers embebidos son quisquillosos
            const EGLint fallback_attribs[] = {
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
                EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
                EGL_RED_SIZE,        8,
                EGL_GREEN_SIZE,      8,
                EGL_BLUE_SIZE,       8,
                EGL_DEPTH_SIZE,      16,
                EGL_NONE
            };
            if (!eglChooseConfig(m_display, fallback_attribs, &config, 1, &num_configs) || num_configs == 0) {
                LOGF_ERROR("GLESSwapchain", "eglChooseConfig falló: 0x{:04X}.", eglGetError());
                eglTerminate(m_display);
                m_display = EGL_NO_DISPLAY;
                return;
            }
            LOG_WARNING("GLESSwapchain", "Usando configuración EGL de reserva (sin stencil/alpha).");
        }

        // 4. Crea el contexto (GLES 3.x) -----------------------------------------------------------
        eglBindAPI(EGL_OPENGL_ES_API);

        const EGLint ctx_attribs[] = {
            EGL_CONTEXT_MAJOR_VERSION, 3,
            EGL_CONTEXT_MINOR_VERSION, 0,
            EGL_NONE
        };
        m_context = eglCreateContext(m_display, config, EGL_NO_CONTEXT, ctx_attribs);
        if (m_context == EGL_NO_CONTEXT) {
            LOGF_ERROR("GLESSwapchain", "eglCreateContext falló: 0x{:04X}.", eglGetError());
            eglTerminate(m_display);
            m_display = EGL_NO_DISPLAY;
            return;
        }

        // 5. Crea la surface de ventana ---------------------------------------------------------------
        if (!desc.native_window_handle) {
            LOG_ERROR("GLESSwapchain", "SwapchainDesc::nativeWindowHandle es nulo.");
            eglDestroyContext(m_display, m_context);
            eglTerminate(m_display);
            m_display = EGL_NO_DISPLAY;
            m_context = EGL_NO_CONTEXT;
            return;
        }

        m_surface = eglCreateWindowSurface(m_display, config, reinterpret_cast<EGLNativeWindowType>(desc.native_window_handle), nullptr);

        if (m_surface == EGL_NO_SURFACE) {
            LOGF_ERROR("GLESSwapchain", "eglCreateWindowSurface falló: 0x{:04X}.", eglGetError());
            eglDestroyContext(m_display, m_context);
            eglTerminate(m_display);
            m_display = EGL_NO_DISPLAY;
            m_context = EGL_NO_CONTEXT;
            return;
        }

        // 6. Activa el contexto + intervalo de swap --------------------------------------------------------
        if (!eglMakeCurrent(m_display, m_surface, m_surface, m_context)) {
            LOGF_ERROR("GLESSwapchain", "eglMakeCurrent falló: 0x{:04X}.", eglGetError());
            eglDestroySurface(m_display, m_surface);
            eglDestroyContext(m_display, m_context);
            eglTerminate(m_display);
            m_display = EGL_NO_DISPLAY;
            m_context = EGL_NO_CONTEXT;
            m_surface = EGL_NO_SURFACE;
            return;
        }

        eglSwapInterval(m_display, m_vsync ? 1 : 0);

        // 7. Handle centinela del backbuffer ----------------------------------------------------------
        // id == 1 está reservado permanentemente para el framebuffer por defecto
        m_backbuffer = anxiety::rendering::rhi::TextureHandle{ 1 };

        LOGF_INFO("GLESSwapchain", "Surface de EGL creada {}x{} vsync={}.", desc.extent.width, desc.extent.height, m_vsync);
    }

    // Destructor ---------------------------------------------------------------------------------
    GLESSwapchain::~GLESSwapchain() {
        if (m_display == EGL_NO_DISPLAY) return;

        // Libera el contexto antes de destruir los objetos
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (m_context != EGL_NO_CONTEXT) {
            eglDestroyContext(m_display, m_context);
            m_context = EGL_NO_CONTEXT;
        }

        if (m_surface != EGL_NO_SURFACE) {
            eglDestroySurface(m_display, m_surface);
            m_surface = EGL_NO_SURFACE;
        }

        eglTerminate(m_display);
        m_display = EGL_NO_DISPLAY;

        LOG_INFO("GLESSwapchain", "Surface de EGL destruida.");
    }

    // present ------------------------------------------------------------------------------------
    void GLESSwapchain::present() {
        if (m_display == EGL_NO_DISPLAY || m_surface == EGL_NO_SURFACE) {
            LOG_WARNING("GLESSwapchain", "present() llamado con una surface de EGL inválida.");
            return;
        }

        if (!eglSwapBuffers(m_display, m_surface)) {
            const EGLint err = eglGetError();
            if (err == EGL_BAD_SURFACE || err == EGL_BAD_CONTEXT) {
                // La surface/contexto se perdió — esto es recuperable; quien llama debe
                // recrear el swapchain.
                LOGF_ERROR("GLESSwapchain", "eglSwapBuffers: surface/contexto perdido (0x{:04X}).", err);
            }
            else {
                LOGF_WARNING("GLESSwapchain", "eglSwapBuffers: error 0x{:04X}.", err);
            }
        }
    }

    // resize -------------------------------------------------------------------------------------
    void GLESSwapchain::resize(rhi::Extent2D new_extent) {
        m_extent = new_extent;
        // Las surfaces de ventana de EGL siguen automáticamente el tamaño de la ventana nativa.
        // No se requiere una recreación explícita a menos que se pierda la surface.
        LOGF_INFO("GLESSwapchain", "Redimensionado a {}x{}.", new_extent.width, new_extent.height);
    }
} // namespace anxiety::rendering::backend::gles

#endif // ANXIETY_BACKEND_GLES
