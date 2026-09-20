#ifdef ANXIETY_BACKEND_VULKAN

#include "VulkanCommandBuffer.h"
#include "VulkanDevice.h"
#include "VulkanPipeline.h"
#include "VulkanDescriptorSet.h"
#include "VulkanHelpers.h"
#include "Logger.h"

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
        m_current_pipeline = nullptr;

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
        // oldLayout debe reflejar el layout real actual de la imagen, no el estado lógico `before`
        // que asume el render graph — las imágenes de swapchain empiezan su vida en
        // VK_IMAGE_LAYOUT_UNDEFINED (a diferencia de D3D12, donde COMMON/PRESENT comparten un único
        // valor de enum y los buffers recién creados ya están en "Present"), así que confiar en
        // `before` tal cual declara mal el barrier en el primer uso de cada imagen por ciclo de
        // swapchain. slot.layout es la fuente de verdad — se actualiza tras cada transición.
        VkImageLayout  old_layout = slot.layout;
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

    // clear_depth_stencil ------------------------------------------------------------------------
    void VulkanCommandBuffer::clear_depth_stencil(rhi::TextureHandle depth, float depth_val, uint8_t stencil) {
        if (!depth.is_valid()) return;

        end_dynamic_rendering_if_active();

        m_pending_depth     = depth;
        m_pending_depth_val = depth_val;
        m_pending_stencil   = stencil;
        m_has_pending_depth = true;
    }

    // Estado de pipeline ---------------------------------------------------------------------------
    void VulkanCommandBuffer::bind_pipeline(anxiety::rendering::rhi::IPipeline& pipeline) {
        auto& p = static_cast<VulkanPipeline&>(pipeline);
        m_current_pipeline = &p;

        vkCmdBindPipeline(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p.pipeline());

        // El viewport/scissor son estado dinámico en todo VulkanPipeline — se fijan a partir del
        // área de renderizado activa (establecida por el clear_render_target() anterior).
        if (m_render_area.extent.width > 0 && m_render_area.extent.height > 0) {
            // Nuestras matrices de proyección (SceneMath::mat4_perspective_LH) se escriben para la
            // convención de D3D12, donde +Y en clip-space es arriba y la transformación de viewport
            // de la API lo invierte a +Y hacia abajo en pantalla. La transformación de viewport de
            // Vulkan NO hace esa inversión por defecto, así que pasar las mismas coordenadas de clip
            // sin modificar renderiza boca abajo — y, como esa inversión también invierte el winding
            // de triángulo que percibe el rasterizador, descarta en silencio todos los triángulos con
            // nuestra configuración CW-front/back-cull (ver frontFace en VulkanPipeline). Un viewport
            // con altura negativa (parte del core desde Vulkan 1.1 / VK_KHR_maintenance1 — exigimos
            // apiVersion 1.2) restaura la inversión al estilo D3D sin tocar la matemática compartida
            // ni el backend DX12.
            VkViewport viewport{};
            viewport.y        = static_cast<float>(m_render_area.extent.height);
            viewport.width    = static_cast<float>(m_render_area.extent.width);
            viewport.height   = -static_cast<float>(m_render_area.extent.height);
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            vkCmdSetViewport(m_cmd, 0, 1, &viewport);
            vkCmdSetScissor(m_cmd, 0, 1, &m_render_area);
        }
    }

    void VulkanCommandBuffer::bind_descriptor_set(uint32_t set, anxiety::rendering::rhi::IDescriptorSet& ds) {
        if (!m_current_pipeline) {
            LOG_WARNING("RHI", "VulkanCommandBuffer::bind_descriptor_set llamado antes que bind_pipeline.");
            return;
        }
        VkDescriptorSet vk_set = static_cast<VulkanDescriptorSet&>(ds).descriptor_set();
        vkCmdBindDescriptorSets(m_cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_current_pipeline->layout(), set, 1, &vk_set, 0, nullptr);
    }

    // Vertex / index buffers -------------------------------------------------------------------------
    void VulkanCommandBuffer::bind_vertex_buffer(uint32_t slot, anxiety::rendering::rhi::BufferHandle handle, uint64_t offset, uint32_t stride) {
        if (!handle.is_valid()) return;
        VkBufferSlot& buf = m_device->buf_slot(handle);
        VkDeviceSize vk_offset = static_cast<VkDeviceSize>(offset);

        // Igual que en las demás APIs, stride == 0 significa "usar el del pipeline".
        const uint32_t pipeline_stride = m_current_pipeline ? m_current_pipeline->vertex_stride() : 0;
        const uint32_t eff_stride      = stride > 0 ? stride : pipeline_stride;

        if (m_device->has_dynamic_vertex_stride()) {
            const VkDeviceSize vk_stride = eff_stride;
            m_device->pfn_cmd_bind_vertex_buffers2(m_cmd, slot, 1, &buf.buffer, &vk_offset, nullptr, &vk_stride);
            return;
        }

        // Sin stride dinámico el stride del pipeline es inamovible: si no coincide, el resultado sería basura.
        if (stride > 0 && pipeline_stride > 0 && stride != pipeline_stride) {
            LOGF_ERROR("VulkanCommandBuffer", "bind_vertex_buffer: stride {} distinto del del pipeline ({}) y el dispositivo no soporta stride dinámico.", stride, pipeline_stride);
        }
        vkCmdBindVertexBuffers(m_cmd, slot, 1, &buf.buffer, &vk_offset);
    }

    void VulkanCommandBuffer::bind_index_buffer(anxiety::rendering::rhi::BufferHandle handle, uint64_t offset, bool use_32_bit) {
        if (!handle.is_valid()) return;
        VkBufferSlot& buf = m_device->buf_slot(handle);
        vkCmdBindIndexBuffer(m_cmd, buf.buffer, static_cast<VkDeviceSize>(offset), use_32_bit ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
    }

    // draw / dispatch ----------------------------------------------------------------------------
    void VulkanCommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
        vkCmdDraw(m_cmd, vertex_count, instance_count, first_vertex, first_instance);
    }

    void VulkanCommandBuffer::draw_indexed(uint32_t index_count, uint32_t instance_count,
        uint32_t first_index, int32_t vertex_offset, uint32_t first_instance) {
        vkCmdDrawIndexed(m_cmd, index_count, instance_count, first_index, vertex_offset, first_instance);
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
