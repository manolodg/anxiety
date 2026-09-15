#pragma once

#ifdef ANXIETY_BACKEND_VULKAN

#include "../../rhi/IDevice.h"

#include <vulkan/vulkan.h>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>

namespace anxiety::rendering::backend::vulkan {
    // VkTextureSlot — una entrada en el pool de recursos de texturas del dispositivo -------------
    struct VkTextureSlot {
        VkImage        image    = VK_NULL_HANDLE;
        VkImageView    view     = VK_NULL_HANDLE;
        VkDeviceMemory memory   = VK_NULL_HANDLE;
        VkFormat       format   = VK_FORMAT_UNDEFINED;
        VkImageLayout  layout   = VK_IMAGE_LAYOUT_UNDEFINED;
        uint32_t       width    = 0;
        uint32_t       height   = 0;
        bool           is_depth = false;
        bool           external = false;
        bool           alive    = false;
    };

    // VkBufferSlot — una entrada en el pool de recursos de buffers del dispositivo ----------------
    struct VkBufferSlot {
        VkBuffer       buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize   size   = 0;
        void*          mapped = nullptr;
        bool           alive  = false;
    };

    // VulkanDevice — implementación Vulkan de rhi::IDevice ---------------------------------------
    class VulkanDevice final : public rhi::IDevice {
    public:
        explicit VulkanDevice(bool enable_validation = false);
        ~VulkanDevice() override;

        [[nodiscard]] bool is_valid() const noexcept { return m_valid; }

        // rhi::IDevice ---------------------------------------------------------------------------
        [[nodiscard]] std::string_view backend_name() const noexcept override { return "Vulkan"; }

        [[nodiscard]] rhi::BufferHandle  create_buffer(const rhi::BufferDesc&)   override;
        [[nodiscard]] rhi::TextureHandle create_texture(const rhi::TextureDesc&) override;
        void destroy_buffer(rhi::BufferHandle)   override;
        void destroy_texture(rhi::TextureHandle) override;

        [[nodiscard]] std::unique_ptr<rhi::ICommandBuffer> create_command_buffer()                      override;
        [[nodiscard]] std::unique_ptr<rhi::ISwapchain>     create_swapchain(const rhi::SwapchainDesc&)  override;

        void submit(rhi::ICommandBuffer&) override;
        void wait_idle()                  override;

        // Operaciones de recursos extendidas (no están en IDevice — accesibles vía el tipo concreto)
        [[nodiscard]] rhi::BufferHandle create_buffer(const rhi::BufferDesc& desc, const void* initial_data, size_t initial_data_sz);

        void write_buffer(rhi::BufferHandle handle, const void* data, size_t offset, size_t size);
        void upload_texture_data(rhi::TextureHandle handle, const uint8_t* rgba8, uint32_t width, uint32_t height);

        // Accesores internos (usados por command buffers, pipelines, swapchain) ------------------
        [[nodiscard]] VkDevice         device()                const noexcept { return m_device; }
        [[nodiscard]] VkPhysicalDevice phys_device()           const noexcept { return m_phys_device; }
        [[nodiscard]] VkQueue          graphics_queue()        const noexcept { return m_graphics_queue; }
        [[nodiscard]] uint32_t         graphics_family()       const noexcept { return m_graphics_family; }
        [[nodiscard]] VkCommandPool    command_pool()          const noexcept { return m_command_pool; }
        [[nodiscard]] VkDescriptorPool descriptor_pool()       const noexcept { return m_desc_pool; }
        [[nodiscard]] VkInstance       instance()              const noexcept { return m_instance; }
        [[nodiscard]] VkPipelineCache  pipeline_cache()        const noexcept { return m_pipeline_cache; }
        [[nodiscard]] bool             has_dynamic_rendering() const noexcept { return m_has_dynamic_rendering; }

        // Punteros a función de dynamic rendering (extensión KHR o núcleo de Vulkan 1.3)
        PFN_vkCmdBeginRenderingKHR pfn_cmd_begin_rendering = nullptr;
        PFN_vkCmdEndRenderingKHR   pfn_cmd_end_rendering   = nullptr;

        // Accesores del pool de recursos ----------------------------------------------------------
        [[nodiscard]]       VkTextureSlot& tex_slot(rhi::TextureHandle h);
        [[nodiscard]] const VkTextureSlot& tex_slot(rhi::TextureHandle h) const;
        [[nodiscard]]       VkBufferSlot&  buf_slot(rhi::BufferHandle  h);
        [[nodiscard]] const VkBufferSlot&  buf_slot(rhi::BufferHandle  h) const;

        // Registra una imagen de propiedad externa (backbuffer del swapchain) --------------------
        [[nodiscard]] rhi::TextureHandle register_external_image(VkImage image, VkImageView view, VkFormat format, uint32_t width, uint32_t height);
        void          unregister_texture(rhi::TextureHandle handle);

        // Helpers de command buffer de un solo uso ------------------------------------------------
        [[nodiscard]] VkCommandBuffer begin_one_shot();
        void                          end_one_shot(VkCommandBuffer cmd);

        // Canal lateral de semáforos del swapchain ------------------------------------------------
        void set_wait_semaphore(VkSemaphore sem)   noexcept { m_wait_semaphore = sem; }
        void set_signal_semaphore(VkSemaphore sem) noexcept { m_signal_semaphore = sem; }

        // Helper de memoria ------------------------------------------------------------------------
        [[nodiscard]] uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags props) const;

    private:
        // Pasos de inicialización ------------------------------------------------------------------
        bool create_instance(bool enable_validation);
        bool setup_debug_messenger();
        bool select_physical_device();
        bool create_logical_device();
        bool create_command_pool();
        bool create_descriptor_pool();
        bool create_pipeline_cache();
        bool create_idle_fence();

        // Gestión del pool de recursos --------------------------------------------------------------
        [[nodiscard]] uint32_t allocate_texture_slot();
        void                   free_texture_slot(uint32_t slot);
        [[nodiscard]] uint32_t allocate_buffer_slot();
        void                   free_buffer_slot(uint32_t slot);

        // Helpers internos de buffer/imagen ---------------------------------------------------------
        void create_vk_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& out_buf, VkDeviceMemory& out_mem);
        void transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect, VkImageLayout old_layout, VkImageLayout new_layout);

        // Callback de depuración estático -----------------------------------------------------------
        static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user_data);

        // Estado -------------------------------------------------------------------------------------
        VkInstance               m_instance        = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_messenger       = VK_NULL_HANDLE;
        VkPhysicalDevice         m_phys_device     = VK_NULL_HANDLE;
        VkDevice                 m_device          = VK_NULL_HANDLE;
        VkQueue                  m_graphics_queue  = VK_NULL_HANDLE;
        uint32_t                 m_graphics_family = 0;
        VkCommandPool            m_command_pool    = VK_NULL_HANDLE;
        VkDescriptorPool         m_desc_pool       = VK_NULL_HANDLE;
        VkPipelineCache          m_pipeline_cache  = VK_NULL_HANDLE;
        VkFence                  m_idle_fence      = VK_NULL_HANDLE;

        VkSemaphore              m_wait_semaphore   = VK_NULL_HANDLE;
        VkSemaphore              m_signal_semaphore = VK_NULL_HANDLE;

        bool m_has_dynamic_rendering = false;
        bool m_valid                 = false;

        std::vector<VkTextureSlot> m_textures;
        std::vector<uint32_t>      m_texture_free;
        std::vector<VkBufferSlot>  m_buffers;
        std::vector<uint32_t>      m_buffer_free;
    };
} // namespace anxiety::rendering::backend::vulkan

#endif // ANXIETY_BACKEND_VULKAN
