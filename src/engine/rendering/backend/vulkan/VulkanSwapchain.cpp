#ifdef ANXIETY_BACKEND_VULKAN

#include "VulkanSwapchain.h"
#include "VulkanDevice.h"
#include "VulkanHelpers.h"
#include "Logger.h"

#include <cassert>
#include <cstdio>
#include <algorithm>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#  if defined(USE_WAYLAND)
#    include <wayland-client.h>
#    include <vulkan/vulkan_wayland.h>
#  else
#    include <X11/Xlib.h>
#    include <vulkan/vulkan_xlib.h>
#  endif
#elif defined(__APPLE__)
#  import <AppKit/AppKit.h>
#  import <QuartzCore/CAMetalLayer.h>
#  include <vulkan/vulkan_metal.h>
#endif

namespace anxiety::rendering::backend::vulkan {
    // Construcción / destrucción -------------------------------------------------------------------
    VulkanSwapchain::VulkanSwapchain(VulkanDevice& device, const rhi::SwapchainDesc& desc) : m_device(device), m_vsync(desc.vsync) {
        create_surface(desc);
        if (!m_surface) {
            LOG_ERROR("VulkanSwapchain", "La creación de la superficie falló — se aborta el swapchain.");
            return;
        }
        create_swapchain(desc);

        // Semáforos por fotograma
        VkSemaphoreCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vkCreateSemaphore(device.device(), &sci, nullptr, &m_image_available);
        vkCreateSemaphore(device.device(), &sci, nullptr, &m_render_finished);
    }

    VulkanSwapchain::~VulkanSwapchain() {
        destroy_swapchain_resources();

        if (m_render_finished) vkDestroySemaphore(m_device.device(), m_render_finished, nullptr);
        if (m_image_available) vkDestroySemaphore(m_device.device(), m_image_available, nullptr);
        if (m_swapchain)       vkDestroySwapchainKHR(m_device.device(), m_swapchain, nullptr);
        if (m_surface)         vkDestroySurfaceKHR(m_device.instance(), m_surface, nullptr);
    }

    // createSurface — específico de la plataforma --------------------------------------------------
    void VulkanSwapchain::create_surface(const rhi::SwapchainDesc& desc) {
#ifdef _WIN32
        VkWin32SurfaceCreateInfoKHR ci{};
        ci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        ci.hwnd      = static_cast<HWND>(desc.native_window_handle);
        ci.hinstance = GetModuleHandle(nullptr);
        VkResult res = vkCreateWin32SurfaceKHR(m_device.instance(), &ci, nullptr, &m_surface);
        if (res != VK_SUCCESS) LOGF_ERROR("VulkanSwapchain", "vkCreateWin32SurfaceKHR falló (VkResult={})", (int)res);
#elif defined(__linux__)
#  if defined(USE_WAYLAND)
        VkWaylandSurfaceCreateInfoKHR ci{};
        ci.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        ci.display = wl_display_connect(nullptr);
        ci.surface = static_cast<struct wl_surface*>(desc.native_window_handle);
        vkCreateWaylandSurfaceKHR(m_device.instance(), &ci, nullptr, &m_surface);
#  else
        VkXlibSurfaceCreateInfoKHR ci{};
        ci.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        ci.dpy    = XOpenDisplay(nullptr);
        ci.window = reinterpret_cast<Window>(reinterpret_cast<uintptr_t>(desc.native_window_handle));
        vkCreateXlibSurfaceKHR(m_device.instance(), &ci, nullptr, &m_surface);
#  endif
#elif defined(__APPLE__)
        @autoreleasepool{
            NSWindow * win = (__bridge NSWindow*)desc.native_window_handle;
            if (!win) {
                LOG_ERROR("VulkanSwapchain", "native_window_handle es nil.");
                return;
            }
            NSView* view = [win contentView];

            // MoltenVK (VK_EXT_metal_surface) renderiza a través de un CAMetalLayer, igual que el
            // backend nativo de Metal — se adjunta uno a la content view de la ventana para que el
            // swapchain lo use como destino.
            CAMetalLayer* layer = [CAMetalLayer layer];
            [view setLayer : layer] ;
            [view setWantsLayer : YES] ;

            VkMetalSurfaceCreateInfoEXT ci{};
            ci.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
            ci.pLayer = layer;
            VkResult res = vkCreateMetalSurfaceEXT(m_device.instance(), &ci, nullptr, &m_surface);
            if (res != VK_SUCCESS)
                LOGF_ERROR("VulkanSwapchain", "vkCreateMetalSurfaceEXT falló (VkResult={}).", (int)res);
        }
#else
        LOG_ERROR("VulkanSwapchain", "Plataforma no soportada para la creación de la superficie.");
#endif
    }

    // createSwapchain ------------------------------------------------------------------------------
    void VulkanSwapchain::create_swapchain(const rhi::SwapchainDesc& desc) {
        VkPhysicalDevice phys = m_device.phys_device();

        // Capacidades de la superficie
        VkSurfaceCapabilitiesKHR caps{};
        {
            VkResult r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, m_surface, &caps);
            if (r != VK_SUCCESS) {
                LOGF_ERROR("VulkanSwapchain", "vkGetPhysicalDeviceSurfaceCapabilitiesKHR falló (VkResult={}).", (int)r);
                return;
            }
        }
        LOGF_INFO("VulkanSwapchain", "Capacidades de superficie: currentExtent={}x{} minExt={}x{} maxExt={}x{} imgCount=[{},{}] supportedUsage=0x{:X} compositeAlpha=0x{:X}.",
            caps.currentExtent.width,           caps.currentExtent.height,
            caps.minImageExtent.width,          caps.minImageExtent.height,
            caps.maxImageExtent.width,          caps.maxImageExtent.height,
            caps.minImageCount,                 caps.maxImageCount,
            (unsigned)caps.supportedUsageFlags, (unsigned)caps.supportedCompositeAlpha);

        // Elige el formato
        uint32_t fmt_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(phys, m_surface, &fmt_count, nullptr);
        std::vector<VkSurfaceFormatKHR> surface_fmts(fmt_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(phys, m_surface, &fmt_count, surface_fmts.data());

        if (fmt_count == 0) {
            LOGF_ERROR("VulkanSwapchain", "No se devolvió ningún formato de superficie — no se puede crear el swapchain.");
            return;
        }
        VkSurfaceFormatKHR chosen_fmt = surface_fmts[0];
        VkFormat desired_Vk_fmt = to_vk_format(desc.format);
        for (auto& sf : surface_fmts) {
            if (sf.format == desired_Vk_fmt && sf.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                chosen_fmt = sf;
                break;
            }
        }

        // Elige el modo de presentación
        uint32_t pm_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(phys, m_surface, &pm_count, nullptr);
        std::vector<VkPresentModeKHR> present_modes(pm_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(phys, m_surface, &pm_count, present_modes.data());

        VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;       // garantizado, con vsync
        if (!desc.vsync) {
            for (auto pm : present_modes) {
                if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
                    present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                    break;
                }
            }
            if (present_mode == VK_PRESENT_MODE_FIFO_KHR) {
                for (auto pm : present_modes) {
                    if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                        present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                        break;
                    }
                }
            }
        }

        // Extensión del swapchain —
        // currentExtent == UINT32_MAX significa "el driver no la fija, usa el tamaño de la ventana".
        // currentExtent == {0,0} puede ocurrir si la consulta de capacidades se hizo contra una
        // superficie no inicializada o la ventana aún no está mapeada; en ese caso se recurre al
        // tamaño de la ventana.
        VkExtent2D extent{};
        if (caps.currentExtent.width != UINT32_MAX && caps.currentExtent.width != 0) {
            extent = caps.currentExtent;
        } else {
            extent.width = desc.extent.width;
            extent.height = desc.extent.height;
            if (caps.maxImageExtent.width > 0) {
                extent.width  = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
                extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
            }
        }

        if (extent.width == 0 || extent.height == 0) {
            LOGF_ERROR("VulkanSwapchain", "La extensión del swapchain es {}x{} — no se puede crear el swapchain.", extent.width, extent.height);
            return;
        }

        // Número de imágenes
        uint32_t image_count = desc.image_count;
        image_count = std::max(image_count, caps.minImageCount);
        if (caps.maxImageCount > 0) image_count = std::min(image_count, caps.maxImageCount);
        image_count = std::min(image_count, static_cast<uint32_t>(k_max_images));

        // Uso de la imagen — solo se solicitan los bits que la superficie realmente soporta
        VkImageUsageFlags image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) image_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        // Composite alpha — se elige el primer modo que soporte la superficie
        VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        for (auto ca : { VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR }) {
            if (caps.supportedCompositeAlpha & ca) { composite_alpha = ca; break; }
        }

        VkSwapchainCreateInfoKHR ci{};
        ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        ci.surface          = m_surface;
        ci.minImageCount    = image_count;
        ci.imageFormat      = chosen_fmt.format;
        ci.imageColorSpace  = chosen_fmt.colorSpace;
        ci.imageExtent      = extent;
        ci.imageArrayLayers = 1;
        ci.imageUsage       = image_usage;
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.preTransform     = caps.currentTransform;
        ci.compositeAlpha   = composite_alpha;
        ci.presentMode      = present_mode;
        ci.clipped          = VK_TRUE;
        ci.oldSwapchain     = VK_NULL_HANDLE;

        LOGF_INFO("VulkanSwapchain", "Creando swapchain: {}x{} fmt={} imgCount={} presentMode={} compositeAlpha={} usage=0x{:X}.",
            extent.width,      extent.height,        (int)chosen_fmt.format, image_count,
            (int)present_mode, (int)composite_alpha, (unsigned)image_usage);

        VkResult res = vkCreateSwapchainKHR(m_device.device(), &ci, nullptr, &m_swapchain);
        if (res != VK_SUCCESS) {
            LOGF_ERROR("VulkanSwapchain", "vkCreateSwapchainKHR falló (VkResult={}).", (int)res);
            return;
        }

        m_extent = { extent.width, extent.height };
        m_format = from_vk_format(chosen_fmt.format);

        acquire_images();
    }

    // acquireImages — obtiene las imágenes de backbuffer y las registra en el dispositivo ----------
    void VulkanSwapchain::acquire_images() {
        vkGetSwapchainImagesKHR(m_device.device(), m_swapchain, &m_image_count, nullptr);
        m_image_count = std::min(m_image_count, k_max_images);

        VkImage tmp_images[k_max_images];
        vkGetSwapchainImagesKHR(m_device.device(), m_swapchain, &m_image_count, tmp_images);

        VkFormat fmt = to_vk_format(m_format);

        for (uint32_t i = 0; i < m_image_count; ++i) {
            m_images[i] = tmp_images[i];

            // Crea la image view
            VkImageViewCreateInfo viewCI{};
            viewCI.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewCI.image                           = m_images[i];
            viewCI.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            viewCI.format                          = fmt;
            viewCI.components                      = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
            viewCI.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            viewCI.subresourceRange.baseMipLevel   = 0;
            viewCI.subresourceRange.levelCount     = 1;
            viewCI.subresourceRange.baseArrayLayer = 0;
            viewCI.subresourceRange.layerCount     = 1;
            vkCreateImageView(m_device.device(), &viewCI, nullptr, &m_views[i]);

            m_handles[i] = m_device.register_external_image(m_images[i], m_views[i], fmt, m_extent.width, m_extent.height);
        }
    }

    // destroySwapchainResources — views y texture handles (no el swapchain en sí) -------------------
    void VulkanSwapchain::destroy_swapchain_resources() {
        for (uint32_t i = 0; i < m_image_count; ++i) {
            if (m_handles[i].is_valid()) {
                m_device.unregister_texture(m_handles[i]);
                m_handles[i] = {};
            }
            if (m_views[i]) {
                vkDestroyImageView(m_device.device(), m_views[i], nullptr);
                m_views[i] = VK_NULL_HANDLE;
            }
        }
        m_image_count = 0;
    }

    // ISwapchain ---------------------------------------------------------------------------------
    anxiety::rendering::rhi::TextureHandle VulkanSwapchain::current_backbuffer() const noexcept {
        if (m_current_image >= m_image_count) return {};
        return m_handles[m_current_image];
    }

    uint32_t VulkanSwapchain::acquire_next_image() {
        if (!m_swapchain || !m_image_available) return m_current_image;

        // Registra nuestro semáforo de "imagen disponible" en el canal lateral de espera del
        // dispositivo, para que el siguiente IDevice::submit() espere en él en la etapa
        // COLOR_ATTACHMENT_OUTPUT.
        m_device.set_wait_semaphore(m_image_available);
        m_device.set_signal_semaphore(m_render_finished);

        vkAcquireNextImageKHR(m_device.device(), m_swapchain, UINT64_MAX, m_image_available, VK_NULL_HANDLE, &m_current_image);
        return m_current_image;
    }

    void VulkanSwapchain::present() {
        if (!m_swapchain || !m_render_finished) return;

        // En este punto el command buffer ya se ha enviado y ha señalizado m_renderFinished.
        // Esperamos en él dentro de vkQueuePresentKHR para que la GPU haya terminado de escribir
        // antes de mostrarlo en pantalla.

        VkPresentInfoKHR pi{};
        pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores    = &m_render_finished;
        pi.swapchainCount     = 1;
        pi.pSwapchains        = &m_swapchain;
        pi.pImageIndices      = &m_current_image;
        vkQueuePresentKHR(m_device.graphics_queue(), &pi);

        // Nota: el layout rastreado del backbuffer se deja en VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        // (fijado por el resource_barrier() que se ejecutó justo antes de este present) — ese sigue
        // siendo genuinamente el layout real de la imagen tras vkQueuePresentKHR. NO debe
        // reiniciarse a UNDEFINED aquí: VulkanCommandBuffer::resource_barrier() usa este valor
        // rastreado como el oldLayout real del barrier en el siguiente fotograma en que se reutilice
        // este mismo índice de imagen, y un UNDEFINED falso declararía mal ese barrier frente al
        // propio rastreo (correcto) de la capa de validación.
    }

    void VulkanSwapchain::resize(rhi::Extent2D new_extent) {
        m_device.wait_idle();
        destroy_swapchain_resources();

        if (m_swapchain) {
            vkDestroySwapchainKHR(m_device.device(), m_swapchain, nullptr);
            m_swapchain = VK_NULL_HANDLE;
        }

        rhi::SwapchainDesc desc;
        desc.extent      = new_extent;
        desc.image_count = k_max_images;
        desc.format      = m_format;
        desc.vsync       = m_vsync;
        create_swapchain(desc);
    }
} // namespace anxiety::rendering::backend::vulkan

#endif // ANXIETY_BACKEND_VULKAN
