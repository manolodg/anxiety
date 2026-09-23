#pragma once

#include "Renderer2DTypes.h"
#include "TextureAtlas.h"

#include <vector>

namespace anxiety::rendering2d {

    // Instanciador de sprite orientado a la GPU --------------------------------------------------
    // Recolecta estructuras SpriteInstance, sube a GPU VBO, dibuja ocn una llamada de dibujado por
    // textura (glDrawArraysInstanced).
    class SpriteInstancer {
    public:
        static constexpr uint32_t k_max_instances = 65536u;

        explicit SpriteInstancer(uint32_t max_instances = k_max_instances);

        void begin();
        void submit(const SpriteInstance& inst);
        void end(const Camera2D& camera, R2DTextureHandle texture);

        uint32_t instance_count() const { return (uint32_t)m_instances.size(); }

        // Organizado por zOrder dentro de una capa
        void sort();

    private:
        std::vector<SpriteInstance> m_instances;
        uint32_t                    m_instance_VBO = 0;
        uint32_t                    m_quad_VBO     = 0;
        uint32_t                    m_vao          = 0;
        bool                        m_gpu_init     = false;

        void init_GPU();
        void upload_instances();
        void build_ortho(const Camera2D& cam, float out[16]) const;
    };

} // namespace anxiety::rendering2d
