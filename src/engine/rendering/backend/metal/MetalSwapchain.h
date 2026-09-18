#pragma once

#ifdef ANXIETY_BACKEND_METAL

#include "../../rhi/ISwapchain.h"
#include "../../rhi/RHITypes.h"

namespace anxiety::rendering::backend::metal {
    class MetalDevice;

    class MetalSwapchain final : public rhi::ISwapchain {
    public:
        MetalSwapchain(MetalDevice* device, const rhi::SwapchainDesc& desc);
        ~MetalSwapchain() override;

        // ISwapchain -------------------------------------------------------------------------------
        [[nodiscard]] uint32_t           image_count()        const noexcept override { return m_image_count; }
        [[nodiscard]] rhi::Extent2D      extent()             const noexcept override { return m_extent; }
        [[nodiscard]] rhi::Format        format()             const noexcept override { return m_format; }
        [[nodiscard]] rhi::TextureHandle current_backbuffer() const noexcept override { return m_backbuffer; }

        uint32_t acquire_next_image()             override;
        void     present()                        override;
        void     resize(rhi::Extent2D new_extent) override;

    private:
        MetalDevice*       m_device      = nullptr;
        void*              m_metal_layer = nullptr;     // CAMetalLayer* (__bridge_retained)
        void*              m_drawable    = nullptr;     // id<CAMetalDrawable> (fotograma actual)
        rhi::TextureHandle m_backbuffer  = {};
        rhi::Extent2D      m_extent      = {};
        rhi::Format        m_format      = rhi::Format::BGRA8_Unorm;
        uint32_t           m_image_count = 3;
    };
} // namespace anxiety::rendering::backend::metal

#endif // ANXIETY_BACKEND_METAL
