#ifdef ANXIETY_BACKEND_METAL

#pragma once

#include "../../rhi/IDevice.h"

#include <vector>
#include <cstdint>

namespace anxiety::rendering::backend::metal {
    // Tipos de slot de recurso ---------------------------------------------------------------------
    struct MetalTextureSlot {
        void*    texture  = nullptr;               // id<MTLTexture>
        uint32_t width    = 0;
        uint32_t height   = 0;
        bool     is_depth = false;
        bool     external = false;                 // true = propiedad de un drawable de CAMetalLayer
        bool     alive    = false;
    };

        struct MetalBufferSlot {
        void*    buffer  = nullptr;                 // id<MTLBuffer>
        void*    mapped  = nullptr;                 // MTLBuffer.contents (no nulo para buffers shared/managed)
        uint64_t size    = 0;
        bool     alive   = false;
    };

    // MetalDevice --------------------------------------------------------------------------------
    class MetalDevice final : public rhi::IDevice {
    public:
        explicit MetalDevice(bool enable_validation = false);
        ~MetalDevice() override;

        [[nodiscard]] bool is_valid() const noexcept { return m_valid; }

        // IDevice interface ----------------------------------------------------------------------
        [[nodiscard]] std::string_view backend_name() const noexcept override { return "Metal"; }

        [[nodiscard]] rhi::BufferHandle  create_buffer(const rhi::BufferDesc& desc, const void* initial_data = nullptr, size_t initial_data_sz = 0)   override;
        [[nodiscard]] rhi::TextureHandle create_texture(const rhi::TextureDesc& desc) override;
        void destroy_buffer(rhi::BufferHandle handle)   override;
        void destroy_texture(rhi::TextureHandle handle) override;

        void write_buffer(rhi::BufferHandle handle, const void* data, size_t offset, size_t size) override;

        void upload_texture_data(rhi::TextureHandle handle, const uint8_t* rgba8, uint32_t width, uint32_t height) override;

        [[nodiscard]] std::vector<uint8_t> compile_shader_from_source(const char* source, const char* entry_point, rhi::ShaderStage stage) override;

        [[nodiscard]] std::unique_ptr<rhi::IShader> create_shader(const rhi::ShaderDesc& desc, rhi::ShaderStage stage) override;

        [[nodiscard]] std::unique_ptr<rhi::IPipeline> create_pipeline(const rhi::PipelineDesc& desc) override;

        [[nodiscard]] std::unique_ptr<rhi::IDescriptorSet> create_descriptor_set(const rhi::DescriptorSetLayout& layout) override;

        [[nodiscard]] std::unique_ptr<rhi::ICommandBuffer> create_command_buffer()                          override;
        [[nodiscard]] std::unique_ptr<rhi::ISwapchain>     create_swapchain(const rhi::SwapchainDesc& desc) override;

        void submit(rhi::ICommandBuffer& cmd) override;
        void wait_idle()                      override;

        // Accesores internos (void* por seguridad del consumidor C++) ----------------------------
        void* mtl_device()        const noexcept { return m_device; }
        void* mtl_command_queue() const noexcept { return m_command_queue; }

        MetalTextureSlot& tex_slot(rhi::TextureHandle h);
        MetalBufferSlot&  buf_slot(rhi::BufferHandle h);

        // Registra una MTLTexture cuya propiedad es externa (p. ej. un drawable de CAMetalLayer). Devuelve un handle que se puede usar como cualquier otro handle de textura.
        rhi::TextureHandle register_external_texture(void* mtl_texture, uint32_t w, uint32_t h);
        void unregister_texture(rhi::TextureHandle h);

    private:
        bool  m_valid         = false;
        void* m_device        = nullptr;            // id<MTLDevice>
        void* m_command_queue = nullptr;            // id<MTLCommandQueue>

        std::vector<MetalTextureSlot> m_textures;
        std::vector<uint32_t>         m_texture_free;
        std::vector<MetalBufferSlot>  m_buffers;
        std::vector<uint32_t>         m_buffer_free;

        // El slot de índice 0 está reservado como handle "nulo" / inválido (id == 0). Los slots empiezan en el índice 1.
        uint32_t alloc_tex_slot();
        void     free_tex_slot(uint32_t idx);
        uint32_t alloc_buf_slot();
        void     free_buf_slot(uint32_t idx);
    };
} // namespace anxiety::rendering::backend::metal

#endif // ANXIETY_BACKEND_METAL
