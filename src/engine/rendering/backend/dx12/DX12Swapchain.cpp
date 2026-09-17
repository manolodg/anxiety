#ifdef ANXIETY_BACKEND_DX12

#include "DX12Swapchain.h"
#include "DX12Device.h"
#include "DX12Helpers.h"
#include "Logger.h"

#include <cassert>

namespace anxiety::rendering::backend::dx12 {
    static constexpr char k_category[] = "RHI";

    DX12Swapchain::DX12Swapchain(DX12Device& device, const rhi::SwapchainDesc& desc) : m_device(device), m_extent(desc.extent), m_image_count(desc.image_count <= k_max_images ? desc.image_count : k_max_images), m_vsync(desc.vsync), m_format(rhi::Format::BGRA8_Unorm) {
        // Inicializa los arrays a cero.
        m_backbuffer_handles.fill({});
        m_rtv_handles.fill({ 0 });

        // Comprobación blanda, no un rechazo: hoy DX12 solo sabe interpretar un HWND de Win32 pase
        // lo que pase en surface_type, así que esto es una señal de aviso para cuando existan más
        // backends (Vulkan, etc.) que sí puedan recibir otros tipos — no bloquea nada todavía.
        if (desc.surface_type != rhi::NativeSurfaceType::Unknown && desc.surface_type != rhi::NativeSurfaceType::Win32) {
            LOG_WARNING(k_category, "DX12Swapchain: se recibió un NativeSurfaceType que no es Win32; se usará igualmente como HWND.");
        }

        auto* hwnd = static_cast<HWND>(desc.native_window_handle);

        DXGI_SWAP_CHAIN_DESC1 sc_desc{};
        sc_desc.Width       = desc.extent.width;
        sc_desc.Height      = desc.extent.height;
        sc_desc.Format      = to_D3D12_format(m_format);
        sc_desc.BufferCount = m_image_count;
        sc_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sc_desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sc_desc.SampleDesc  = { 1, 0 };
        sc_desc.Scaling     = DXGI_SCALING_STRETCH;
        sc_desc.AlphaMode   = DXGI_ALPHA_MODE_UNSPECIFIED;

        ComPtr<IDXGISwapChain1> sc1;
        HRESULT hr = device.dxgi_factory()->CreateSwapChainForHwnd(device.command_queue(), hwnd, &sc_desc, nullptr, nullptr, &sc1);

        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "CreateSwapChainForHwnd falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return;
        }

        // Desactiva el atajo Alt+Intro para pasar a pantalla completa.
        device.dxgi_factory()->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

        hr = sc1.As(&m_swapchain);
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "QueryInterface IDXGISwapChain3 falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return;
        }

        create_backbuffers();
        m_current_index = m_swapchain->GetCurrentBackBufferIndex();

        LOGF_INFO(k_category, "Swapchain creado ({}×{}, {} imágenes, vsync={}).", m_extent.width, m_extent.height, m_image_count, m_vsync ? "activado" : "desactivado");
    }

    DX12Swapchain::~DX12Swapchain() {
        // Garantiza que la GPU haya terminado antes de liberar los recursos de los backbuffers.
        m_device.wait_idle();
        release_backbuffers();
    }

    // Operaciones por fotograma ------------------------------------------------------------------
    uint32_t DX12Swapchain::acquire_next_image() {
        m_current_index = m_swapchain->GetCurrentBackBufferIndex();
        return m_current_index;
    }

    void DX12Swapchain::present() { m_swapchain->Present(m_vsync ? 1u : 0u, 0); }

    anxiety::rendering::rhi::TextureHandle DX12Swapchain::current_backbuffer() const noexcept { return m_backbuffer_handles[m_current_index]; }

    // Redimensionado -------------------------------------------------------------------------------
    void DX12Swapchain::resize(anxiety::rendering::rhi::Extent2D new_extent) {
        if (new_extent.width == m_extent.width && new_extent.height == m_extent.height) return;

        m_device.wait_idle();
        release_backbuffers();

        HRESULT hr = m_swapchain->ResizeBuffers(m_image_count, new_extent.width, new_extent.height, to_D3D12_format(m_format), 0);
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "ResizeBuffers falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return;
        }

        m_extent = new_extent;
        create_backbuffers();
        m_current_index = m_swapchain->GetCurrentBackBufferIndex();
    }

    // Helpers privados -------------------------------------------------------------------------------
    void DX12Swapchain::create_backbuffers() {
        for (uint32_t i = 0; i < m_image_count; ++i) {
            HRESULT hr = m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_backbuffers[i]));
            if (FAILED(hr)) {
                LOGF_ERROR(k_category, "GetBuffer({}) falló: 0x{:08X}", i, static_cast<uint32_t>(hr));
                continue;
            }

            // Reserva un RTV y crea la vista.
            m_rtv_handles[i] = m_device.allocate_RTV();
            m_device.d3d_device()->CreateRenderTargetView(m_backbuffers[i].Get(), nullptr, m_rtv_handles[i]);

            // Registra el buffer en el pool del dispositivo para que los command buffers puedan buscarlo por TextureHandle.
            m_backbuffer_handles[i] = m_device.register_external_texture(m_backbuffers[i].Get(), m_rtv_handles[i], rhi::ResourceState::Present);
        }
    }

    void DX12Swapchain::release_backbuffers() {
        for (uint32_t i = 0; i < m_image_count; ++i) {
            if (m_backbuffer_handles[i].is_valid()) {
                m_device.unregister_texture(m_backbuffer_handles[i]);
                m_backbuffer_handles[i] = {};
            }
            m_backbuffers[i].Reset();
            m_rtv_handles[i] = { 0 };
        }
    }
} // namespace anxiety::rendering::backend::dx12

#endif ANXIETY_BACKEND_DX12