#ifdef ANXIETY_BACKEND_VULKAN

#include "VulkanCommandBuffer.h"
#include "VulkanDevice.h"
#include "VulkanHelpers.h"

#include <cassert>
#include <cstdio>

namespace anxiety::rendering::backend::vulkan {
    // Construcción / destrucción -------------------------------------------------------------------
    VulkanCommandBuffer::VulkanCommandBuffer(VulkanDevice& device) : m_device(&device) {
        VkCommandBufferAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool        = device.command_pool();
        ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        vkAllocateCommandBuffers(device.device(), &ai, &m_cmd);
    }

    VulkanCommandBuffer::~VulkanCommandBuffer() {
        if (m_cmd) vkFreeCommandBuffers(m_device->device(), m_device->command_pool(), 1, &m_cmd);
    }

    // begin / end --------------------------------------------------------------------------------
    void VulkanCommandBuffer::begin() {
        m_has_pending_RT    = false;
        m_has_pending_depth = false;
        m_in_render_pass    = false;

        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(m_cmd, &bi);
    }

    void VulkanCommandBuffer::end() {
        end_dynamic_rendering_if_active();
        vkEndCommandBuffer(m_cmd);
    }

    // resource_barrier — transición del layout de imagen -------------------------------------------
    void VulkanCommandBuffer::resource_barrier(rhi::TextureHandle texture, rhi::ResourceState before, rhi::ResourceState after) {
        end_dynamic_rendering_if_active();

        if (!texture.is_valid()) return;

        VkTextureSlot& slot       = m_device->tex_slot(texture);
        VkImageLayout  old_layout = to_vk_image_layout(before);
        VkImageLayout  new_layout = to_vk_image_layout(after);

        if (old_layout == new_layout) return;

        VkImageAspectFlags aspect = slot.is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        if (slot.format == VK_FORMAT_D24_UNORM_S8_UINT || slot.format == VK_FORMAT_D32_SFLOAT_S8_UINT) aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = old_layout;
        barrier.newLayout                       = new_layout;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = slot.image;
        barrier.subresourceRange.aspectMask     = aspect;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
        barrier.srcAccessMask                   = to_vk_access_flags(before);
        barrier.dstAccessMask                   = to_vk_access_flags(after);

        vkCmdPipelineBarrier(m_cmd, to_vk_stage_flags(before), to_vk_stage_flags(after), 0, 0, nullptr, 0, nullptr, 1, &barrier);

        slot.layout = new_layout;
    }

    // clear_render_target ------------------------------------------------------------------------
    void VulkanCommandBuffer::clear_render_target(rhi::TextureHandle rt, const rhi::ClearColor& color) {
        if (!rt.is_valid()) return;

        end_dynamic_rendering_if_active();

        m_pending_RT     = rt;
        m_pending_clear  = color;
        m_has_pending_RT = true;

        VkTextureSlot& slot = m_device->tex_slot(rt);
        if (slot.layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            VkImageMemoryBarrier barrier{};
            barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout                       = slot.layout;
            barrier.newLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            barrier.image                           = slot.image;
            barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel   = 0;
            barrier.subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;
            barrier.srcAccessMask                   = VK_ACCESS_MEMORY_READ_BIT;
            barrier.dstAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            vkCmdPipelineBarrier(m_cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            slot.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }

        m_render_area = { {0, 0}, { slot.width, slot.height } };

        begin_dynamic_rendering();
    }

    // draw / dispatch ----------------------------------------------------------------------------
    void VulkanCommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
        vkCmdDraw(m_cmd, vertex_count, instance_count, first_vertex, first_instance);
    }

    void VulkanCommandBuffer::dispatch(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) {
        vkCmdDispatch(m_cmd, groups_x, groups_y, groups_z);
    }

    // Helpers de dynamic rendering ------------------------------------------------------------------
    void VulkanCommandBuffer::begin_dynamic_rendering() {
        if (!m_device->has_dynamic_rendering()) {
            std::fprintf(stderr, "[VulkanCommandBuffer] El dynamic rendering no está disponible.\n");
            return;
        }

        VkRenderingAttachmentInfoKHR color_attachment{};
        color_attachment.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        if (m_has_pending_RT) {
            VkTextureSlot& slot = m_device->tex_slot(m_pending_RT);
            color_attachment.imageView        = slot.view;
            color_attachment.loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color_attachment.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
            color_attachment.clearValue.color = {
                m_pending_clear.r, m_pending_clear.g,
                m_pending_clear.b, m_pending_clear.a
            };
        }

        VkRenderingAttachmentInfoKHR depth_attachment{};
        depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        bool has_depth = false;
        if (m_has_pending_depth) {
            VkTextureSlot& d_slot = m_device->tex_slot(m_pending_depth);
            depth_attachment.imageView               = d_slot.view;
            depth_attachment.imageLayout             = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depth_attachment.loadOp                  = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth_attachment.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
            depth_attachment.clearValue.depthStencil = { m_pending_depth_val, m_pending_stencil };
            has_depth = true;
        }

        VkRenderingInfoKHR rendering_info{};
        rendering_info.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        rendering_info.renderArea           = m_render_area;
        rendering_info.layerCount           = 1;
        rendering_info.colorAttachmentCount = m_has_pending_RT ? 1 : 0;
        rendering_info.pColorAttachments    = m_has_pending_RT ? &color_attachment : nullptr;
        rendering_info.pDepthAttachment     = has_depth ? &depth_attachment : nullptr;
        rendering_info.pStencilAttachment   = nullptr;

        m_device->pfn_cmd_begin_rendering(m_cmd, &rendering_info);
        m_in_render_pass = true;
    }

    void VulkanCommandBuffer::end_dynamic_rendering_if_active() {
        if (!m_in_render_pass) return;
        if (m_device->has_dynamic_rendering() && m_device->pfn_cmd_end_rendering) m_device->pfn_cmd_end_rendering(m_cmd);
        
        m_in_render_pass    = false;
        m_has_pending_RT    = false;
        m_has_pending_depth = false;
    }

} // namespace anxiety::rendering::backend::vulkan

#endif // ANXIETY_BACKEND_VULKAN
