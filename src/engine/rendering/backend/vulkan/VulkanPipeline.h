#pragma once

#ifdef ANXIETY_BACKEND_VULKAN

#include "../../rhi/IPipeline.h"
#include "../../rhi/PipelineDesc.h"

#include <vulkan/vulkan.h>
#include <string>

namespace anxiety::rendering::backend::vulkan {
    class VulkanDevice;

    // VulkanPipeline — implementación Vulkan de rhi::IPipeline --------------------------------------
    // Es propietario del VkPipeline y del VkPipelineLayout. El VkDescriptorSetLayout usado en el set
    // 0 es compartido (propiedad de la caché de descriptor-set-layout de VulkanDevice — ver
    // VulkanDevice::get_or_create_descriptor_set_layout) para que los descriptor sets creados de
    // forma independiente vía IDevice::create_descriptor_set() se vinculen correctamente contra este
    // pipeline. Construido para dynamic rendering (VK_KHR_dynamic_rendering) — no se crea ningún
    // VkRenderPass/VkFramebuffer.
    // --------------------------------------------------------------------------------------------
    class VulkanPipeline final : public rhi::IPipeline {
    public:
        VulkanPipeline(VulkanDevice& device, const rhi::PipelineDesc& desc);
        ~VulkanPipeline() override;

        // rhi::IPipeline ---------------------------------------------------------------------------
        [[nodiscard]] std::string_view debug_name() const noexcept override { return m_debug_name; }

        // Accesores Vulkan (usados por VulkanCommandBuffer) ---------------------------------------
        [[nodiscard]] bool                   is_valid() const noexcept { return m_pipeline != VK_NULL_HANDLE; }
        [[nodiscard]] VkPipeline             pipeline() const noexcept { return m_pipeline; }
        [[nodiscard]] VkPipelineLayout       layout()   const noexcept { return m_layout; }
        // Stride de vértices declarado en el layout del pipeline (0 si no hay entrada de vértices).
        [[nodiscard]] uint32_t               vertex_stride() const noexcept { return m_vertex_stride; }

    private:
        VulkanDevice&    m_device;

        VkPipeline       m_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout m_layout   = VK_NULL_HANDLE;
        std::string      m_debug_name;
        uint32_t         m_vertex_stride = 0;
    };
} // namespace anxiety::rendering::backend::vulkan

#endif // ANXIETY_BACKEND_VULKAN
