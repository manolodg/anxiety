#pragma once

#include "../../rhi/IDevice.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <cstdint>
#include <memory>
#include <vector>

namespace anxiety::rendering::backend::dx11 {
    using Microsoft::WRL::ComPtr;

    // TextureSlot — una entrada en el pool de recursos de texturas del dispositivo ------------------------------
    struct TextureSlot {
        ComPtr<ID3D11Texture2D>                texture;
        ComPtr<ID3D11RenderTargetView>         rtv;
        anxiety::rendering::rhi::ResourceState state  = anxiety::rendering::rhi::ResourceState::Undefined;
        uint32_t                               width  = 0;
        uint32_t                               height = 0;
        DXGI_FORMAT                            format = DXGI_FORMAT_UNKNOWN;
        bool                                   alive  = false;
    };
    // DX11Device — implementación de rhi::IDevice para DirectX 11 -------------------------------------
    class DX11Device final : public anxiety::rendering::rhi::IDevice {
    public:
        explicit DX11Device(bool enable_debug_layer = false);
        ~DX11Device() override;

        [[nodiscard]] bool is_valid() const noexcept { return m_valid; }

        // rhi::IDevice ---------------------------------------------------------------------------
        [[nodiscard]] std::string_view backend_name() const noexcept override { return "DirectX 11"; }

        [[nodiscard]] anxiety::rendering::rhi::BufferHandle  create_buffer(const anxiety::rendering::rhi::BufferDesc& desc) override;
        [[nodiscard]] anxiety::rendering::rhi::TextureHandle create_texture(const anxiety::rendering::rhi::TextureDesc&)    override;
        void destroy_buffer(anxiety::rendering::rhi::BufferHandle)   override;
        void destroy_texture(anxiety::rendering::rhi::TextureHandle) override;

        [[nodiscard]] std::unique_ptr<anxiety::rendering::rhi::ICommandBuffer> create_command_buffer()                                         override;
        [[nodiscard]] std::unique_ptr<anxiety::rendering::rhi::ISwapchain>     create_swapchain(const anxiety::rendering::rhi::SwapchainDesc&) override;

        void submit(anxiety::rendering::rhi::ICommandBuffer&) override;
        void wait_idle()                                      override;

        // Accesores internos (usados por DX11CommandBuffer / DX11Swapchain) -------------------------
        [[nodiscard]] ID3D11Device*        d3d_device()      const noexcept { return m_device.Get(); }
        [[nodiscard]] ID3D11DeviceContext* immediate_ctx()   const noexcept { return m_imm_ctx.Get(); }
        [[nodiscard]] IDXGIFactory2*       dxgi_factory()    const noexcept { return m_factory.Get(); }

        // Búsquedas de textura / RTV / DSV / SRV — devuelve nullptr si el handle no es válido.
        [[nodiscard]] ID3D11RenderTargetView* lookup_RTV(anxiety::rendering::rhi::TextureHandle) const noexcept;

        // Registra una textura externa (p. ej. el backbuffer del swapchain). Crea un RTV automáticamente; toma propiedad compartida de la textura.
        [[nodiscard]] anxiety::rendering::rhi::TextureHandle register_external_texture(ID3D11Texture2D* texture, anxiety::rendering::rhi::ResourceState state);

        void unregister_texture(anxiety::rendering::rhi::TextureHandle);

    private:
        [[nodiscard]] uint32_t allocate_texture_slot();
        void                   free_texture_slot(uint32_t slot);

        bool m_valid = false;

        ComPtr<ID3D11Device>        m_device;
        ComPtr<ID3D11DeviceContext> m_imm_ctx;
        ComPtr<IDXGIFactory2>       m_factory;

        std::vector<TextureSlot> m_textures;
        std::vector<uint32_t>    m_texture_free_list;
    };
} // namespace anxiety::rendering::backend::dx11
