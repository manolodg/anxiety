#ifdef ANXIETY_BACKEND_VULKAN

#include "VulkanDescriptorSet.h"
#include "VulkanDevice.h"
#include "VulkanHelpers.h"
#include "Logger.h"

namespace anxiety::rendering::backend::vulkan {
    static constexpr char k_category[] = "RHI";

    VulkanDescriptorSet::VulkanDescriptorSet(VulkanDevice& device, const rhi::DescriptorSetLayout& layout) : m_device(device), m_layout(layout) {
        VkDescriptorSetLayout vk_layout = device.get_or_create_descriptor_set_layout(layout, nullptr);
        if (vk_layout == VK_NULL_HANDLE) {
            LOG_ERROR(k_category, "VulkanDescriptorSet: falló la creación del descriptor set layout.");
            return;
        }

        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = device.descriptor_pool();
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &vk_layout;

        VkResult res = vkAllocateDescriptorSets(device.device(), &ai, &m_set);
        if (res != VK_SUCCESS) {
            LOGF_ERROR(k_category, "VulkanDescriptorSet: vkAllocateDescriptorSets falló (VkResult={}).", static_cast<int>(res));
            m_set = VK_NULL_HANDLE;
        }
    }

    VulkanDescriptorSet::~VulkanDescriptorSet() {
        if (m_set) vkFreeDescriptorSets(m_device.device(), m_device.descriptor_pool(), 1, &m_set);
    }

    void VulkanDescriptorSet::update(const std::vector<rhi::DescriptorWrite>& writes) {
        if (!m_set) return;

        std::vector<VkWriteDescriptorSet>   vk_writes;
        std::vector<VkDescriptorBufferInfo> buf_infos;
        std::vector<VkDescriptorImageInfo>  img_infos;
        vk_writes.reserve(writes.size());
        buf_infos.reserve(writes.size());
        img_infos.reserve(writes.size());

        for (const auto& w : writes) {
            VkWriteDescriptorSet vkw{};
            vkw.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            vkw.dstSet          = m_set;
            vkw.dstBinding      = to_vk_binding_index(w.type, w.binding);
            vkw.descriptorCount = 1;
            vkw.descriptorType  = to_vk_descriptor_type(w.type);

            if (w.type == rhi::DescriptorType::UniformBuffer || w.type == rhi::DescriptorType::StorageBuffer) {
                if (!w.buffer.is_valid()) {
                    LOGF_WARNING(k_category, "VulkanDescriptorSet::update: el binding {} espera un buffer pero no se dio ninguno.", w.binding);
                    continue;
                }
                const VkBufferSlot& slot = m_device.buf_slot(w.buffer);
                VkDescriptorBufferInfo bi{};
                bi.buffer = slot.buffer;
                bi.range  = slot.size;
                buf_infos.push_back(bi);
                vkw.pBufferInfo = &buf_infos.back();
            } else {
                if (!w.texture.is_valid()) {
                    LOGF_WARNING(k_category, "VulkanDescriptorSet::update: el binding {} espera una textura pero no se dio ninguna.", w.binding);
                    continue;
                }
                const VkTextureSlot& slot = m_device.tex_slot(w.texture);
                VkDescriptorImageInfo ii{};
                ii.imageView   = slot.view;
                ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                img_infos.push_back(ii);
                vkw.pImageInfo = &img_infos.back();
            }

            vk_writes.push_back(vkw);
        }

        if (!vk_writes.empty()) vkUpdateDescriptorSets(m_device.device(), static_cast<uint32_t>(vk_writes.size()), vk_writes.data(), 0, nullptr);
    }
} // namespace anxiety::rendering::backend::vulkan

#endif // ANXIETY_BACKEND_VULKAN
