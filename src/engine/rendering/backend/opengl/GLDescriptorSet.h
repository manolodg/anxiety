#pragma once

#ifdef ANXIETY_BACKEND_OPENGL

#include "GLHelpers.h"
#include "../../rhi/IDescriptorSet.h"

#include <vector>

namespace anxiety::rendering::backend::opengl {

    class GLDevice; // forward

    // GLBinding — un binding de recurso resuelto para un único slot de descriptor.
    struct GLBinding {
        rhi::DescriptorType type       = rhi::DescriptorType::UniformBuffer;
        uint32_t            binding    = 0;
        GLuint              gl_name    = 0;   // nombre del objeto buffer o textura de GL
        uint64_t            buf_offset = 0;   // offset en bytes dentro del buffer
        uint64_t            buf_size   = 0;   // tamaño en bytes del rango vinculado
    };

    // GLDescriptorSet — implementación concreta de rhi::IDescriptorSet.
    //
    // update() almacena los nombres de objeto de GL resueltos en m_bindings.
    // apply_to_context() recorre m_bindings y maneja la máquina de estados de GL para vincular
    // cada recurso en la unidad / base / punto de binding correctos.
    class GLDescriptorSet final : public rhi::IDescriptorSet {
    public:
        explicit GLDescriptorSet(const rhi::DescriptorSetLayout& layout, GLDevice* device) : m_layout(layout), m_device(device) {}

        ~GLDescriptorSet() override = default;

        // IDescriptorSet
        void update(const std::vector<rhi::DescriptorWrite>& writes) override;

        // Se llama desde la cola de comandos diferida durante execute_all().
        // Vincula todos los recursos de m_bindings al contexto de GL actual.
        void apply_to_context() const;

    private:
        std::vector<GLBinding>   m_bindings;
        rhi::DescriptorSetLayout m_layout;
        GLDevice*                m_device = nullptr;
    };

} // namespace anxiety::rendering::backend::opengl

#endif // ANXIETY_BACKEND_OPENGL
