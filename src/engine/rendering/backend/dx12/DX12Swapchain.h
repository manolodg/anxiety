#ifdef ANXIETY_BACKEND_DX12

#pragma once

#include "../../rhi/ISwapchain.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>

namespace anxiety::rendering::backend::dx12 {
    using Microsoft::WRL::ComPtr;

    class DX12Device;                               // declaración adelantada — el tipo completo solo hace falta en el .cpp

    // DX12Swapchain — implementación de rhi::ISwapchain para DirectX 12 --------------------------
    // Envuelve un IDXGISwapChain3 y registra cada backbuffer como un TextureHandle en el
    // DX12Device propietario. Los handles de backbuffer son válidos hasta que el swapchain se
    // destruye o se redimensiona.
    // --------------------------------------------------------------------------------------------
    class DX12Swapchain final : public anxiety::rendering::rhi::ISwapchain {
    public:
        static constexpr uint32_t k_max_images = 3;

        DX12Swapchain(DX12Device& device, const anxiety::rendering::rhi::SwapchainDesc& desc);
        ~DX12Swapchain() override;

        // rhi::ISwapchain ------------------------------------------------------------------------
        [[nodiscard]] uint32_t                               image_count()        const noexcept override { return m_image_count; }
        [[nodiscard]] anxiety::rendering::rhi::Extent2D      extent()             const noexcept override { return m_extent; }
        [[nodiscard]] anxiety::rendering::rhi::Format        format()             const noexcept override { return m_format; }
        [[nodiscard]] anxiety::rendering::rhi::TextureHandle current_backbuffer() const noexcept override;

        uint32_t acquire_next_image()                                 override;
        void     present()                                            override;
        void     resize(anxiety::rendering::rhi::Extent2D new_extent) override;

    private:
        void create_backbuffers();
        void release_backbuffers();

        DX12Device& m_device;
        ComPtr<IDXGISwapChain3>             m_swapchain;

        anxiety::rendering::rhi::Extent2D   m_extent;
        anxiety::rendering::rhi::Format     m_format = anxiety::rendering::rhi::Format::BGRA8_Unorm;
        uint32_t                            m_image_count = 0;
        uint32_t                            m_current_index = 0;
        bool                                m_vsync = true;

        // Recursos por cada backbuffer (hasta k_max_images).
        std::array<ComPtr<ID3D12Resource>, k_max_images>                  m_backbuffers;
        std::array<anxiety::rendering::rhi::TextureHandle, k_max_images>  m_backbuffer_handles;
        std::array<D3D12_CPU_DESCRIPTOR_HANDLE, k_max_images>             m_rtv_handles;
    };
} // namespace anxiety::rendering::backend::dx12

#endif ANXIETY_BACKEND_DX12