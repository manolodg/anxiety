#pragma once

#include "RHITypes.h"

#include <cstdint>
#include <vector>

namespace anxiety::rendering::rhi {
    // DescriptorType -----------------------------------------------------------------------------
    enum class DescriptorType : uint32_t {
        UniformBuffer,                              // CBV — datos constantes/uniformes leídos por los shaders
        Texture,                                    // SRV — textura muestreada
        Sampler,                                    // estado de sampler
        StorageBuffer,                              // UAV — buffer de lectura-escritura
    };

    // DescriptorBinding --------------------------------------------------------------------------
    // Un slot dentro de un layout de descriptor set.
    //   binding — índice de registro del shader (b0, t0, s0, u0 en HLSL)
    //   type    — qué tipo de recurso ocupa este slot
    //   count   — número de descriptores (tamaño de array; 1 si no es array)
    struct DescriptorBinding {
        uint32_t       binding = 0;
        DescriptorType type = DescriptorType::UniformBuffer;
        uint32_t       count = 1;
    };

    // DescriptorSetLayout ------------------------------------------------------------------------
    // Describe el conjunto completo de slots de recursos de un descriptor set. Se pasa a
    // IDevice::createDescriptorSet() y se embebe en PipelineDesc para que el backend pueda construir
    // el root signature / pipeline layout correspondiente.
    struct DescriptorSetLayout {
        std::vector<DescriptorBinding> bindings;
    };

    // DescriptorWrite ----------------------------------------------------------------------------
    // Vincula un recurso concreto a un slot al actualizar un descriptor set. Solo se rellena el
    // campo que coincide con el tipo del binding; el otro debe quedar en su valor por defecto.
    struct DescriptorWrite {
        uint32_t       binding = 0;
        DescriptorType type = DescriptorType::UniformBuffer;
        BufferHandle   buffer = {};                                // para UniformBuffer / StorageBuffer
        TextureHandle  texture = {};                                // para Texture
    };

    // IDescriptorSet -----------------------------------------------------------------------------
    // Un conjunto de bindings de recursos, actualizable. Creado por IDevice::createDescriptorSet().
    // Layout inmutable; solo los recursos vinculados pueden cambiar vía update().
    //
    // Uso típico:
    //   auto ds = device.create_descriptor_set({ .bindings = { {0, UniformBuffer} } });
    //   ds->update({ { 0, UniformBuffer, myBuffer } });
    //   // por fotograma:
    //   cmd.bind_descriptor_set(0, *ds);
    class IDescriptorSet {
    public:
        virtual ~IDescriptorSet() = default;

        // Actualiza uno o más bindings de recursos. Puede llamarse cualquier número de veces, pero
        // no mientras la GPU esté ejecutando un pase que use este set.
        virtual void update(const std::vector<DescriptorWrite>& writes) = 0;

    protected:
        IDescriptorSet() = default;
        IDescriptorSet(const IDescriptorSet&) = delete;
        IDescriptorSet& operator=(const IDescriptorSet&) = delete;
    };
} // namespace anxiety::rendering::rhi