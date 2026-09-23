#pragma once

#include "Renderer2DTypes.h"
#include "TextureAtlas.h"

#include <vector>

namespace anxiety::rendering2d {

    // Entrada de lote (un cuadrado texturado) ----------------------------------------------------
    struct BatchEntry {
        Vec2          position;
        Vec2          scale    = { 1,1 };
        float         rotation = 0.0f;
        Vec2          pivot    = { 0.5f, 0.5f };
        TextureRegion region;
        Color         color    = Color::white();
        BlendMode     blend    = BlendMode::Alpha;
        SortingLayer  sorting;

        bool          flip_X = false, flip_Y = false;
    };

    // Lote de sprites ----------------------------------------------------------------------------
    // Acumula los Sprites por frame, ordena y emite las mínimas llamadas de dibujado.
    class SpriteBatch {
    public:
        static constexpr uint32_t k_max_sprites_per_batch = 16384u;
        static constexpr uint32_t k_max_texture_slots     = 8u;    // mínimo GLES 3.0

        explicit SpriteBatch(R2DTier tier = R2DTier::Desktop);

        void begin();
        void draw(const BatchEntry& entry);
        void end (const Camera2D&   camera);

        // Primitivas de dibujado directo
        void draw_rect(const Vec2& pos, const Vec2& size, const Color& color, int16_t layer = 0);
        void draw_line(const Vec2& a, const Vec2& b, float width, const Color& color, int16_t layer = 0);

        // Estadisticas
        uint32_t sprite_count()    const { return m_sprite_count; }
        uint32_t batch_count()     const { return m_batch_count; }
        uint32_t draw_call_count() const { return m_draw_call_count; }

    private:
        R2DTier  m_tier;

        struct Batch {
            R2DTextureHandle  texture;
            BlendMode         blend;
            int16_t           layer;
            uint32_t          first_vert;
            uint32_t          num_verts;                    // multiplo de 6 (2 triangulos por cuadrado)
        };

        std::vector<SpriteVertex> m_vertices;
        std::vector<Batch>        m_batches;
        std::vector<BatchEntry>   m_entries;                // copia ordenada

        uint32_t m_sprite_count    = 0;
        uint32_t m_batch_count     = 0;
        uint32_t m_draw_call_count = 0;

        // Handle de buffer de GPU simulado
        uint32_t m_vbo = 0;
        uint32_t m_vao = 0;

        void sort();
        void build_batches();
        void emit_quad(const BatchEntry& e);
        void flush_batches(const Camera2D& cam);
        void build_ortho_matrix(const Camera2D& cam, float out[16]) const;
    };

} // namespace anxiety::rendering2d
