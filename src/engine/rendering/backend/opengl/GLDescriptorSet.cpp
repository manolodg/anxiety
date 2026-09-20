#ifdef ANXIETY_BACKEND_OPENGL

#include "GLDescriptorSet.h"
#include "GLDevice.h"
#include "Logger.h"

namespace anxiety::rendering::backend::opengl {

    // update — almacena los handles de recursos de GL resueltos para todas las escrituras ----------
    void GLDescriptorSet::update(const std::vector<rhi::DescriptorWrite>& writes) {
        for (const auto& w : writes) {
            // Busca o crea un slot para este (índice de binding, tipo). El tipo forma parte de la clave:
            // el registro HLSL b0 (UBO) y t0 (textura) comparten número pero no son el mismo recurso.
            GLBinding* existing = nullptr;
            for (auto& b : m_bindings) {
                if (b.binding == w.binding && b.type == w.type) { existing = &b; break; }
            }
            if (!existing) {
                m_bindings.push_back({});
                existing = &m_bindings.back();
            }

            existing->binding = w.binding;
            existing->type    = w.type;

            using DT = anxiety::rendering::rhi::DescriptorType;
            switch (w.type) {
            case DT::UniformBuffer:
            case DT::StorageBuffer:
            {
                if (!w.buffer.is_valid()) {
                    LOGF_WARNING("GLDescriptorSet", "update(): el binding {} tiene un handle de buffer inválido.", w.binding);
                    existing->gl_name    = 0;
                    existing->buf_offset = 0;
                    existing->buf_size   = 0;
                    break;
                }
                const GLBufferSlot& slot = m_device->buf_slot(w.buffer);
                existing->gl_name    = slot.name;
                existing->buf_offset = 0;       // rango completo del buffer
                existing->buf_size   = slot.size;
                break;
            }

            case DT::Texture:
            {
                if (!w.texture.is_valid()) {
                    LOGF_WARNING("GLDescriptorSet", "update(): el binding {} tiene un handle de textura inválido.", w.binding);
                    existing->gl_name = 0;
                    break;
                }
                // Protección contra el handle centinela del backbuffer (id == 1)
                if (w.texture.id == 1) {
                    existing->gl_name = 0;  // el framebuffer por defecto no se puede vincular como sampler
                    break;
                }
                const GLTextureSlot& slot = m_device->tex_slot(w.texture);
                existing->gl_name = slot.name;
                break;
            }

            case DT::Sampler:
                // Los objetos sampler se almacenan por separado (creados fuera de este set).
                // Convención: BufferHandle::id codifica (nombre del objeto sampler de GL + 1).
                if (w.buffer.is_valid()) {
                    existing->gl_name = static_cast<GLuint>(w.buffer.id - 1);
                } else {
                    existing->gl_name = 0;
                }
                break;
            }
        }
    }

    // apply_to_context — vincula todos los recursos almacenados al contexto de GL actual -----------
    void GLDescriptorSet::apply_to_context() const {
        using DT = rhi::DescriptorType;
        for (const auto& b : m_bindings) {
            if (b.gl_name == 0) continue;  // se omiten los bindings no resueltos

            switch (b.type) {
            case DT::UniformBuffer:
                glBindBufferRange(GL_UNIFORM_BUFFER,
                                  b.binding,
                                  b.gl_name,
                                  static_cast<GLintptr>(b.buf_offset),
                                  static_cast<GLsizeiptr>(b.buf_size));
                break;

            case DT::StorageBuffer:
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, b.binding, b.gl_name);
                break;

            case DT::Texture:
                glActiveTexture(GL_TEXTURE0 + b.binding);
                glBindTexture(GL_TEXTURE_2D, b.gl_name);
                break;

            case DT::Sampler:
                glBindSampler(b.binding, b.gl_name);
                break;
            }
        }
    }

} // namespace anxiety::rendering::backend::opengl

#endif // ANXIETY_BACKEND_OPENGL
