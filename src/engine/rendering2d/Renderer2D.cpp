#include "Renderer2D.h"

#include <cstdio>
#include <chrono>

namespace anxiety::rendering2d {

    Renderer2D::Renderer2D(R2DTier tier, uint32_t atlas_page_size) : m_tier(tier), m_atlas(atlas_page_size, atlas_page_size), m_batch(tier), m_instancer() {}

    void Renderer2D::begin_frame(const R2DFrameContext& ctx) {
        m_ctx     = ctx;
        m_inFrame = true;
        m_batch.begin();
        m_instancer.begin();
        m_stats = {};
    }

    void Renderer2D::end_frame() {
        if (!m_inFrame) return;

        m_batch.end(m_ctx.camera);
        m_stats.sprite_count    = m_batch.sprite_count();
        m_stats.batch_count     = m_batch.batch_count();
        m_stats.draw_call_count = m_batch.draw_call_count();
        m_stats.atlas_memory_MB = m_atlas.memory_MB();

        m_inFrame = false;
    }

    void Renderer2D::draw_sprite(const Vec2& pos, const Vec2& size, const TextureRegion& region, const Color& color, float rotation, const SortingLayer& sorting, BlendMode blend, bool flip_x, bool flip_y) {
        BatchEntry e;
        e.position = pos;
        e.scale    = size;
        e.rotation = rotation;
        e.region   = region;
        e.color    = color;
        e.blend    = blend;
        e.sorting  = sorting;
        e.flip_X   = flip_x;
        e.flip_Y   = flip_y;

        m_batch.draw(e);
    }

    void Renderer2D::submit_instance(const SpriteInstance& inst) {
        m_instancer.submit(inst);
    }

    void Renderer2D::flush_instances(R2DTextureHandle texture) {
        m_instancer.end(m_ctx.camera, texture);
        m_stats.sprite_count += m_instancer.instance_count();
        m_instancer.begin();
    }

    TextureRegion Renderer2D::load_texture(const std::string& name, const uint8_t* rgba, uint32_t w, uint32_t h) {
        return m_atlas.pack(name, rgba, w, h);
    }

    TextureRegion Renderer2D::find_texture(const std::string& name) const {
        return const_cast<TextureAtlas&>(m_atlas).find(name);
    }

    void Renderer2D::evict_texture(const std::string& name) {
        m_atlas.evict(name);
    }

    void Renderer2D::draw_rect(const Vec2& pos, const Vec2& size, const Color& color, int16_t layer) {
        m_batch.draw_rect(pos, size, color, layer);
    }

    void Renderer2D::draw_line(const Vec2& a, const Vec2& b, float width, const Color& c, int16_t layer) {
        m_batch.draw_line(a, b, width, c, layer);
    }

    void Renderer2D::set_tier(R2DTier tier) {
        m_tier = tier;
    }

    void Renderer2D::print_debug_overlay() const {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "┌───────────────── Renderer2D ────────────────┐\n"
            "│ sprites   =%-5u  batches    =%  -5u         │\n"
            "│ draw_calls=%-5u  tiles      =%-5u           │\n"
            "│ particles =%-5u  atlas_pages=%-3u           │\n"
            "│ atlas_memory=%.1fMB                         │\n"
            "└─────────────────────────────────────────────┘",
            m_stats.sprite_count,    m_stats.batch_count,
            m_stats.draw_call_count, m_stats.tile_count,
            m_stats.particle_count,  m_atlas.page_count(),
            m_stats.atlas_memory_MB);
        std::puts(buf);
    }

} // namespace anxiety::rendering2d
