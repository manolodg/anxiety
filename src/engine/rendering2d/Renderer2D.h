#pragma once

#include "SpriteBatch.h"
#include "SpriteInstancer.h"
#include "TextureAtlas.h"

#include <memory>
#include <functional>

namespace anxiety::rendering2d {

    // Contexto de frame --------------------------------------------------------------------------
    struct R2DFrameContext {
        Camera2D     camera;
        float        delta_time = 0.f;
        uint32_t     frame_index = 0;
        float        sim_time = 0.f;
    };

    // Renderer 2D principal ----------------------------------------------------------------------
    class Renderer2D {
    public:
        explicit Renderer2D(R2DTier tier = R2DTier::Desktop, uint32_t atlas_page_size = 2048);

        // Ciclo de vida del frame ----------------------------------------------------------------
        void begin_frame(const R2DFrameContext& ctx);
        void end_frame();

        // API del sprite -------------------------------------------------------------------------
        void draw_sprite(const Vec2& pos, const Vec2& size, const TextureRegion& region, const Color& color = Color::white(), float rotation = 0.f, const SortingLayer& sorting = {}, BlendMode blend = BlendMode::Alpha, bool flip_x = false, bool flip_y = false);

        // Variante de GPU instanciada (preferida por > 100 sprites de la misma textura)
        void submit_instance(const SpriteInstance& inst);
        void flush_instances(R2DTextureHandle texture);

        // API del atlas --------------------------------------------------------------------------
        TextureRegion   load_texture(const std::string& name, const uint8_t* rgba, uint32_t w, uint32_t h);
        TextureRegion   find_texture(const std::string& name) const;
        void            evict_texture(const std::string& name);
        
        TextureAtlas& atlas() { return m_atlas; }

        // API de dibujado directo ----------------------------------------------------------------
        void draw_rect(const Vec2& pos, const Vec2& size, const Color& color, int16_t layer = 0);
        void draw_line(const Vec2& a, const Vec2& b, float width, const Color& c, int16_t l = 0);

        // Camara ---------------------------------------------------------------------------------
        const Camera2D& camera() const { return m_ctx.camera; }
        Vec2 world_to_screen(const Vec2& w) const { return m_ctx.camera.world_to_screen(w); }
        Vec2 screen_to_world(const Vec2& s) const { return m_ctx.camera.screen_to_world(s); }

        // Integración 3D hibrida -----------------------------------------------------------------
        // Permite el dibujado del contenido 2D sompuesto sobre/bajo una escena 3D.
        void     set_render_target(uint32_t fbo)       { m_fbo = fbo; }
        uint32_t render_target()                 const { return m_fbo; }

        // Escalabilidad --------------------------------------------------------------------------
        void    set_tier(R2DTier tier);
        R2DTier tier()                  const { return m_tier; }

        // Estadisticas ---------------------------------------------------------------------------
        const R2DStats& stats() const { return m_stats; }

        // Overlay de depuración ------------------------------------------------------------------
        void print_debug_overlay() const;

    private:
        R2DTier         m_tier;
        R2DFrameContext m_ctx;
        TextureAtlas    m_atlas;
        SpriteBatch     m_batch;
        SpriteInstancer m_instancer;
        R2DStats        m_stats;
        uint32_t        m_fbo        = 0;                       // 0 = framebuffer por defecto
        bool            m_inFrame    = false;
    };

    // Implementación de cámara -------------------------------------------------------------------

    inline void Camera2D::build_ortho_matrix(float out[16]) const {
        float pw = vp_w / zoom;
        float ph = vp_h / zoom;
        float s = std::sinf(rotation), c = std::cosf(rotation);
        // Traduce a la posición de la cámara, entonces rota, entonces proyección orto
        // Simplificado: sin rotación por ahora
        (void)s; (void)c;
        float l = position.x - pw * 0.5f, r = position.x + pw * 0.5f;
        float b = position.y - ph * 0.5f, t = position.y + ph * 0.5f;
        float n = near_z, f = far_z;
        float* m = out;
        m[0]  = 2 / (r - l);        m[1]  = 0;                  m[2]  = 0;                  m[3]  = 0;
        m[4]  = 0;                  m[5]  = 2 / (t - b);        m[6]  = 0;                  m[7]  = 0;
        m[8]  = 0;                  m[9]  = 0;                  m[10] = -2 / (f - n);       m[11] = 0;
        m[12] = -(r + l) / (r - l); m[13] = -(t + b) / (t - b); m[14] = -(f + n) / (f - n); m[15] = 1;
    }

    inline Vec2 Camera2D::world_to_screen(const Vec2& world) const {
        float pw = vp_w / zoom, ph = vp_h / zoom;
        float nx = (world.x - position.x) / (pw * 0.5f);
        float ny = (world.y - position.y) / (ph * 0.5f);

        return { (nx + 1.f) * 0.5f * vp_w + vp_x, (1.f - ny) * 0.5f * vp_h + vp_y };
    }

    inline Vec2 Camera2D::screen_to_world(const Vec2& screen) const {
        float pw = vp_w / zoom, ph = vp_h / zoom;
        float nx = ((screen.x - vp_x) / vp_w) * 2.f - 1.f;
        float ny = 1.f - ((screen.y - vp_y) / vp_h) * 2.f;

        return { position.x + nx * (pw * 0.5f), position.y + ny * (ph * 0.5f) };
    }

} // namespace engine::rendering2d
