#ifdef ANXIETY_BACKEND_VULKAN

#include "VulkanPipeline.h"
#include "VulkanDevice.h"
#include "VulkanShader.h"
#include "VulkanHelpers.h"
#include "Logger.h"

#include <vector>

namespace anxiety::rendering::backend::vulkan {
    static constexpr char k_category[] = "RHI";

    VulkanPipeline::VulkanPipeline(VulkanDevice& device, const rhi::PipelineDesc& desc) : m_device(device) {
        if (desc.debug_name) m_debug_name = desc.debug_name;

        if (!device.has_dynamic_rendering()) {
            LOGF_ERROR(k_category, "VulkanPipeline '{}': dynamic rendering no está disponible en este dispositivo.", m_debug_name);
            return;
        }

        // Descriptor set layout (set 0) — compartido con cualquier llamada a
        // IDevice::create_descriptor_set() hecha contra el mismo contenido de DescriptorSetLayout;
        // incrusta desc.static_samplers como samplers inmutables.
        VkDescriptorSetLayout ds_layout = device.get_or_create_descriptor_set_layout(desc.descriptor_layout, &desc.static_samplers);
        if (ds_layout == VK_NULL_HANDLE) {
            LOGF_ERROR(k_category, "VulkanPipeline '{}': falló la creación del descriptor set layout.", m_debug_name);
            return;
        }

        // Pipeline layout ---------------------------------------------------------------------------
        VkPipelineLayoutCreateInfo layout_ci{};
        layout_ci.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_ci.setLayoutCount = 1;
        layout_ci.pSetLayouts    = &ds_layout;

        if (vkCreatePipelineLayout(device.device(), &layout_ci, nullptr, &m_layout) != VK_SUCCESS) {
            LOGF_ERROR(k_category, "VulkanPipeline '{}': vkCreatePipelineLayout falló.", m_debug_name);
            return;
        }

        // Etapas de shader ---------------------------------------------------------------------------
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        if (desc.vertex_shader) {
            const auto& vs = static_cast<const VulkanShader&>(*desc.vertex_shader);
            VkPipelineShaderStageCreateInfo si{};
            si.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            si.stage  = VK_SHADER_STAGE_VERTEX_BIT;
            si.module = vs.module();
            si.pName  = vs.entry_point().data();
            stages.push_back(si);
        }
        if (desc.fragment_shader) {
            const auto& ps = static_cast<const VulkanShader&>(*desc.fragment_shader);
            VkPipelineShaderStageCreateInfo si{};
            si.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            si.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
            si.module = ps.module();
            si.pName  = ps.entry_point().data();
            stages.push_back(si);
        }

        // Entrada de vértices ------------------------------------------------------------------------
        VkVertexInputBindingDescription binding{};
        binding.binding   = 0;
        binding.stride    = desc.vertex_layout.stride_bytes;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attrs;
        attrs.reserve(desc.vertex_layout.attributes.size());
        for (uint32_t i = 0; i < desc.vertex_layout.attributes.size(); ++i) {
            const auto& a = desc.vertex_layout.attributes[i];
            VkVertexInputAttributeDescription attr{};
            attr.location = i;
            attr.binding  = a.input_slot;
            attr.format   = to_vk_vertex_format(a.format);
            attr.offset   = a.byte_offset;
            attrs.push_back(attr);
        }

        VkPipelineVertexInputStateCreateInfo vertex_input_ci{};
        vertex_input_ci.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_ci.vertexBindingDescriptionCount   = desc.vertex_layout.stride_bytes > 0 ? 1 : 0;
        vertex_input_ci.pVertexBindingDescriptions      = desc.vertex_layout.stride_bytes > 0 ? &binding : nullptr;
        vertex_input_ci.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
        vertex_input_ci.pVertexAttributeDescriptions    = attrs.empty() ? nullptr : attrs.data();

        // Input assembly ------------------------------------------------------------------------------
        VkPipelineInputAssemblyStateCreateInfo ia_ci{};
        ia_ci.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia_ci.topology = to_vk_primitive_topology(desc.topology);

        // Viewport / scissor — fijados dinámicamente por VulkanCommandBuffer::bind_pipeline(). -----
        VkPipelineViewportStateCreateInfo viewport_ci{};
        viewport_ci.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_ci.viewportCount = 1;
        viewport_ci.scissorCount  = 1;

        // Rasterizador -----------------------------------------------------------------------------
        VkPipelineRasterizationStateCreateInfo raster_ci{};
        raster_ci.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster_ci.polygonMode = to_vk_polygon_mode(desc.rasterizer.fill_mode);
        raster_ci.cullMode    = to_vk_cull_mode(desc.rasterizer.cull_mode);
        raster_ci.frontFace   = desc.rasterizer.front_face_CCW ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
        raster_ci.lineWidth   = 1.f;

        // Multisample -------------------------------------------------------------------------------
        VkPipelineMultisampleStateCreateInfo ms_ci{};
        ms_ci.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // Profundidad / stencil -----------------------------------------------------------------------
        VkPipelineDepthStencilStateCreateInfo ds_ci{};
        ds_ci.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds_ci.depthTestEnable  = desc.depth_stencil.depth_test_enable  ? VK_TRUE : VK_FALSE;
        ds_ci.depthWriteEnable = desc.depth_stencil.depth_write_enable ? VK_TRUE : VK_FALSE;
        ds_ci.depthCompareOp   = to_vk_compare_op(desc.depth_stencil.depth_compare_op);
        ds_ci.minDepthBounds   = 0.f;
        ds_ci.maxDepthBounds   = 1.f;

        // Blend ---------------------------------------------------------------------------------------
        VkPipelineColorBlendAttachmentState blend_attachment{};
        blend_attachment.blendEnable         = desc.blend.blend_enable ? VK_TRUE : VK_FALSE;
        blend_attachment.srcColorBlendFactor = to_vk_blend_factor(desc.blend.src_color_factor);
        blend_attachment.dstColorBlendFactor = to_vk_blend_factor(desc.blend.dst_color_factor);
        blend_attachment.colorBlendOp        = to_vk_blend_op(desc.blend.color_blend_op);
        blend_attachment.srcAlphaBlendFactor = to_vk_blend_factor(desc.blend.src_alpha_factor);
        blend_attachment.dstAlphaBlendFactor = to_vk_blend_factor(desc.blend.dst_alpha_factor);
        blend_attachment.alphaBlendOp        = to_vk_blend_op(desc.blend.alpha_blend_op);
        blend_attachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo blend_ci{};
        blend_ci.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend_ci.attachmentCount = desc.render_target_fmts.empty() ? 0 : 1;
        blend_ci.pAttachments    = desc.render_target_fmts.empty() ? nullptr : &blend_attachment;

        // Estado dinámico ---------------------------------------------------------------------------
        VkDynamicState dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamic_ci{};
        dynamic_ci.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_ci.dynamicStateCount = static_cast<uint32_t>(std::size(dynamic_states));
        dynamic_ci.pDynamicStates    = dynamic_states;

        // Formatos de adjuntos para dynamic rendering --------------------------------------------------
        std::vector<VkFormat> color_formats;
        color_formats.reserve(desc.render_target_fmts.size());
        for (auto fmt : desc.render_target_fmts) color_formats.push_back(to_vk_format(fmt));

        VkPipelineRenderingCreateInfoKHR rendering_ci{};
        rendering_ci.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        rendering_ci.colorAttachmentCount    = static_cast<uint32_t>(color_formats.size());
        rendering_ci.pColorAttachmentFormats = color_formats.empty() ? nullptr : color_formats.data();
        rendering_ci.depthAttachmentFormat   = desc.depth_stencil.depth_format != anxiety::rendering::rhi::Format::Unknown
                                              ? to_vk_format(desc.depth_stencil.depth_format)
                                              : VK_FORMAT_UNDEFINED;

        // Ensamblaje --------------------------------------------------------------------------------
        VkGraphicsPipelineCreateInfo pipeline_ci{};
        pipeline_ci.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_ci.pNext               = &rendering_ci;
        pipeline_ci.stageCount          = static_cast<uint32_t>(stages.size());
        pipeline_ci.pStages             = stages.empty() ? nullptr : stages.data();
        pipeline_ci.pVertexInputState   = &vertex_input_ci;
        pipeline_ci.pInputAssemblyState = &ia_ci;
        pipeline_ci.pViewportState      = &viewport_ci;
        pipeline_ci.pRasterizationState = &raster_ci;
        pipeline_ci.pMultisampleState   = &ms_ci;
        pipeline_ci.pDepthStencilState  = &ds_ci;
        pipeline_ci.pColorBlendState    = &blend_ci;
        pipeline_ci.pDynamicState       = &dynamic_ci;
        pipeline_ci.layout              = m_layout;

        VkResult res = vkCreateGraphicsPipelines(device.device(), device.pipeline_cache(), 1, &pipeline_ci, nullptr, &m_pipeline);
        if (res != VK_SUCCESS) {
            LOGF_ERROR(k_category, "VulkanPipeline '{}': vkCreateGraphicsPipelines falló (VkResult={}).", m_debug_name, static_cast<int>(res));
            vkDestroyPipelineLayout(device.device(), m_layout, nullptr);
            m_layout = VK_NULL_HANDLE;
        }
    }

    VulkanPipeline::~VulkanPipeline() {
        if (m_pipeline) vkDestroyPipeline(m_device.device(), m_pipeline, nullptr);
        if (m_layout)   vkDestroyPipelineLayout(m_device.device(), m_layout, nullptr);
        // El descriptor set layout del set 0 es propiedad de la caché de VulkanDevice (compartido
        // entre pipelines/descriptor sets construidos a partir del mismo contenido de layout) — no se destruye aquí.
    }
} // namespace anxiety::rendering::backend::vulkan

#endif // ANXIETY_BACKEND_VULKAN
