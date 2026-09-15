#include "DX11Swapchain.h"
#include "DX11Device.h"
#include "DX11Helpers.h"
#include "Logger.h"

namespace anxiety::rendering::backend::dx11 {
    static constexpr char k_category[] = "RHI";

    DX11Swapchain::DX11Swapchain(DX11Device& device, const anxiety::rendering::rhi::SwapchainDesc& desc) : m_device(device), m_extent(desc.extent), m_image_count(std::min(desc.image_count, k_max_images)),
        m_vsync(desc.vsync), m_format(desc.format) {
        if (!desc.native_window_handle) {
            LOG_ERROR(k_category, "DX11Swapchain: HWND nulo.");
            return;
        }

        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.Width       = desc.extent.width;
        sd.Height      = desc.extent.height;
        sd.Format      = to_DX11_format(desc.format);
        sd.SampleDesc  = { 1, 0 };
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = m_image_count;
        sd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.Flags       = 0;

        HRESULT hr = device.dxgi_factory()->CreateSwapChainForHwnd(device.d3d_device(), static_cast<HWND>(desc.native_window_handle), &sd, nullptr, nullptr, &m_swapchain);
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "CreateSwapChainForHwnd falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return;
        }

        // Desactiva el atajo Alt+Intro para pantalla completa — las aplicaciones lo gestionan ellas mismas.
        device.dxgi_factory()->MakeWindowAssociation(static_cast<HWND>(desc.native_window_handle), DXGI_MWA_NO_ALT_ENTER);

        create_backbuffers();
        LOGF_INFO(k_category, "DX11Swapchain creado ({}x{}, {} buffers).", desc.extent.width, desc.extent.height, m_image_count);
    }
    DX11Swapchain::~DX11Swapchain() { release_backbuffers(); }

    // ISwapchain ---------------------------------------------------------------------------------
    anxiety::rendering::rhi::TextureHandle DX11Swapchain::current_backbuffer() const noexcept { return m_handles[m_current_idx]; }

    uint32_t DX11Swapchain::acquire_next_image() {
        // Modelo flip de DX11: el índice del back buffer actual siempre es 0 después de Present.
        m_current_idx = 0;
        return m_current_idx;
    }

    void DX11Swapchain::present() { m_swapchain->Present(m_vsync ? 1u : 0u, 0u); }

    void DX11Swapchain::resize(anxiety::rendering::rhi::Extent2D new_extent) {
        if (new_extent.width == 0 || new_extent.height == 0) return;

        release_backbuffers();

        HRESULT hr = m_swapchain->ResizeBuffers(m_image_count, new_extent.width, new_extent.height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "ResizeBuffers falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return;
        }

        m_extent = new_extent;
        create_backbuffers();
    }

    // Helpers privados ---------------------------------------------------------------------------
    void DX11Swapchain::create_backbuffers() {
        // Con FLIP_DISCARD, solo el índice de buffer 0 es accesible.
        ComPtr<ID3D11Texture2D> backbuffer;
        HRESULT hr = m_swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "GetBuffer(0) falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return;
        }

        m_handles[0] = m_device.register_external_texture(backbuffer.Get(), anxiety::rendering::rhi::ResourceState::Present);
    }

    void DX11Swapchain::release_backbuffers() {
        for (uint32_t i = 0; i < m_image_count; ++i) {
            if (m_handles[i].is_valid()) {
                m_device.unregister_texture(m_handles[i]);
                m_handles[i] = {};
            }
        }
    }
} // namespace anxiety::rendering::backend::dx11
