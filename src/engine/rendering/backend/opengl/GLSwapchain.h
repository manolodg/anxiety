#ifdef ANXIETY_BACKEND_OPENGL

#pragma once

#include "GLHelpers.h"
#include "../../rhi/ISwapchain.h"

namespace anxiety::rendering::backend::opengl {
    // GLSwapchain — implementación de ISwapchain para OpenGL de escritorio. ----------------------
    // A diferencia de Vulkan/DX12, OpenGL usa el sistema de ventanas de la plataforma (WGL/GLX/CGL)
    // para el intercambio de buffers. Se asume que el contexto GL real ya está activo antes de
    // llamar a cualquier método del device; GLSwapchain solo dirige las llamadas de intercambio de buffers.
    //
    // El backbuffer está representado por el TextureHandle{1} sentinela, que se corresponde con el
    // framebuffer 0 por defecto en GLCommandBuffer::clearRenderTarget().
    // --------------------------------------------------------------------------------------------
    class GLSwapchain final : public rhi::ISwapchain {
    public:
        explicit GLSwapchain(const rhi::SwapchainDesc& desc);
        ~GLSwapchain() override = default;

        // ISwapchain -----------------------------------------------------------------------------
        [[nodiscard]] uint32_t           image_count()        const noexcept override { return 1; }
        [[nodiscard]] rhi::Extent2D      extent()             const noexcept override { return m_extent; }
        [[nodiscard]] rhi::Format        format()             const noexcept override { return m_format; }
        [[nodiscard]] rhi::TextureHandle current_backbuffer() const noexcept override { return m_backbuffer; }

        uint32_t acquire_next_image()             override { return 0; }
        void     present()                        override;
        void     resize(rhi::Extent2D new_extent) override;

    private:
        rhi::TextureHandle m_backbuffer;    // < handle sentinela {1} == FBO por defecto
        rhi::Extent2D      m_extent;
        rhi::Format        m_format;
        bool               m_vsync = true;

        void* m_native_window = nullptr;                        // < HWND en Win32, Window en X11, etc.

#ifdef _WIN32
        void* m_hdc = nullptr;                        // < HDC asociado a la ventana nativa
#endif
    };
} // namespace anxiety::rendering::backend::opengl

#endif // ANXIETY_BACKEND_OPENGL