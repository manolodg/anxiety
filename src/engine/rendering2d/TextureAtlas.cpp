#include "TextureAtlas.h"

#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace anxiety::rendering2d {
    TextureAtlas::TextureAtlas(uint32_t page_w, uint32_t page_h) : m_page_w(page_w), m_page_h(page_h) {
        add_page();
    }

    void TextureAtlas::add_page() {
        AtlasPage page;
        page.page_index = (uint32_t)m_pages.size();
        page.width      = m_page_w;
        page.height     = m_page_h;
        page.gpu_handle = k_r_2D_invalid;
        page.memory_MB  = (m_page_w * m_page_h * 4) / (1024.0f * 1024.0f);
        m_pages.push_back(page);

        // Pixel buffer
        m_pixel_data.push_back(std::vector<uint8_t>(m_page_w * m_page_h * 4, 0));

        // Root packing node for this page
        PackNode root;
        root.x = 0;        root.y = 0;
        root.w = m_page_w; root.h = m_page_h;
        root.used  = false;
        root.left  = 0xFFFFFFFFu;
        root.right = 0xFFFFFFFFu;
        m_nodes.push_back(root);
    }

    bool TextureAtlas::try_pack(uint32_t page_idx, uint32_t w, uint32_t h, uint32_t& out_x, uint32_t& out_y) {
        // Auto empaquetado simple por página (guillotine simplificado)
        // Usa un simple tracker shelf-Y por página
        // Almacenado como dos float extra por página: (current_x, shelf_y)
        // Por simplicidad, manten un mapa de pixel usado por página
        // Actualmente haz un simple escaneo: encuentra la primera línea libre en la que encaja
        // Simplificado: usa el scan de la página lineal con padding
        uint32_t pad_w = w + k_padding;
        uint32_t pad_h = h + k_padding;

        // Cursor por página: sigue el estante actual X e Y
        struct Cursor { uint32_t x = k_padding, y = k_padding, shelf_h = 0; };
        
        static std::vector<Cursor> cursors;
        while (cursors.size() <= page_idx) {
            cursors.push_back({});
        }

        Cursor& cur = cursors[page_idx];

        if (cur.x + pad_w > m_page_w - k_padding) {
            // Siguiente estante
            cur.y      += cur.shelf_h + k_padding;
            cur.x       = k_padding;
            cur.shelf_h = 0;
        }
        if (cur.y + pad_h > m_page_h - k_padding) return false;

        out_x = cur.x; out_y = cur.y;
        cur.x      += pad_w;
        cur.shelf_h = std::max(cur.shelf_h, pad_h);

        return true;
    }

    TextureRegion TextureAtlas::pack(const std::string& name,
        const uint8_t* rgba, uint32_t w, uint32_t h) {
        // se ha empaquetado ya?
        auto it = m_entries.find(name);
        if (it != m_entries.end()) return it->second.region;

        uint32_t px = 0, py = 0;
        uint32_t page_idx = 0xFFFFFFFFu;
        for (uint32_t pi = 0; pi < (uint32_t)m_pages.size(); ++pi) {
            if (try_pack(pi, w, h, px, py)) { page_idx = pi; break; }
        }
        if (page_idx == 0xFFFFFFFFu) {
            add_page();
            page_idx = (uint32_t)m_pages.size() - 1;
            if (!try_pack(page_idx, w, h, px, py))
                throw std::invalid_argument("TextureAtlas::pack: el sprite (" + std::to_string(w) + "x" + std::to_string(h) + ") no cabe ni en una página nueva.");
        }

        // Copia pixeles en el buffer
        auto& buf = m_pixel_data[page_idx];
        for (uint32_t row = 0; row < h; ++row) {
            const uint8_t* src = rgba + row * w * 4;
            uint8_t* dst = buf.data() + ((py + row) * m_page_w + px) * 4;
            std::memcpy(dst, src, w * 4);
        }

        TextureRegion reg;
        reg.texture = m_pages[page_idx].gpu_handle;          // ha de ser invalido hasta el volcado
        reg.u0      = (float)px / (float)m_page_w;
        reg.v0      = (float)py / (float)m_page_h;
        reg.u1      = (float)(px + w) / (float)m_page_w;
        reg.v1      = (float)(py + h) / (float)m_page_h;
        reg.pixel_w = (uint16_t)w;
        reg.pixel_h = (uint16_t)h;

        Entry e;
        e.region = reg;
        e.page   = page_idx;
        e.dirty  = true;
        m_entries[name] = e;

        return reg;
    }

    TextureRegion TextureAtlas::find(const std::string& name) const {
        auto it = m_entries.find(name);
        return it != m_entries.end() ? it->second.region : TextureRegion{};
    }

    bool TextureAtlas::contains(const std::string& name) const {
        return m_entries.count(name) > 0;
    }

    void TextureAtlas::flush_to_GPU() {
        for (uint32_t pi = 0; pi < (uint32_t)m_pages.size(); ++pi) {
            
            bool dirty = false;
            for (auto& [k, e] : m_entries) {
                if (e.page == pi && e.dirty) { dirty = true; break; }
            }

            if (!dirty && m_pages[pi].gpu_handle != k_r_2D_invalid) continue;
            upload_page(pi);
        }
    }

    void TextureAtlas::upload_page(uint32_t page_idx) {
        // Carga simulada a la GPU — asigna un handle estable
        if (m_pages[page_idx].gpu_handle == k_r_2D_invalid) m_pages[page_idx].gpu_handle = m_next_handle++;

        R2DTextureHandle h = m_pages[page_idx].gpu_handle;
        // Actualiza todas las entradas en esta página con el handle de textura real
        for (auto& [k, e] : m_entries) {
            if (e.page == page_idx) {
                e.region.texture = h;
                e.dirty          = false;
            }
        }
    }

    void TextureAtlas::evict(const std::string& name) {
        m_entries.erase(name);
    }

    float TextureAtlas::memory_MB() const {
        float total = 0.f;
        for (auto& p : m_pages) {
            total += p.memory_MB;
        }

        return total;
    }

    // RuntimeAtlasBuilder ------------------------------------------------------------------------
    void RuntimeAtlasBuilder::enqueue(const std::string& name, const uint8_t* rgba, uint32_t w, uint32_t h) {
        if (m_atlas.contains(name)) return;

        PendingEntry e;
        e.name = name;
        e.pixels.assign(rgba, rgba + w * h * 4);
        e.w = w; e.h = h;
        m_pending.push_back(std::move(e));
    }

    void RuntimeAtlasBuilder::commit() {
        for (auto& p : m_pending) {
            m_atlas.pack(p.name, p.pixels.data(), p.w, p.h);
        }

        m_pending.clear();
        m_atlas.flush_to_GPU();
    }

} // namespace anxiety::rendering2d
