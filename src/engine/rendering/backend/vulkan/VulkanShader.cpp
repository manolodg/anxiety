#ifdef ANXIETY_BACKEND_VULKAN

#include "VulkanShader.h"

namespace anxiety::rendering::backend::vulkan {
    VulkanShader::VulkanShader(VkDevice device, VkShaderModule module, rhi::ShaderStage stage, const char* entry_point) : m_device(device), m_module(module), m_stage(stage), m_entry_point(entry_point ? entry_point : "main") {}

    VulkanShader::~VulkanShader() {
        if (m_module && m_device) vkDestroyShaderModule(m_device, m_module, nullptr);
    }
} // namespace anxiety::rendering::backend::vulkan

#endif // ANXIETY_BACKEND_VULKAN
