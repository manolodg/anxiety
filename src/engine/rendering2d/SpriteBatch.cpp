#include "SpriteBatch.h"

#include <algorithm>
#include <cmath>

namespace anxiety::rendering2d {

    SpriteBatch::SpriteBatch(R2DTier tier) : m_tier(tier) {
        m_vertices.reserve(k_max_sprites_per_batch * 6);
        m_entries.reserve(k_max_sprites_per_batch);
    }

    void SpriteBatch::begin() {
        m_entries.clear();
        m_vertices.clear();
        m_batches.clear();
        
        m_sprite_count    = 0;
        m_batch_count     = 0;
        m_draw_call_count = 0;
    }

    void SpriteBatch::draw(const BatchEntry& entry) {
        if (m_entries.size() >= k_max_sprites_per_batch) return;
        m_entries.push_back(entry);
        ++m_sprite_count;
    }

    void SpriteBatch::draw_rect(const Vec2& pos, const Vec2& size, const Color& color, int16_t layer) {
        static TextureRegion white; // 1×1 white pixel

        BatchEntry e;
        e.position      = pos;
        e.scale         = size;
        e.region        = white;
        e.color         = color;
        e.sorting.layer = layer;

        draw(e);
    }

    void SpriteBatch::draw_line(const Vec2& a, const Vec2& b, float width, const Color& color, int16_t layer) {
        Vec2  dir    = (b - a);
        float len    = dir.length();
        Vec2  norm   = dir.normalized();
        Vec2  centre { (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f };
        float angle  = std::atan2f(dir.y, dir.x);

        BatchEntry e;
        e.position      = centre;
        e.scale         = { len, width };
        e.rotation      = angle;
        e.color         = color;
        e.sorting.layer = layer;

        draw(e);
    }

    void SpriteBatch::sort() {
        std::stable_sort(m_entries.begin(), m_entries.end(),
            [](const BatchEntry& a, const BatchEntry& b) {
                if (a.sorting.layer != b.sorting.layer)                         return a.sorting.layer < b.sorting.layer;
                if (a.sorting.order != b.sorting.order)                         return a.sorting.order < b.sorting.order;
                if (a.blend == BlendMode::Alpha && b.blend != BlendMode::Alpha) return false;                               // opaque before transparent
                
                return a.region.texture < b.region.texture;
            });
    }

    void SpriteBatch::emit_quad(const BatchEntry& e) {
        float hw = 0.5f * (e.region.pixel_w > 0 ? (float)e.region.pixel_w : 1.f) * e.scale.x;
        float hh = 0.5f * (e.region.pixel_h > 0 ? (float)e.region.pixel_h : 1.f) * e.scale.y;

        // Pivot offset
        float px = (e.pivot.x - 0.5f) * 2.f * hw;
        float py = (e.pivot.y - 0.5f) * 2.f * hh;

        // Corners in local space (centred at pivot)
        float cx[4] = { -hw - px,  hw - px,  hw - px, -hw - px };
        float cy[4] = { -hh - py, -hh - py,  hh - py,  hh - py };

        float s = std::sinf(e.rotation), c = std::cosf(e.rotation);
        uint32_t col = pack_color(e.color);

        // UV
        float u0 = e.region.u0, v0 = e.region.v0;
        float u1 = e.region.u1, v1 = e.region.v1;
        if (e.flip_X) std::swap(u0, u1);
        if (e.flip_Y) std::swap(v0, v1);

        float uvs[4][2] = { {u0,v0},{u1,v0},{u1,v1},{u0,v1} };

        SpriteVertex verts[4];
        for (int i = 0; i < 4; i++) {
            float rx = c * cx[i] - s * cy[i];
            float ry = s * cx[i] + c * cy[i];

            verts[i].x     = e.position.x + rx;
            verts[i].y     = e.position.y + ry;
            verts[i].u     = uvs[i][0];
            verts[i].v     = uvs[i][1];
            verts[i].color = col;
        }
        // Two triangles: 0,1,2 and 0,2,3
        m_vertices.push_back(verts[0]); m_vertices.push_back(verts[1]); m_vertices.push_back(verts[2]);
        m_vertices.push_back(verts[0]); m_vertices.push_back(verts[2]); m_vertices.push_back(verts[3]);
    }

    void SpriteBatch::build_batches() {
        if (m_entries.empty()) return;

        R2DTextureHandle curTex   = 0xFFFFFFFFu;
        BlendMode        curBlend = BlendMode::Alpha;
        int16_t          curLayer = -32768;
        Batch            cur{};
        cur.first_vert = 0;
        cur.num_verts = 0;

        for (const auto& e : m_entries) {
            bool newBatch = (e.region.texture != curTex)
                || (e.blend != curBlend)
                || (e.sorting.layer != curLayer);

            if (newBatch) {
                if (cur.num_verts > 0) {
                    m_batches.push_back(cur);
                    ++m_batch_count;
                }

                curTex   = e.region.texture;
                curBlend = e.blend;
                curLayer = e.sorting.layer;
                cur.texture    = curTex;
                cur.blend      = curBlend;
                cur.layer      = curLayer;
                cur.first_vert = (uint32_t)m_vertices.size();
                cur.num_verts  = 0;
            }

            uint32_t before = (uint32_t)m_vertices.size();
            emit_quad(e);
            cur.num_verts += (uint32_t)m_vertices.size() - before;
        }

        if (cur.num_verts > 0) {
            m_batches.push_back(cur);
            ++m_batch_count;
        }
    }

    void SpriteBatch::build_ortho_matrix(const Camera2D& cam, float out[16]) const {
        float pw = cam.vp_w / cam.zoom;
        float ph = cam.vp_h / cam.zoom;

        float l = cam.position.x - pw * 0.5f, r = cam.position.x + pw * 0.5f;
        float b = cam.position.y - ph * 0.5f, t = cam.position.y + ph * 0.5f;
        float n = cam.near_z, f = cam.far_z;

        // Columna ortográfica mayor
        float* m = out;
        m[0]  = 2 / (r - l);        m[1]  = 0;                  m[2]  = 0;                  m[3]  = 0;
        m[4]  = 0;                  m[5]  = 2 / (t - b);        m[6]  = 0;                  m[7]  = 0;
        m[8]  = 0;                  m[9]  = 0;                  m[10] = -2 / (f - n);       m[11] = 0;
        m[12] = -(r + l) / (r - l); m[13] = -(t + b) / (t - b); m[14] = -(f + n) / (f - n); m[15] = 1;
    }

    void SpriteBatch::flush_batches(const Camera2D& cam) {
        if (m_batches.empty()) return;

        float vp[16];
        build_ortho_matrix(cam, vp);

        // Simulado
        for ([[maybe_unused]] const auto& batch : m_batches) {
            // liga batch.texture
            // configura el modo de mezcla
            // glDrawArrays(GL_TRIANGLES, batch.firstVert, batch.numVerts)
            ++m_draw_call_count;
        }
    }

    void SpriteBatch::end(const Camera2D& camera) {
        sort();
        build_batches();
        flush_batches(camera);
    }

} // namespace engine::rendering2d
