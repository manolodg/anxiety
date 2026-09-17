#pragma once

#ifdef ANXIETY_BACKEND_VULKAN

#include "../../rhi/ShaderTypes.h"

#include <vulkan/vulkan.h>
#include <string>
#include <string_view>

namespace anxiety::rendering::backend::vulkan {
    // VulkanShader — implementación Vulkan de rhi::IShader ------------------------------------------
    // Envuelve un VkShaderModule construido a partir de bytecode SPIR-V (ver
    // VulkanDevice::compile_shader_from_source). El módulo se destruye cuando se elimina el objeto
    // VulkanShader.
    // --------------------------------------------------------------------------------------------
    class VulkanShader final : public rhi::IShader {
    public:
        VulkanShader(VkDevice device, VkShaderModule module, rhi::ShaderStage stage, const char* entry_point);
        ~VulkanShader() override;

        // rhi::IShader ---------------------------------------------------------------------------
        [[nodiscard]] std::string_view entry_point() const noexcept override { return m_entry_point; }
        [[nodiscard]] rhi::ShaderStage stage()       const noexcept override { return m_stage; }

        // Accesor Vulkan ---------------------------------------------------------------------------
        [[nodiscard]] VkShaderModule module()   const noexcept { return m_module; }
        [[nodiscard]] bool           is_valid() const noexcept { return m_module != VK_NULL_HANDLE; }

    private:
        VkDevice         m_device = VK_NULL_HANDLE;
        VkShaderModule   m_module = VK_NULL_HANDLE;
        rhi::ShaderStage m_stage;
        std::string      m_entry_point;
    };
} // namespace anxiety::rendering::backend::vulkan

#endif // ANXIETY_BACKEND_VULKAN
