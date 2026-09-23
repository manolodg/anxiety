#include "SpriteInstancer.h"

#include <algorithm>
#include <cmath>

namespace anxiety::rendering2d {

    SpriteInstancer::SpriteInstancer(uint32_t max_instances) {
        m_instances.reserve(max_instances);
    }

    void SpriteInstancer::init_GPU() {
        if (m_gpu_init) return;
        // Configuración de GPU simulada:
        // 1. Genera VAO, quad VBO (unidad de quad [-0.5,0.5]^2), instancia VBO
        // 2. Configura los punteros de atributos del vertice con divisor=1 instanciado por instancia de datos
        m_vao          = 1;
        m_quad_VBO     = 2;
        m_instance_VBO = 3;
        m_gpu_init     = true;
    }

    void SpriteInstancer::begin() {
        m_instances.clear();
    }

    void SpriteInstancer::submit(const SpriteInstance& inst) {
        if (m_instances.size() < k_max_instances) m_instances.push_back(inst);
    }

    void SpriteInstancer::sort() {
        std::stable_sort(m_instances.begin(), m_instances.end(),
            [](const SpriteInstance& a, const SpriteInstance& b) { return a.z_order < b.z_order; });
    }

    void SpriteInstancer::build_ortho(const Camera2D& cam, float out[16]) const {
        float pw = cam.vp_w / cam.zoom;
        float ph = cam.vp_h / cam.zoom;
        float l = cam.position.x - pw * 0.5f, r = cam.position.x + pw * 0.5f;
        float b = cam.position.y - ph * 0.5f, t = cam.position.y + ph * 0.5f;
        float n = cam.near_z, f = cam.far_z;
        float* m = out;
        m[0]  = 2 / (r - l);        m[1]  = 0;                  m[2]  = 0;                  m[3]  = 0;
        m[4]  = 0;                  m[5]  = 2 / (t - b);        m[6]  = 0;                  m[7]  = 0;
        m[8]  = 0;                  m[9]  = 0;                  m[10] = -2 / (f - n);       m[11] = 0;
        m[12] = -(r + l) / (r - l); m[13] = -(t + b) / (t - b); m[14] = -(f + n) / (f - n); m[15] = 1;
    }

    void SpriteInstancer::upload_instances() {
        if (m_instances.empty()) return;
        // Simulado:  glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
        //            glBufferSubData(..., m_instances.data(), ...)
    }

    void SpriteInstancer::end(const Camera2D& camera, R2DTextureHandle /*texture*/) {
        if (m_instances.empty()) return;
        init_GPU();
        sort();
        upload_instances();

        float vp[16];
        build_ortho(camera, vp);

        // Simulado:
        // glBindVertexArray(m_vao);
        // glBindTexture(GL_TEXTURE_2D, texture);
        // glUniformMatrix4fv(u_viewProj, 1, GL_FALSE, vp);
        // glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, (GLsizei)m_instances.size());
    }

} // namespace anxiety::rendering2d
