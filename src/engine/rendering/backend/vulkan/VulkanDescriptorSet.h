#pragma once

#ifdef ANXIETY_BACKEND_VULKAN

#include "../../rhi/IDescriptorSet.h"

#include <vulkan/vulkan.h>
#include <vector>

namespace anxiety::rendering::backend::vulkan {
    class VulkanDevice;

    // VulkanDescriptorSet — implementación Vulkan de rhi::IDescriptorSet ---------------------------
    // Asigna un VkDescriptorSet del pool de descriptores compartido del dispositivo, usando el
    // VkDescriptorSetLayout cacheado para el contenido de este layout (ver
    // VulkanDevice::get_or_create_descriptor_set_layout) — el mismo layout que usará un pipeline
    // construido a partir de un DescriptorSetLayout idéntico, así que este set se vincula
    // correctamente contra ese pipeline.
    //
    // Creado por VulkanDevice::create_descriptor_set(). Llama a update() para vincular recursos
    // concretos; VulkanCommandBuffer::bind_descriptor_set() emite la llamada a vkCmdBindDescriptorSets.
    // --------------------------------------------------------------------------------------------
    class VulkanDescriptorSet final : public rhi::IDescriptorSet {
    public:
        VulkanDescriptorSet(VulkanDevice& device, const rhi::DescriptorSetLayout& layout);
        ~VulkanDescriptorSet() override;

        // rhi::IDescriptorSet --------------------------------------------------------------------
        void update(const std::vector<rhi::DescriptorWrite>& writes) override;

        // Interno de Vulkan -------------------------------------------------------------------------
        [[nodiscard]] VkDescriptorSet descriptor_set() const noexcept { return m_set; }

    private:
        VulkanDevice&            m_device;
        VkDescriptorSet          m_set     = VK_NULL_HANDLE;
        rhi::DescriptorSetLayout m_layout;
    };
} // namespace anxiety::rendering::backend::vulkan

#endif // ANXIETY_BACKEND_VULKAN
