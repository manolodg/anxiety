// Objective-C++ — se compila solo en macOS/iOS con el framework Metal.
#ifdef ANXIETY_BACKEND_METAL

#import <Metal/Metal.h>

#include "MetalDescriptorSet.h"
#include "MetalDevice.h"
#include "MetalHelpers.h"

namespace anxiety::rendering::backend::metal {

    // Construcción -------------------------------------------------------------------------------
    MetalDescriptorSet::MetalDescriptorSet(MetalDevice* device, const rhi::DescriptorSetLayout& layout) : m_device(device) , m_layout(layout) {}

    // Actualización ------------------------------------------------------------------------------
    void MetalDescriptorSet::update(const std::vector<rhi::DescriptorWrite>& writes) {
        for (const auto& w : writes) {
            void*    resource      = nullptr;
            uint64_t buffer_offset = 0;

            if (w.type == rhi::DescriptorType::Texture) {
                if (w.texture.is_valid()) {
                    auto& slot = m_device->tex_slot(w.texture);
                    resource   = slot.texture;  // id<MTLTexture> como void*
                }
            } else {
                // UniformBuffer / StorageBuffer
                if (w.buffer.is_valid()) {
                    auto& slot    = m_device->buf_slot(w.buffer);
                    resource      = slot.buffer;  // id<MTLBuffer> como void*
                    buffer_offset = 0;
                }
            }

            // Reemplaza el binding existente con el mismo índice de slot y tipo, o agrega uno nuevo.
            bool found = false;
            for (auto& b : m_bindings) {
                if (b.binding == w.binding && b.type == w.type) {
                    b.resource      = resource;
                    b.buffer_offset = buffer_offset;
                    found = true;
                    break;
                }
            }
            if (!found) {
                m_bindings.push_back({ w.type, w.binding, resource, buffer_offset });
            }
        }
    }

    // Aplicación ---------------------------------------------------------------------------------
    void MetalDescriptorSet::apply(void* encoder_ptr, uint32_t /*set_index*/) const {
        if (!encoder_ptr) return;
        id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)encoder_ptr;

        for (const auto& b : m_bindings) {
            if (!b.resource) continue;

            switch (b.type) {
            case rhi::DescriptorType::UniformBuffer:
            case rhi::DescriptorType::StorageBuffer: {
                id<MTLBuffer> buf = (__bridge id<MTLBuffer>)b.resource;
                const NSUInteger slot = (b.type == rhi::DescriptorType::UniformBuffer ? k_msl_cbv_buffer_base : k_msl_uav_buffer_base) + b.binding;
                [enc setVertexBuffer:buf   offset:(NSUInteger)b.buffer_offset atIndex:slot];
                [enc setFragmentBuffer:buf offset:(NSUInteger)b.buffer_offset atIndex:slot];
                break;
            }
            case rhi::DescriptorType::Texture: {
                id<MTLTexture> tex = (__bridge id<MTLTexture>)b.resource;
                [enc setFragmentTexture:tex atIndex:b.binding];
                [enc setVertexTexture:tex   atIndex:b.binding];
                break;
            }
            case rhi::DescriptorType::Sampler:
                // Los samplers se incrustan en el pipeline como samplers estáticos (ver MetalCommandBuffer::bind_pipeline).
                break;
            default:
                break;
            }
        }
    }

} // namespace anxiety::rendering::backend::metal

#endif // ANXIETY_BACKEND_METAL
