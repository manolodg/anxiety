#ifdef ANXIETY_BACKEND_METAL

// MetalDescriptorSet.h — implementación de IDescriptorSet para Metal.
// Metal no tiene descriptor sets explícitos; los bindings se aplican directamente
// al command encoder en el momento del draw.
#pragma once

#include "../../rhi/IDescriptorSet.h"
#include <vector>

namespace anxiety::rendering::backend::metal {

    class MetalDevice;

    // Un binding de recurso almacenado en el set.
    struct MetalBinding {
        rhi::DescriptorType type;
        uint32_t            binding;
        void*               resource;       // id<MTLBuffer> o id<MTLTexture>
        uint64_t            buffer_offset;  // offset en bytes dentro del buffer (binding de uniform)
    };

    class MetalDescriptorSet final : public rhi::IDescriptorSet {
    public:
        MetalDescriptorSet(MetalDevice* device, const rhi::DescriptorSetLayout& layout);
        ~MetalDescriptorSet() override = default;

        // IDescriptorSet
        void update(const std::vector<rhi::DescriptorWrite>& writes) override;

        // Llamado por MetalCommandBuffer::bind_descriptor_set.
        // encoder_ptr debe ser un id<MTLRenderCommandEncoder> válido convertido a void*.
        void apply(void* encoder_ptr, uint32_t set_index) const;

    private:
        MetalDevice*              m_device;
        rhi::DescriptorSetLayout  m_layout;
        std::vector<MetalBinding> m_bindings;
    };

} // namespace anxiety::rendering::backend::metal

#endif // ANXIETY_BACKEND_METAL
