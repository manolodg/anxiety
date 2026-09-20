#ifdef ANXIETY_BACKEND_VULKAN

#include "VulkanDevice.h"
#include "VulkanCommandBuffer.h"
#include "VulkanSwapchain.h"
#include "VulkanShader.h"
#include "VulkanPipeline.h"
#include "VulkanDescriptorSet.h"
#include "VulkanHelpers.h"
#include "../../shader/HlslCompiler.h"
#include "Logger.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>


namespace anxiety::rendering::backend::vulkan {

    // Construcción / destrucción -----------------------------------------------------------------
    VulkanDevice::VulkanDevice(bool enable_validation) {
        if (!create_instance(enable_validation)) { return; }
        if (enable_validation)                   { setup_debug_messenger(); }
        if (!select_physical_device())           { return; }
        if (!create_logical_device())            { return; }
        if (!create_command_pool())              { return; }
        if (!create_descriptor_pool())           { return; }
        if (!create_pipeline_cache())            { return; }
        if (!create_idle_fence())                { return; }
        m_valid = true;
    }

    VulkanDevice::~VulkanDevice() {
        if (m_device) {
            vkDeviceWaitIdle(m_device);

            for (auto& slot : m_buffers) {
                if (!slot.alive) continue;
                if (slot.mapped) vkUnmapMemory(m_device, slot.memory);
                vkDestroyBuffer(m_device, slot.buffer, nullptr);
                vkFreeMemory(m_device, slot.memory, nullptr);
            }

            for (auto& slot : m_textures) {
                if (!slot.alive || slot.external) continue;
                vkDestroyImageView(m_device, slot.view, nullptr);
                vkDestroyImage(m_device, slot.image, nullptr);
                vkFreeMemory(m_device, slot.memory, nullptr);
            }

            for (auto& [key, cached] : m_desc_set_layout_cache) {
                for (VkSampler s : cached.immutable_samplers) vkDestroySampler(m_device, s, nullptr);
                if (cached.layout) vkDestroyDescriptorSetLayout(m_device, cached.layout, nullptr);
            }
            m_desc_set_layout_cache.clear();

            if (m_idle_fence)     { vkDestroyFence(m_device, m_idle_fence, nullptr); }
            if (m_pipeline_cache) { vkDestroyPipelineCache(m_device, m_pipeline_cache, nullptr); }
            if (m_desc_pool)      { vkDestroyDescriptorPool(m_device, m_desc_pool, nullptr); }
            if (m_command_pool)   { vkDestroyCommandPool(m_device, m_command_pool, nullptr); }
            vkDestroyDevice(m_device, nullptr);
        }

        if (m_messenger != VK_NULL_HANDLE) {
            auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (fn) fn(m_instance, m_messenger, nullptr);
        }

        if (m_instance) vkDestroyInstance(m_instance, nullptr);
    }

    // create_instance ----------------------------------------------------------------------------
    bool VulkanDevice::create_instance(bool enable_validation) {
        VkApplicationInfo app_info{};
        app_info.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName   = "Anxiety";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName        = "Anxiety";
        app_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion         = VK_API_VERSION_1_2;

        // Enumera las extensiones de instancia disponibles para solicitar únicamente las que existen.
        uint32_t avail_count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &avail_count, nullptr);
        std::vector<VkExtensionProperties> avail_exts(avail_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &avail_count, avail_exts.data());
        auto has_inst_ext = [&](const char* name) {
            for (auto& e : avail_exts) {
                if (std::strcmp(e.extensionName, name) == 0) return true;
            }

            return false;
        };

        std::vector<const char*> extensions = { "VK_KHR_surface" };
#ifdef _WIN32
        extensions.push_back("VK_KHR_win32_surface");
#elif defined(__linux__)
        // VulkanSwapchain crea la superficie mediante vkCreateXlibSurfaceKHR (LinuxWindow se basa en Xlib),
        // por lo que la instancia debe habilitar la extensión correspondiente — no VK_KHR_xcb_surface.
        extensions.push_back("VK_KHR_xlib_surface");
#elif defined(__APPLE__)
        extensions.push_back("VK_EXT_metal_surface");
        // MoltenVK es un ICD de "portabilidad" — como el loader lo marca como tal, las aplicaciones deben
        // optar explícitamente vía VK_KHR_portability_enumeration + VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
        // o vkCreateInstance devuelve VK_ERROR_INCOMPATIBLE_DRIVER aunque el ICD esté presente.
        if (has_inst_ext("VK_KHR_portability_enumeration")) extensions.push_back("VK_KHR_portability_enumeration");
#endif
        if (enable_validation) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        std::vector<const char*> layers;
        if (enable_validation) layers.push_back("VK_LAYER_KHRONOS_validation");

        VkInstanceCreateInfo ci{};
        ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ci.pApplicationInfo        = &app_info;
        ci.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
        ci.ppEnabledExtensionNames = extensions.data();
        ci.enabledLayerCount       = static_cast<uint32_t>(layers.size());
        ci.ppEnabledLayerNames     = layers.data();
#if defined(__APPLE__)
        ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

        VkResult res = vkCreateInstance(&ci, nullptr, &m_instance);
        if (res != VK_SUCCESS) {
            std::fprintf(stderr, "[VulkanDevice] vkCreateInstance falló: %d\n", res);
            return false;
        }
        return true;
    }

    // setup_debug_messenger ----------------------------------------------------------------------
    bool VulkanDevice::setup_debug_messenger() {
        auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
        if (!fn) return false;

        VkDebugUtilsMessengerCreateInfoEXT ci{};
        ci.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        ci.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        ci.pfnUserCallback = &VulkanDevice::debug_callback;
        ci.pUserData       = this;

        fn(m_instance, &ci, nullptr, &m_messenger);
        return true;
    }

    // select_physical_device ---------------------------------------------------------------------
    bool VulkanDevice::select_physical_device() {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
        if (count == 0) {
            std::fprintf(stderr, "[VulkanDevice] No se encontró ningún dispositivo físico Vulkan.\n");
            return false;
        }

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

        auto device_score = [](VkPhysicalDeviceType t) {
            switch (t) {
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return 4;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 3;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return 2;
            case VK_PHYSICAL_DEVICE_TYPE_OTHER:          return 1;
            default:                                     return 0;
            }
        };

        m_phys_device = VK_NULL_HANDLE;
        int best_score = -1;

        for (auto dev : devices) {
            uint32_t qf_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, nullptr);
            std::vector<VkQueueFamilyProperties> qf_props(qf_count);
            vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, qf_props.data());

            bool has_graphics = false;
            for (uint32_t i = 0; i < qf_count; ++i) {
                if (qf_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    has_graphics = true;
                    break;
                }
            }
            if (!has_graphics) continue;

            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(dev, &props);

            int score = device_score(props.deviceType);
            if (score > best_score) {
                best_score    = score;
                m_phys_device = dev;
                // Registra la familia de gráficos de este dispositivo
                for (uint32_t i = 0; i < qf_count; ++i) {
                    if (qf_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                        m_graphics_family = i;
                        break;
                    }
                }

                if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) break;
            }
        }

        if (m_phys_device == VK_NULL_HANDLE) {
            std::fprintf(stderr, "[VulkanDevice] No se encontró ningún dispositivo físico adecuado.\n");
            return false;
        }

        // Vuelve a explorar las familias de colas en el dispositivo seleccionado
        {
            uint32_t qf_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(m_phys_device, &qf_count, nullptr);
            std::vector<VkQueueFamilyProperties> qf_props(qf_count);
            vkGetPhysicalDeviceQueueFamilyProperties(m_phys_device, &qf_count, qf_props.data());
            for (uint32_t i = 0; i < qf_count; ++i) {
                if (qf_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                    m_graphics_family = i;
                    break;
                }
            }
        }
        return true;
    }

    // create_logical_device ----------------------------------------------------------------------
    bool VulkanDevice::create_logical_device() {
        const float queue_priority = 1.f;
        VkDeviceQueueCreateInfo queue_ci{};
        queue_ci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_ci.queueFamilyIndex = m_graphics_family;
        queue_ci.queueCount       = 1;
        queue_ci.pQueuePriorities = &queue_priority;

        uint32_t ext_count = 0;
        vkEnumerateDeviceExtensionProperties(m_phys_device, nullptr, &ext_count, nullptr);
        std::vector<VkExtensionProperties> avail_exts(ext_count);
        vkEnumerateDeviceExtensionProperties(m_phys_device, nullptr, &ext_count, avail_exts.data());

        auto has_ext = [&](const char* name) {
            for (auto& e : avail_exts) {
                if (std::strcmp(e.extensionName, name) == 0) return true;
            }

            return false;
        };

        std::vector<const char*> device_exts = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        if (has_ext(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)) {
            device_exts.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
            m_has_dynamic_rendering = true;
        }

        // Stride de vértices dinámico: requiere la extensión Y que el dispositivo soporte la feature.
        bool has_extended_dynamic_state = false;
        if (has_ext(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME)) {
            VkPhysicalDeviceExtendedDynamicStateFeaturesEXT eds_query{};
            eds_query.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
            VkPhysicalDeviceFeatures2 query2{};
            query2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            query2.pNext = &eds_query;
            vkGetPhysicalDeviceFeatures2(m_phys_device, &query2);

            if (eds_query.extendedDynamicState) {
                device_exts.push_back(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
                has_extended_dynamic_state = true;
            }
        }

        if (has_ext(VK_KHR_MAINTENANCE1_EXTENSION_NAME))        device_exts.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
        if (has_ext(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)) device_exts.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

        VkPhysicalDeviceFeatures features{};
        features.fillModeNonSolid  = VK_TRUE;
        features.samplerAnisotropy = VK_TRUE;

        VkPhysicalDeviceDynamicRenderingFeaturesKHR dyn_feature{};
        dyn_feature.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
        dyn_feature.dynamicRendering = VK_TRUE;

        VkPhysicalDeviceExtendedDynamicStateFeaturesEXT eds_feature{};
        eds_feature.sType                = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT;
        eds_feature.extendedDynamicState = VK_TRUE;

        VkDeviceCreateInfo ci{};
        ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        ci.queueCreateInfoCount    = 1;
        ci.pQueueCreateInfos       = &queue_ci;
        ci.enabledExtensionCount   = static_cast<uint32_t>(device_exts.size());
        ci.ppEnabledExtensionNames = device_exts.data();
        ci.pEnabledFeatures        = &features;
        // Cadena de features: dynamic rendering y/o extended dynamic state, según lo disponible.
        if (m_has_dynamic_rendering) {
            ci.pNext = &dyn_feature;
            if (has_extended_dynamic_state) dyn_feature.pNext = &eds_feature;
        } else if (has_extended_dynamic_state) {
            ci.pNext = &eds_feature;
        }

        VkResult res = vkCreateDevice(m_phys_device, &ci, nullptr, &m_device);
        if (res != VK_SUCCESS) {
            std::fprintf(stderr, "[VulkanDevice] vkCreateDevice falló: %d\n", res);
            return false;
        }

        vkGetDeviceQueue(m_device, m_graphics_family, 0, &m_graphics_queue);

        if (has_extended_dynamic_state) {
            pfn_cmd_bind_vertex_buffers2 = reinterpret_cast<PFN_vkCmdBindVertexBuffers2EXT>(vkGetDeviceProcAddr(m_device, "vkCmdBindVertexBuffers2EXT"));
            if (!pfn_cmd_bind_vertex_buffers2) pfn_cmd_bind_vertex_buffers2 = reinterpret_cast<PFN_vkCmdBindVertexBuffers2EXT>(vkGetDeviceProcAddr(m_device, "vkCmdBindVertexBuffers2"));
        }

        if (m_has_dynamic_rendering) {
            pfn_cmd_begin_rendering = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(vkGetDeviceProcAddr(m_device, "vkCmdBeginRenderingKHR"));
            pfn_cmd_end_rendering   = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(vkGetDeviceProcAddr(m_device, "vkCmdEndRenderingKHR"));

            if (!pfn_cmd_begin_rendering) pfn_cmd_begin_rendering = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(vkGetDeviceProcAddr(m_device, "vkCmdBeginRendering"));
            if (!pfn_cmd_end_rendering)   pfn_cmd_end_rendering = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(vkGetDeviceProcAddr(m_device, "vkCmdEndRendering"));

            if (!pfn_cmd_begin_rendering || !pfn_cmd_end_rendering) {
                std::fprintf(stderr, "[VulkanDevice] La extensión de dynamic rendering está presente pero los punteros a función no están disponibles — desactivando.\n");
                m_has_dynamic_rendering = false;
            }
        }
        return true;
    }

    // create_command_pool / create_descriptor_pool / create_pipeline_cache / create_idle_fence ---
    bool VulkanDevice::create_command_pool() {
        VkCommandPoolCreateInfo ci{};
        ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        ci.queueFamilyIndex = m_graphics_family;
        ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkResult res = vkCreateCommandPool(m_device, &ci, nullptr, &m_command_pool);
        if (res != VK_SUCCESS) {
            std::fprintf(stderr, "[VulkanDevice] vkCreateCommandPool falló: %d\n", res);
            return false;
        }
        return true;
    }

    bool VulkanDevice::create_descriptor_pool() {
        VkDescriptorPoolSize sizes[] = {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,           500 },
            { VK_DESCRIPTOR_TYPE_SAMPLER,                 500 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,           256 },
        };

        VkDescriptorPoolCreateInfo ci{};
        ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        ci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        ci.maxSets       = 4096;
        ci.poolSizeCount = static_cast<uint32_t>(std::size(sizes));
        ci.pPoolSizes    = sizes;

        VkResult res = vkCreateDescriptorPool(m_device, &ci, nullptr, &m_desc_pool);
        if (res != VK_SUCCESS) {
            std::fprintf(stderr, "[VulkanDevice] vkCreateDescriptorPool falló: %d\n", res);
            return false;
        }
        return true;
    }

    bool VulkanDevice::create_pipeline_cache() {
        VkPipelineCacheCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        return vkCreatePipelineCache(m_device, &ci, nullptr, &m_pipeline_cache) == VK_SUCCESS;
    }

    bool VulkanDevice::create_idle_fence() {
        VkFenceCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        return vkCreateFence(m_device, &ci, nullptr, &m_idle_fence) == VK_SUCCESS;
    }

    // Helpers del pool de recursos ---------------------------------------------------------------
    uint32_t VulkanDevice::allocate_texture_slot() {
        if (!m_texture_free.empty()) {
            uint32_t idx = m_texture_free.back();
            m_texture_free.pop_back();
            return idx;
        }

        m_textures.emplace_back();
        return static_cast<uint32_t>(m_textures.size() - 1);
    }

    void VulkanDevice::free_texture_slot(uint32_t slot) {
        m_textures[slot] = VkTextureSlot{};
        m_texture_free.push_back(slot);
    }

    uint32_t VulkanDevice::allocate_buffer_slot() {
        if (!m_buffer_free.empty()) {
            uint32_t idx = m_buffer_free.back();
            m_buffer_free.pop_back();
            return idx;
        }

        m_buffers.emplace_back();
        return static_cast<uint32_t>(m_buffers.size() - 1);
    }

    void VulkanDevice::free_buffer_slot(uint32_t slot) {
        m_buffers[slot] = VkBufferSlot{};
        m_buffer_free.push_back(slot);
    }

    VkTextureSlot& VulkanDevice::tex_slot(rhi::TextureHandle h) {
        assert(h.is_valid() && (h.id - 1) < m_textures.size());
        return m_textures[static_cast<size_t>(h.id - 1)];
    }

    const VkTextureSlot& VulkanDevice::tex_slot(rhi::TextureHandle h) const {
        assert(h.is_valid() && (h.id - 1) < m_textures.size());
        return m_textures[static_cast<size_t>(h.id - 1)];
    }

    VkBufferSlot& VulkanDevice::buf_slot(rhi::BufferHandle h) {
        assert(h.is_valid() && (h.id - 1) < m_buffers.size());
        return m_buffers[static_cast<size_t>(h.id - 1)];
    }

    const VkBufferSlot& VulkanDevice::buf_slot(rhi::BufferHandle h) const {
        assert(h.is_valid() && (h.id - 1) < m_buffers.size());
        return m_buffers[static_cast<size_t>(h.id - 1)];
    }

    // find_memory_type ---------------------------------------------------------------------------
    uint32_t VulkanDevice::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags props) const {
        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(m_phys_device, &mem_props);

        for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
            if ((type_filter & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & props) == props) return i;
        }

        std::fprintf(stderr, "[VulkanDevice] find_memory_type: no se encontró ningún tipo adecuado.\n");
        return 0;
    }

    // create_vk_buffer — crea un par VkBuffer + VkDeviceMemory -----------------------------------

    void VulkanDevice::create_vk_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& out_buf, VkDeviceMemory& out_mem) {
        VkBufferCreateInfo bci{};
        bci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bci.size        = size;
        bci.usage       = usage;
        bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(m_device, &bci, nullptr, &out_buf);

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(m_device, out_buf, &req);

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = req.size;
        ai.memoryTypeIndex = find_memory_type(req.memoryTypeBits, props);
        vkAllocateMemory(m_device, &ai, nullptr, &out_mem);
        vkBindBufferMemory(m_device, out_buf, out_mem, 0);
    }

    // Command buffer de un solo uso --------------------------------------------------------------
    VkCommandBuffer VulkanDevice::begin_one_shot() {
        VkCommandBufferAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool        = m_command_pool;
        ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(m_device, &ai, &cmd);

        VkCommandBufferBeginInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &bi);
        return cmd;
    }

    void VulkanDevice::end_one_shot(VkCommandBuffer cmd) {
        vkEndCommandBuffer(cmd);

        VkSubmitInfo si{};
        si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers    = &cmd;

        vkResetFences(m_device, 1, &m_idle_fence);
        vkQueueSubmit(m_graphics_queue, 1, &si, m_idle_fence);
        vkWaitForFences(m_device, 1, &m_idle_fence, VK_TRUE, UINT64_MAX);
        vkFreeCommandBuffers(m_device, m_command_pool, 1, &cmd);
    }

    // transition_image_layout — helper de barrera ------------------------------------------------
    void VulkanDevice::transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect, VkImageLayout old_layout, VkImageLayout new_layout) {
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = old_layout;
        barrier.newLayout                       = new_layout;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = image;
        barrier.subresourceRange.aspectMask     = aspect;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;

        VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

        if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
            new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
            new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        } else {
            barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            src_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            dst_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }

        vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // register_external_image / unregister_texture -----------------------------------------------
    rhi::TextureHandle VulkanDevice::register_external_image(VkImage image, VkImageView view, VkFormat format, uint32_t width, uint32_t height) {
        uint32_t       idx  = allocate_texture_slot();
        VkTextureSlot& slot = m_textures[idx];
        slot.image    = image;
        slot.view     = view;
        slot.memory   = VK_NULL_HANDLE;
        slot.format   = format;
        slot.layout   = VK_IMAGE_LAYOUT_UNDEFINED;
        slot.width    = width;
        slot.height   = height;
        slot.is_depth = is_depth_format(format);
        slot.external = true;
        slot.alive    = true;
        return { static_cast<uint64_t>(idx) + 1 };
    }

    void VulkanDevice::unregister_texture(rhi::TextureHandle handle) {
        if (!handle.is_valid()) return;
        auto idx = static_cast<uint32_t>(handle.id - 1);
        m_textures[idx].alive = false;
        free_texture_slot(idx);
    }

    // IDevice — create_buffer --------------------------------------------------------------------
    rhi::BufferHandle VulkanDevice::create_buffer(const rhi::BufferDesc& desc, const void* initial_data, size_t initial_data_sz) {
        using BU = rhi::BufferUsage;

        const bool is_uniform = has_flag(desc.usage, BU::Uniform);
        const bool is_vertex  = has_flag(desc.usage, BU::Vertex);
        const bool is_index   = has_flag(desc.usage, BU::Index);
        const bool is_storage = has_flag(desc.usage, BU::Storage);

        VkBufferUsageFlags vk_usage = 0;
        if (is_uniform) vk_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if (is_vertex)  vk_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if (is_index)   vk_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if (is_storage) vk_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

        const bool host_visible = is_uniform;
        VkMemoryPropertyFlags mem_props = host_visible ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if (!host_visible && initial_data) vk_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        uint32_t      idx  = allocate_buffer_slot();
        VkBufferSlot& slot = m_buffers[idx];
        slot.size  = desc.size_bytes;
        slot.alive = true;

        create_vk_buffer(desc.size_bytes, vk_usage, mem_props, slot.buffer, slot.memory);

        if (host_visible) vkMapMemory(m_device, slot.memory, 0, desc.size_bytes, 0, &slot.mapped);

        if (initial_data && initial_data_sz > 0) {
            if (host_visible) {
                std::memcpy(slot.mapped, initial_data, initial_data_sz);
            } else {
                VkBuffer       staging_buf;
                VkDeviceMemory staging_mem;
                create_vk_buffer(initial_data_sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buf, staging_mem);

                void* mapped;
                vkMapMemory(m_device, staging_mem, 0, initial_data_sz, 0, &mapped);
                std::memcpy(mapped, initial_data, initial_data_sz);
                vkUnmapMemory(m_device, staging_mem);

                VkCommandBuffer cmd = begin_one_shot();
                VkBufferCopy copy{ 0, 0, initial_data_sz };
                vkCmdCopyBuffer(cmd, staging_buf, slot.buffer, 1, &copy);
                end_one_shot(cmd);

                vkDestroyBuffer(m_device, staging_buf, nullptr);
                vkFreeMemory(m_device, staging_mem, nullptr);
            }
        }

        return { static_cast<uint64_t>(idx) + 1 };
    }

    // IDevice — create_texture -------------------------------------------------------------------
    rhi::TextureHandle VulkanDevice::create_texture(const rhi::TextureDesc& desc) {
        VkFormat fmt      = to_vk_format(desc.format);
        bool     is_depth = is_depth_format(fmt);

        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        if (desc.is_render_target) usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (is_depth)              usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        VkImageCreateInfo image_ci{};
        image_ci.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_ci.imageType     = VK_IMAGE_TYPE_2D;
        image_ci.format        = fmt;
        image_ci.extent        = { desc.extent.width, desc.extent.height, 1 };
        image_ci.mipLevels     = desc.mip_levels;
        image_ci.arrayLayers   = desc.array_size;
        image_ci.samples       = VK_SAMPLE_COUNT_1_BIT;
        image_ci.tiling        = VK_IMAGE_TILING_OPTIMAL;
        image_ci.usage         = usage;
        image_ci.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        uint32_t       idx  = allocate_texture_slot();
        VkTextureSlot& slot = m_textures[idx];
        slot.format   = fmt;
        slot.layout   = VK_IMAGE_LAYOUT_UNDEFINED;
        slot.width    = desc.extent.width;
        slot.height   = desc.extent.height;
        slot.is_depth = is_depth;
        slot.alive    = true;

        vkCreateImage(m_device, &image_ci, nullptr, &slot.image);

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(m_device, slot.image, &req);

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = req.size;
        ai.memoryTypeIndex = find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(m_device, &ai, nullptr, &slot.memory);
        vkBindImageMemory(m_device, slot.image, slot.memory, 0);

        VkImageViewCreateInfo view_ci{};
        view_ci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_ci.image                           = slot.image;
        view_ci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        view_ci.format                          = fmt;
        view_ci.components                      = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        view_ci.subresourceRange.aspectMask     = is_depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        view_ci.subresourceRange.baseMipLevel   = 0;
        view_ci.subresourceRange.levelCount     = desc.mip_levels;
        view_ci.subresourceRange.baseArrayLayer = 0;
        view_ci.subresourceRange.layerCount     = desc.array_size;
        vkCreateImageView(m_device, &view_ci, nullptr, &slot.view);

        {
            VkCommandBuffer cmd = begin_one_shot();
            VkImageLayout target_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            if (is_depth) {
                target_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            } else if (desc.is_render_target) {
                target_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                }

            if (target_layout != VK_IMAGE_LAYOUT_UNDEFINED) {
                transition_image_layout(cmd, slot.image, view_ci.subresourceRange.aspectMask, VK_IMAGE_LAYOUT_UNDEFINED, target_layout);
                slot.layout = target_layout;
            }
            end_one_shot(cmd);
        }

        return { static_cast<uint64_t>(idx) + 1 };
    }

    // IDevice — destroy_buffer / destroy_texture -------------------------------------------------
    void VulkanDevice::destroy_buffer(rhi::BufferHandle handle) {
        if (!handle.is_valid()) return;
        VkBufferSlot& slot = buf_slot(handle);

        if (!slot.alive) return;
        if (slot.mapped) vkUnmapMemory(m_device, slot.memory);

        vkDestroyBuffer(m_device, slot.buffer, nullptr);
        vkFreeMemory(m_device, slot.memory, nullptr);
        free_buffer_slot(static_cast<uint32_t>(handle.id - 1));
    }

    void VulkanDevice::destroy_texture(rhi::TextureHandle handle) {
        if (!handle.is_valid()) return;

        VkTextureSlot& slot = tex_slot(handle);
        if (!slot.alive) return;
        if (!slot.external) {
            vkDestroyImageView(m_device, slot.view, nullptr);
            vkDestroyImage(m_device, slot.image, nullptr);
            vkFreeMemory(m_device, slot.memory, nullptr);
        }

        free_texture_slot(static_cast<uint32_t>(handle.id - 1));
    }

    // write_buffer / upload_texture_data ---------------------------------------------------------
    void VulkanDevice::write_buffer(rhi::BufferHandle handle, const void* data, size_t offset, size_t size) {
        if (!handle.is_valid()) return;

        VkBufferSlot& slot = buf_slot(handle);
        if (!slot.alive) return;

        if (slot.mapped) {
            std::memcpy(static_cast<uint8_t*>(slot.mapped) + offset, data, size);
        } else {
            VkBuffer       staging_buf;
            VkDeviceMemory staging_mem;
            create_vk_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buf, staging_mem);

            void* mapped;
            vkMapMemory(m_device, staging_mem, 0, size, 0, &mapped);
            std::memcpy(mapped, data, size);
            vkUnmapMemory(m_device, staging_mem);

            VkCommandBuffer cmd = begin_one_shot();
            VkBufferCopy copy{ 0, offset, size };
            vkCmdCopyBuffer(cmd, staging_buf, slot.buffer, 1, &copy);
            end_one_shot(cmd);

            vkDestroyBuffer(m_device, staging_buf, nullptr);
            vkFreeMemory(m_device, staging_mem, nullptr);
        }
    }

    void VulkanDevice::upload_texture_data(rhi::TextureHandle handle, const uint8_t* rgba8, uint32_t width, uint32_t height) {
        if (!handle.is_valid()) return;

        VkTextureSlot& slot = tex_slot(handle);
        if (!slot.alive) return;

        const VkDeviceSize image_size = static_cast<VkDeviceSize>(width) * height * 4;

        VkBuffer       staging_buf;
        VkDeviceMemory staging_mem;
        create_vk_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buf, staging_mem);

        void* mapped;
        vkMapMemory(m_device, staging_mem, 0, image_size, 0, &mapped);
        std::memcpy(mapped, rgba8, static_cast<size_t>(image_size));
        vkUnmapMemory(m_device, staging_mem);

        VkCommandBuffer cmd = begin_one_shot();

        transition_image_layout(cmd, slot.image, VK_IMAGE_ASPECT_COLOR_BIT, slot.layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel       = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = 1;
        region.imageExtent                     = { width, height, 1 };
        vkCmdCopyBufferToImage(cmd, staging_buf, slot.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        transition_image_layout(cmd, slot.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        slot.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        end_one_shot(cmd);

        vkDestroyBuffer(m_device, staging_buf, nullptr);
        vkFreeMemory(m_device, staging_mem, nullptr);
    }

    // IDevice — create_command_buffer / create_swapchain -----------------------------------------
    std::unique_ptr<rhi::ICommandBuffer> VulkanDevice::create_command_buffer() {
        return std::make_unique<VulkanCommandBuffer>(*this);
    }

    std::unique_ptr<rhi::ISwapchain> VulkanDevice::create_swapchain(const rhi::SwapchainDesc& desc) {
        return std::make_unique<VulkanSwapchain>(*this, desc);
    }

    // IDevice — submit / wait_idle ---------------------------------------------------------------
    void VulkanDevice::submit(rhi::ICommandBuffer& cmd_buf) {
        auto& vk_cmd_buf = static_cast<VulkanCommandBuffer&>(cmd_buf);
        VkCommandBuffer cmd = vk_cmd_buf.vk_cmd();

        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo si{};
        si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers    = &cmd;

        if (m_wait_semaphore != VK_NULL_HANDLE) {
            si.waitSemaphoreCount = 1;
            si.pWaitSemaphores    = &m_wait_semaphore;
            si.pWaitDstStageMask  = &wait_stage;
        }

        if (m_signal_semaphore != VK_NULL_HANDLE) {
            si.signalSemaphoreCount = 1;
            si.pSignalSemaphores    = &m_signal_semaphore;
        }

        vkResetFences(m_device, 1, &m_idle_fence);
        vkQueueSubmit(m_graphics_queue, 1, &si, m_idle_fence);
        vkWaitForFences(m_device, 1, &m_idle_fence, VK_TRUE, UINT64_MAX);

        m_wait_semaphore   = VK_NULL_HANDLE;
        m_signal_semaphore = VK_NULL_HANDLE;
    }

    void VulkanDevice::wait_idle() {
        vkDeviceWaitIdle(m_device);
    }

    // ---------------------------------------------------------------------------
    // Compilación de shaders — fuente HLSL -> SPIR-V vía shaderc (el front-end HLSL de glslang).
    //
    // Nuestros shaders se escriben para D3D12 con numeración por clase de registro (los cbuffers
    // usan bN, las texturas tN, los samplers sN — el mismo N puede repetirse entre clases). Los
    // bindings de descriptor de Vulkan deben ser únicos dentro de un set, así que cada clase de
    // registro HLSL se desplaza a su propio rango de binding vía SetBindingBase. Esto DEBE coincidir
    // con VulkanHelpers::to_vk_binding_index(), que VulkanPipeline / VulkanDescriptorSet usan para
    // construir los VkDescriptorSetLayoutBinding correspondientes.
    // ---------------------------------------------------------------------------

    std::vector<uint8_t> VulkanDevice::compile_shader_from_source(const char* source, const char* entry_point, anxiety::rendering::rhi::ShaderStage stage)
    {
        return shader::compile_hlsl_to_spirv(source, entry_point, stage);
    }

    // ---------------------------------------------------------------------------
    // Fábricas de shaders / pipelines / descriptor sets
    // ---------------------------------------------------------------------------

    std::unique_ptr<anxiety::rendering::rhi::IShader> VulkanDevice::create_shader(const anxiety::rendering::rhi::ShaderDesc& desc, anxiety::rendering::rhi::ShaderStage stage)
    {
        if (!m_valid) return nullptr;
        if (!desc.bytecode || desc.bytecode_size == 0 || (desc.bytecode_size % 4) != 0) {
            LOG_ERROR("RHI", "VulkanDevice::create_shader: bytecode SPIR-V vacío o mal alineado.");
            return nullptr;
        }

        VkShaderModuleCreateInfo ci{};
        ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = desc.bytecode_size;
        ci.pCode    = reinterpret_cast<const uint32_t*>(desc.bytecode);

        VkShaderModule module = VK_NULL_HANDLE;
        if (vkCreateShaderModule(m_device, &ci, nullptr, &module) != VK_SUCCESS) {
            LOG_ERROR("RHI", "VulkanDevice::create_shader: vkCreateShaderModule falló.");
            return nullptr;
        }

        return std::make_unique<VulkanShader>(m_device, module, stage, desc.entry_point);
    }

    std::unique_ptr<anxiety::rendering::rhi::IPipeline> VulkanDevice::create_pipeline(const anxiety::rendering::rhi::PipelineDesc& desc)
    {
        if (!m_valid) return nullptr;
        auto pipeline = std::make_unique<VulkanPipeline>(*this, desc);
        if (!pipeline->is_valid()) {
            LOG_ERROR("RHI", "VulkanDevice::create_pipeline: falló la creación del pipeline.");
            return nullptr;
        }
        return pipeline;
    }

    std::unique_ptr<anxiety::rendering::rhi::IDescriptorSet> VulkanDevice::create_descriptor_set(const anxiety::rendering::rhi::DescriptorSetLayout& layout)
    {
        if (!m_valid) return nullptr;
        return std::make_unique<VulkanDescriptorSet>(*this, layout);
    }

    // ---------------------------------------------------------------------------
    // get_or_create_descriptor_set_layout — caché compartida de VkDescriptorSetLayout, indexada por
    // el contenido del DescriptorSetLayout de la RHI (tríos binding/type/count). Ver VulkanDevice.h.
    // ---------------------------------------------------------------------------

    namespace {
        size_t hash_descriptor_layout(const anxiety::rendering::rhi::DescriptorSetLayout& layout) {
            size_t h = layout.bindings.size();
            for (const auto& b : layout.bindings) {
                h ^= std::hash<uint32_t>{}(b.binding)                            + 0x9e3779b9 + (h << 6) + (h >> 2);
                h ^= std::hash<uint32_t>{}(static_cast<uint32_t>(b.type))        + 0x9e3779b9 + (h << 6) + (h >> 2);
                h ^= std::hash<uint32_t>{}(b.count)                              + 0x9e3779b9 + (h << 6) + (h >> 2);
            }
            return h;
        }
    }

    VkDescriptorSetLayout VulkanDevice::get_or_create_descriptor_set_layout(const rhi::DescriptorSetLayout& content, const std::vector<rhi::SamplerDesc>* static_samplers) {
        const size_t key = hash_descriptor_layout(content);

        auto it = m_desc_set_layout_cache.find(key);
        if (it != m_desc_set_layout_cache.end()) return it->second.layout;

        CachedDescriptorSetLayout cached;

        // Un binding por cada entrada de descriptor de la RHI (uniform buffers, texturas, storage
        // buffers, samplers dinámicos), más un binding por cada sampler estático/incrustado (como VkSampler inmutable).
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(content.bindings.size() + (static_samplers ? static_samplers->size() : 0));

        for (const auto& b : content.bindings) {
            VkDescriptorSetLayoutBinding vkb{};
            vkb.binding         = to_vk_binding_index(b.type, b.binding);
            vkb.descriptorType  = to_vk_descriptor_type(b.type);
            vkb.descriptorCount = b.count;
            vkb.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            bindings.push_back(vkb);
        }

        // Los samplers inmutables deben seguir vivos al menos tanto como el VkDescriptorSetLayout —
        // son propiedad de la entrada del caché, y se destruyen junto a ella en ~VulkanDevice().
        if (static_samplers) {
            cached.immutable_samplers.reserve(static_samplers->size());
            for (const auto& sd : *static_samplers) {
                VkSamplerCreateInfo sci{};
                sci.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                sci.magFilter        = to_vk_filter(sd.filter);
                sci.minFilter        = to_vk_filter(sd.filter);
                sci.mipmapMode       = to_vk_mipmap_mode(sd.filter);
                sci.addressModeU     = to_vk_sampler_address_mode(sd.address_U);
                sci.addressModeV     = to_vk_sampler_address_mode(sd.address_V);
                sci.addressModeW     = to_vk_sampler_address_mode(sd.address_W);
                sci.mipLodBias       = sd.mip_lod_bias;
                sci.anisotropyEnable = sd.filter == rhi::FilterMode::Anisotropic ? VK_TRUE : VK_FALSE;
                sci.maxAnisotropy    = static_cast<float>(sd.max_anisotropy);
                sci.compareOp        = VK_COMPARE_OP_ALWAYS;
                sci.minLod           = sd.min_lod;
                sci.maxLod           = sd.max_lod;
                sci.borderColor      = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

                VkSampler sampler = VK_NULL_HANDLE;
                if (vkCreateSampler(m_device, &sci, nullptr, &sampler) != VK_SUCCESS) {
                    LOG_ERROR("RHI", "get_or_create_descriptor_set_layout: vkCreateSampler falló.");
                    continue;
                }
                cached.immutable_samplers.push_back(sampler);
            }

            // Segunda pasada: referencia el almacenamiento de immutable_samplers (ya estable) desde cada binding.
            for (size_t i = 0; i < static_samplers->size() && i < cached.immutable_samplers.size(); ++i) {
                VkDescriptorSetLayoutBinding vkb{};
                vkb.binding            = to_vk_binding_index(rhi::DescriptorType::Sampler, (*static_samplers)[i].shader_register);
                vkb.descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLER;
                vkb.descriptorCount    = 1;
                vkb.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
                vkb.pImmutableSamplers = &cached.immutable_samplers[i];
                bindings.push_back(vkb);
            }
        }

        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = static_cast<uint32_t>(bindings.size());
        ci.pBindings    = bindings.empty() ? nullptr : bindings.data();

        VkResult res = vkCreateDescriptorSetLayout(m_device, &ci, nullptr, &cached.layout);
        if (res != VK_SUCCESS) {
            LOGF_ERROR("RHI", "get_or_create_descriptor_set_layout: vkCreateDescriptorSetLayout falló (VkResult={}).", static_cast<int>(res));
            for (VkSampler s : cached.immutable_samplers) vkDestroySampler(m_device, s, nullptr);
            return VK_NULL_HANDLE;
        }

        VkDescriptorSetLayout result = cached.layout;
        m_desc_set_layout_cache.emplace(key, std::move(cached));
        return result;
    }

    // Callback de depuración ---------------------------------------------------------------------
    VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDevice::debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT /*type*/, const VkDebugUtilsMessengerCallbackDataEXT* data, void* /*user_data*/) {
        const char* level = (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "ERROR" : (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) ? "WARNING" : "INFO";
        std::fprintf(stderr, "[Vulkan %s] %s\n", level, data->pMessage);
        return VK_FALSE;
    }

} // namespace anxiety::rendering::backend::vulkan

#endif // ANXIETY_BACKEND_VULKAN
