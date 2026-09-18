#ifdef ANXIETY_BACKEND_GLES

#pragma once

#include "../../rhi/ISwapchain.h"

#include <EGL/egl.h>

namespace anxiety::rendering::backend::gles {
    // GLESSwapchain — ISwapchain para OpenGL ES usando la API de plataforma EGL ------------------------
    // Gestiona el ciclo de vida del display, la surface y el contexto de EGL. El contexto se activa
    // durante la construcción y se libera durante la destrucción.
    //
    // El backbuffer se representa mediante el TextureHandle{1} centinela, que se corresponde con el
    // framebuffer 0 por defecto de GL en las operaciones del command buffer, en línea con la convención
    // del GLSwapchain de escritorio.
    // --------------------------------------------------------------------------------------------
    class GLESSwapchain final : public rhi::ISwapchain {
    public:
        explicit GLESSwapchain(const rhi::SwapchainDesc& desc);
        ~GLESSwapchain() override;

        // No es copiable ni movible una vez inicializado el estado de EGL
        GLESSwapchain(const GLESSwapchain&) = delete;
        GLESSwapchain& operator=(const GLESSwapchain&) = delete;

        // ISwapchain -----------------------------------------------------------------------------
        [[nodiscard]] uint32_t           image_count()        const noexcept override { return 1; }
        [[nodiscard]] rhi::Extent2D      extent()             const noexcept override { return m_extent; }
        [[nodiscard]] rhi::Format        format()             const noexcept override { return m_format; }
        [[nodiscard]] rhi::TextureHandle current_backbuffer() const noexcept override { return m_backbuffer; }

        uint32_t acquire_next_image()             override { return 0; }
        void     present()                        override;
        void     resize(rhi::Extent2D new_extent) override;

        // Accesores de EGL (usados por GLESDevice si es necesario) -------------------------------------------
        [[nodiscard]] EGLDisplay display()  const noexcept { return m_display; }
        [[nodiscard]] EGLSurface surface()  const noexcept { return m_surface; }
        [[nodiscard]] EGLContext context()  const noexcept { return m_context; }
        [[nodiscard]] bool       is_valid() const noexcept { return m_display != EGL_NO_DISPLAY && m_surface != EGL_NO_SURFACE; }

    private:
        EGLDisplay m_display = EGL_NO_DISPLAY;
        EGLSurface m_surface = EGL_NO_SURFACE;
        EGLContext m_context = EGL_NO_CONTEXT;

        rhi::TextureHandle m_backbuffer;
        rhi::Extent2D      m_extent;
        rhi::Format        m_format;
        bool               m_vsync = true;
    };
} // namespace anxiety::rendering::backend::gles

#endif // ANXIETY_BACKEND_GLES