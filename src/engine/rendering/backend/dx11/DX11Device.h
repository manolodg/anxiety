#ifdef ANXIETY_BACKEND_DX11

#pragma once

#include "../../rhi/IDevice.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace anxiety::rendering::backend::dx11 {
    using Microsoft::WRL::ComPtr;

    // TextureSlot — una entrada en el pool de recursos de texturas del dispositivo ------------------------------
    struct TextureSlot {
        ComPtr<ID3D11Texture2D>                texture;
        ComPtr<ID3D11RenderTargetView>         rtv;
        ComPtr<ID3D11DepthStencilView>         dsv;
        ComPtr<ID3D11ShaderResourceView>       srv;
        anxiety::rendering::rhi::ResourceState state    = rhi::ResourceState::Undefined;
        uint32_t                               width    = 0;
        uint32_t                               height   = 0;
        DXGI_FORMAT                            format   = DXGI_FORMAT_UNKNOWN;
        bool                                   is_depth = false;
        bool                                   alive    = false;
    };

    // BufferSlot — una entrada en el pool de recursos de buffers del dispositivo ------------------
    struct BufferSlot {
        ComPtr<ID3D11Buffer> buffer;
        uint64_t             size_bytes = 0;
        bool                 alive      = false;
    };

    // DX11Device — implementación de rhi::IDevice para DirectX 11 -------------------------------------
    class DX11Device final : public anxiety::rendering::rhi::IDevice {
    public:
        explicit DX11Device(bool enable_debug_layer = false);
        ~DX11Device() override;

        [[nodiscard]] bool is_valid() const noexcept { return m_valid; }

        // rhi::IDevice ---------------------------------------------------------------------------
        [[nodiscard]] std::string_view backend_name() const noexcept override { return "DirectX 11"; }

        [[nodiscard]] rhi::BufferHandle  create_buffer(const rhi::BufferDesc& desc, const void* initial_data = nullptr, size_t initial_data_sz = 0) override;
        [[nodiscard]] rhi::TextureHandle create_texture(const rhi::TextureDesc&)                                                                    override;
        void destroy_buffer(rhi::BufferHandle)   override;
        void destroy_texture(rhi::TextureHandle) override;

        void write_buffer(rhi::BufferHandle handle, const void* data, size_t offset, size_t size)                  override;
        void upload_texture_data(rhi::TextureHandle handle, const uint8_t* rgba8, uint32_t width, uint32_t height) override;

        [[nodiscard]] std::vector<uint8_t>                  compile_shader_from_source(const char* source, const char* entry_point, rhi::ShaderStage stage) override;
        [[nodiscard]] std::unique_ptr<rhi::IShader>         create_shader(const rhi::ShaderDesc& desc, rhi::ShaderStage stage)                              override;
        [[nodiscard]] std::unique_ptr<rhi::IPipeline>       create_pipeline(const rhi::PipelineDesc& desc)                                                  override;
        [[nodiscard]] std::unique_ptr<rhi::IDescriptorSet>  create_descriptor_set(const rhi::DescriptorSetLayout& layout)                                   override;

        [[nodiscard]] std::unique_ptr<rhi::ICommandBuffer>  create_command_buffer()                                                                         override;
        [[nodiscard]] std::unique_ptr<rhi::ISwapchain>      create_swapchain(const anxiety::rendering::rhi::SwapchainDesc&)                                 override;

        void submit(rhi::ICommandBuffer&) override;
        void wait_idle()                  override;

        // Accesores internos (usados por DX11CommandBuffer / DX11Swapchain) -------------------------
        [[nodiscard]] ID3D11Device*        d3d_device()      const noexcept { return m_device.Get(); }
        [[nodiscard]] ID3D11DeviceContext* immediate_ctx()   const noexcept { return m_imm_ctx.Get(); }
        [[nodiscard]] IDXGIFactory2*       dxgi_factory()    const noexcept { return m_factory.Get(); }

        // Búsquedas de textura / RTV / DSV / SRV — devuelve nullptr si el handle no es válido.
        [[nodiscard]] ID3D11RenderTargetView*       lookup_RTV(rhi::TextureHandle)     const noexcept;
        [[nodiscard]] ID3D11DepthStencilView*       lookup_DSV(rhi::TextureHandle)     const noexcept;
        [[nodiscard]] ID3D11ShaderResourceView*     lookup_SRV(rhi::TextureHandle)     const noexcept;
        [[nodiscard]] std::pair<uint32_t, uint32_t> texture_extent(rhi::TextureHandle) const noexcept;

        // Búsqueda de buffer — devuelve nullptr si el handle no es válido.
        [[nodiscard]] ID3D11Buffer* lookup_buffer(anxiety::rendering::rhi::BufferHandle) const noexcept;

        // Registra una textura externa (p. ej. el backbuffer del swapchain). Crea un RTV automáticamente; toma propiedad compartida de la textura.
        [[nodiscard]] anxiety::rendering::rhi::TextureHandle register_external_texture(ID3D11Texture2D* texture, anxiety::rendering::rhi::ResourceState state);

        void unregister_texture(anxiety::rendering::rhi::TextureHandle);

    private:
        [[nodiscard]] uint32_t allocate_texture_slot();
        void                   free_texture_slot(uint32_t slot);
        [[nodiscard]] uint32_t allocate_buffer_slot();
        void                   free_buffer_slot(uint32_t slot);

        bool m_valid = false;

        ComPtr<ID3D11Device>        m_device;
        ComPtr<ID3D11DeviceContext> m_imm_ctx;
        ComPtr<IDXGIFactory2>       m_factory;

        std::vector<TextureSlot> m_textures;
        std::vector<uint32_t>    m_texture_free_list;
        std::vector<BufferSlot>  m_buffers;
        std::vector<uint32_t>    m_buffer_free_list;
    };
} // namespace anxiety::rendering::backend::dx11

#endif ANXIETY_BACKEND_DX11