#pragma once

#ifdef ANXIETY_BACKEND_VULKAN

#include "../../rhi/ICommandBuffer.h"

#include <vulkan/vulkan.h>
#include <cstdint>

namespace anxiety::rendering::backend::vulkan {

    class VulkanDevice;
    class VulkanPipeline;

    // VulkanCommandBuffer — implementación Vulkan de rhi::ICommandBuffer -------------------------
    // Envuelve un VkCommandBuffer asignado del command pool de gráficos del dispositivo. El buffer
    // se resetea al principio de cada llamada a begin().
    //
    // Ruta de dynamic rendering (VK_KHR_dynamic_rendering / Vulkan 1.3):
    //   clear_render_target() guarda el RT pendiente + el color de limpieza. bind_pipeline() inicia
    //   la pasada de dynamic rendering con un load op de tipo CLEAR. end() finaliza la pasada de
    //   dynamic rendering si hay una activa.
    // --------------------------------------------------------------------------------------------
    class VulkanCommandBuffer final : public rhi::ICommandBuffer {
    public:
        explicit VulkanCommandBuffer(VulkanDevice& device);
        ~VulkanCommandBuffer() override;

        // rhi::ICommandBuffer --------------------------------------------------------------------
        void begin() override;
        void end()   override;

        void resource_barrier(rhi::TextureHandle texture, rhi::ResourceState before, rhi::ResourceState after)                override;
        void clear_render_target(rhi::TextureHandle rt, const rhi::ClearColor& color)                                         override;
        void clear_depth_stencil(rhi::TextureHandle depth, float depth_val, uint8_t stencil)                                  override;

        void bind_pipeline(rhi::IPipeline&)                                                                                   override;
        void bind_descriptor_set(uint32_t set, rhi::IDescriptorSet&)                                                          override;

        void bind_vertex_buffer(uint32_t slot, rhi::BufferHandle handle, uint64_t offset = 0, uint32_t stride = 0)            override;
        void bind_index_buffer(rhi::BufferHandle handle, uint64_t offset = 0, bool use_32_bit = true)                         override;

        void draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0)                                  override;
        void draw_indexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0, int32_t vertex_offset = 0, uint32_t first_instance = 0) override;

        void dispatch(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z)                                                override;

        // Accesor interno --------------------------------------------------------------------------
        [[nodiscard]] VkCommandBuffer vk_cmd() const noexcept { return m_cmd; }

    private:
        VulkanDevice* m_device = nullptr;
        VkCommandBuffer                        m_cmd = VK_NULL_HANDLE;

        // Estado pendiente del render target de color (lo fija clear_render_target, lo consume begin_dynamic_rendering)
        bool               m_has_pending_RT    = false;
        bool               m_in_render_pass    = false;
        rhi::TextureHandle m_pending_RT        = {};
        rhi::ClearColor    m_pending_clear     = {};

        // Estado pendiente de profundidad/stencil
        bool               m_has_pending_depth = false;
        rhi::TextureHandle m_pending_depth     = {};
        float              m_pending_depth_val = 1.f;
        uint32_t           m_pending_stencil   = 0;

        // Área de renderizado de la pasada activa (se fija a partir de las dimensiones del RT)
        VkRect2D           m_render_area       = {};

        // Pipeline vinculado (hace falta su VkPipelineLayout al vincular descriptor sets)
        VulkanPipeline*    m_current_pipeline  = nullptr;

        void begin_dynamic_rendering();
        void end_dynamic_rendering_if_active();
    };
} // namespace anxiety::rendering::backend::vulkan

#endif // ANXIETY_BACKEND_VULKAN
