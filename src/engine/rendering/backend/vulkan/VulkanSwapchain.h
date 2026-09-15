#pragma once

#ifdef ANXIETY_BACKEND_VULKAN

#include "../../rhi/ISwapchain.h"
#include "../../rhi/RHITypes.h"

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

namespace anxiety::rendering::backend::vulkan {

    class VulkanDevice;

    // VulkanSwapchain — implementación Vulkan de rhi::ISwapchain ---------------------------------
    // Crea un VkSurfaceKHR y un VkSwapchainKHR a partir del handle de ventana nativo almacenado en
    // SwapchainDesc::native_window_handle.
    //
    // Los VkImage de backbuffer se registran en VulkanDevice como texturas externas para que los
    // command buffers puedan usarlos a través de TextureHandle.
    // --------------------------------------------------------------------------------------------
    class VulkanSwapchain final : public rhi::ISwapchain {
    public:
        VulkanSwapchain(VulkanDevice& device, const rhi::SwapchainDesc& desc);
        ~VulkanSwapchain() override;

        // rhi::ISwapchain ------------------------------------------------------------------------
        [[nodiscard]] uint32_t           image_count()        const noexcept override { return m_image_count; }
        [[nodiscard]] rhi::Extent2D      extent()             const noexcept override { return m_extent; }
        [[nodiscard]] rhi::Format        format()             const noexcept override { return m_format; }
        [[nodiscard]] rhi::TextureHandle current_backbuffer() const noexcept override;

        uint32_t acquire_next_image()             override;
        void     present()                        override;
        void     resize(rhi::Extent2D new_extent) override;

        // Accesores internos -----------------------------------------------------------------------
        [[nodiscard]] VkSwapchainKHR swapchain() const noexcept { return m_swapchain; }
        [[nodiscard]] VkSurfaceKHR   surface()   const noexcept { return m_surface; }

    private:
        static constexpr uint32_t k_max_images = 4;

        VulkanDevice& m_device;

        VkSurfaceKHR         m_surface   = VK_NULL_HANDLE;
        VkSwapchainKHR       m_swapchain = VK_NULL_HANDLE;

        VkImage              m_images[k_max_images]  = {};
        VkImageView          m_views[k_max_images]   = {};
        rhi::TextureHandle   m_handles[k_max_images] = {};

        VkSemaphore          m_image_available = VK_NULL_HANDLE;
        VkSemaphore          m_render_finished = VK_NULL_HANDLE;

        uint32_t             m_current_image = 0;
        uint32_t             m_image_count   = 0;
        rhi::Extent2D        m_extent        = {};
        rhi::Format          m_format        = anxiety::rendering::rhi::Format::Unknown;
        bool                 m_vsync         = true;

        void create_surface(const rhi::SwapchainDesc& desc);
        void create_swapchain(const rhi::SwapchainDesc& desc);
        void acquire_images();
        void destroy_swapchain_resources();
    };

} // namespace anxiety::rendering::backend::vulkan

#endif // ANXIETY_BACKEND_VULKAN
