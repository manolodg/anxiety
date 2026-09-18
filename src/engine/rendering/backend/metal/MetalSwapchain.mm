#ifdef ANXIETY_BACKEND_METAL

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/AppKit.h>               // NSWindow, NSView (solo macOS)

#include "MetalSwapchain.h"
#include "MetalDevice.h"
#include "MetalHelpers.h"
#include "Logger.h"

namespace anxiety::rendering::backend::metal {
    // Construcción -------------------------------------------------------------------------------
    MetalSwapchain::MetalSwapchain(MetalDevice* device, const anxiety::rendering::rhi::SwapchainDesc& desc) : m_device(device), m_extent(desc.extent), m_format(desc.format), m_image_count(desc.image_count > 0 ? desc.image_count : 3) {
        @autoreleasepool {
            id<MTLDevice> dev = (__bridge id<MTLDevice>)device->mtl_device();

            // Obtiene el NSWindow a partir del handle nativo y engancha un CAMetalLayer a su content view.
            NSWindow* win = (__bridge NSWindow*)desc.native_window_handle;
            if (!win) {
                LOGF_ERROR("Metal", "MetalSwapchain: nativeWindowHandle es nil");
                return;
            }

            NSView* view = [win contentView];

            CAMetalLayer* layer = [CAMetalLayer layer];
            layer.device               = dev;
            layer.pixelFormat          = to_MTL_pixel_format(m_format);
            layer.drawableSize         = CGSizeMake((CGFloat)m_extent.width, (CGFloat)m_extent.height);
            layer.maximumDrawableCount = (NSUInteger)m_image_count;
            layer.displaySyncEnabled   = desc.vsync ? YES : NO;
            layer.framebufferOnly      = YES;                 // texturas exclusivas de render target

            [view setLayer:layer];
            [view setWantsLayer:YES];

            // Retiene la layer durante toda la vida útil del swapchain.
            m_metal_layer = (__bridge_retained void*)layer;

            LOGF_INFO("Metal", "MetalSwapchain creado ({}x{}, {} imágenes)", m_extent.width, m_extent.height, m_image_count);
        }
    }

    MetalSwapchain::~MetalSwapchain() {
        // Libera la referencia al drawable actual, si existe.
        if (m_drawable) {
            (void)(__bridge_transfer id<CAMetalDrawable>)m_drawable;
            m_drawable = nullptr;
        }
        // Anula el registro del slot de textura del backbuffer.
        if (m_backbuffer.is_valid()) {
            m_device->unregister_texture(m_backbuffer);
            m_backbuffer = {};
        }
        if (m_metal_layer) {
            (void)(__bridge_transfer CAMetalLayer*)m_metal_layer;
            m_metal_layer = nullptr;
        }
    }

    // Operaciones por fotograma ------------------------------------------------------------------
    uint32_t MetalSwapchain::acquire_next_image() {
        @autoreleasepool {
            CAMetalLayer* layer = (__bridge CAMetalLayer*)m_metal_layer;

            // Libera el drawable anterior (si aún no se presentó).
            if (m_drawable) {
                (void)(__bridge_transfer id<CAMetalDrawable>)m_drawable;
                m_drawable = nullptr;
            }
            // Anula el registro del slot de backbuffer anterior.
            if (m_backbuffer.is_valid()) {
                m_device->unregister_texture(m_backbuffer);
                m_backbuffer = {};
            }

            id<CAMetalDrawable> drawable = [layer nextDrawable];
            if (!drawable) {
                LOGF_ERROR("Metal", "nextDrawable devolvió nil");
                return 0;
            }

            m_drawable = (__bridge_retained void*)drawable;

            // Registra la textura del drawable como textura externa en el device.
            id<MTLTexture> tex = drawable.texture;
            m_backbuffer = m_device->register_external_texture((__bridge void*)tex, (uint32_t)tex.width, (uint32_t)tex.height);

            return 0;                                 // Índice de imagen virtual único (Metal no expone índices explícitos)
        }
    }

    void MetalSwapchain::present() {
        if (!m_drawable) return;
        @autoreleasepool {
            id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)m_device->mtl_command_queue();
            id<MTLCommandBuffer> cmd = [queue commandBuffer];
            [cmd presentDrawable:(__bridge id<CAMetalDrawable>)m_drawable];
            [cmd commit];
        }
    }

    void MetalSwapchain::resize(anxiety::rendering::rhi::Extent2D new_extent) {
        if (new_extent.width  == m_extent.width && new_extent.height == m_extent.height) return;

        m_extent = new_extent;

        @autoreleasepool {
            CAMetalLayer* layer = (__bridge CAMetalLayer*)m_metal_layer;
            layer.drawableSize = CGSizeMake((CGFloat)new_extent.width, (CGFloat)new_extent.height);
        }

        LOGF_INFO("Metal", "MetalSwapchain redimensionado a {}x{}", new_extent.width, new_extent.height);
    }
} // namespace anxiety::rendering::backend::metal

#endif // ANXIETY_BACKEND_METAL
