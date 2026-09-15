#pragma once

#include "../../rhi/ISwapchain.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>

namespace anxiety::rendering::backend::dx11 {
    using Microsoft::WRL::ComPtr;

    class DX11Device;                                   // forward

    // DX11Swapchain — implementación de rhi::ISwapchain para DirectX 11 -------------------------------
    // Envuelve un IDXGISwapChain1 y registra cada backbuffer como un TextureHandle en el DX11Device
    // propietario. Las texturas del backbuffer son propiedad de DXGI; en el pool de texturas del
    // dispositivo solo conservamos punteros no propietarios.
    // --------------------------------------------------------------------------------------------
    class DX11Swapchain final : public anxiety::rendering::rhi::ISwapchain {
    public:
        static constexpr uint32_t k_max_images = 3;

        DX11Swapchain(DX11Device& device, const anxiety::rendering::rhi::SwapchainDesc& desc);
        ~DX11Swapchain() override;

        // rhi::ISwapchain ------------------------------------------------------------------------
        [[nodiscard]] uint32_t                               image_count()        const noexcept override { return m_image_count; }
        [[nodiscard]] anxiety::rendering::rhi::Extent2D      extent()             const noexcept override { return m_extent; }
        [[nodiscard]] anxiety::rendering::rhi::Format        format()             const noexcept override { return m_format; }
        [[nodiscard]] anxiety::rendering::rhi::TextureHandle current_backbuffer() const noexcept override;

        uint32_t acquire_next_image()                                 override;
        void     present()                                            override;
        void     resize(anxiety::rendering::rhi::Extent2D new_extent) override;

        [[nodiscard]] bool is_valid() const noexcept { return m_swapchain != nullptr; }

    private:
        void create_backbuffers();
        void release_backbuffers();

        DX11Device& m_device;
        ComPtr<IDXGISwapChain1>           m_swapchain;

        anxiety::rendering::rhi::Extent2D m_extent;
        anxiety::rendering::rhi::Format   m_format      = anxiety::rendering::rhi::Format::BGRA8_Unorm;
        uint32_t                          m_image_count = 0;
        uint32_t                          m_current_idx = 0;
        bool                              m_vsync       = true;

        std::array<anxiety::rendering::rhi::TextureHandle, k_max_images> m_handles;
    };
} // namespace anxiety::rendering::backend::dx11
