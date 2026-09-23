#pragma once

#include "Renderer2DTypes.h"

#include <vector>
#include <string>
#include <unordered_map>

namespace anxiety::rendering2d {

    // Descriptor de página de atlas --------------------------------------------------------------
    struct AtlasPage {
        R2DTextureHandle gpu_handle = k_r_2D_invalid;
        uint32_t         width      = 2048;
        uint32_t         height     = 2048;
        uint32_t         page_index = 0;
        float            memory_MB  = 0.0f;
    };

    // Nodo de empaquetado del atlas --------------------------------------------------------------
    struct PackNode {
        uint32_t x = 0, y = 0, w = 0, h = 0;
        bool     used = false;
        uint32_t left = 0xFFFFFFFFu, right = 0xFFFFFFFFu;
    };

    // Atlas de textura ---------------------------------------------------------------------------
    class TextureAtlas {
    public:
        static constexpr uint32_t k_default_page_size = 2048u;
        static constexpr uint32_t k_padding           = 2u;         // borde del pixel

        explicit TextureAtlas(uint32_t page_w = k_default_page_size, uint32_t page_h = k_default_page_size);

        // Empaqueta la región de una textura (devuelve TextureRegion para usar por la GPU)
        // 'name' es el identificador único (ruta del archivo o nombre del sprite)
        TextureRegion pack(const std::string& name, const uint8_t* rgba, uint32_t w, uint32_t h);

        // Busca por nombre (devuelve region invalida si no se encuentra)
        TextureRegion find(const std::string& name)     const;
        bool          contains(const std::string& name) const;

        // Sube paginas sucias a la GPU (llama una vez por frame tras empaquetar)
        void          flush_to_GPU();

        void          evict(const std::string& name);

        // Estados del Atlas
        float         memory_MB()   const;
        uint32_t      page_count()  const { return (uint32_t)m_pages.size(); }
        uint32_t      entry_count() const { return (uint32_t)m_entries.size(); }

        const AtlasPage& page(uint32_t i) const { return m_pages[i]; }

    private:
        uint32_t m_page_w, m_page_h;

        struct Entry {
            TextureRegion  region;
            uint32_t       page;
            bool           dirty;
        };

        std::vector<AtlasPage>                     m_pages;
        std::vector<PackNode>                      m_nodes;
        std::vector<std::vector<uint8_t>>          m_pixel_data;        // buffers de paginas de pixeles
        std::unordered_map<std::string, Entry>     m_entries;

        // Ubicador Guillotine
        bool try_pack(uint32_t pageIdx, uint32_t w, uint32_t h, uint32_t& outX, uint32_t& outY);

        void add_page();
        void split_node(uint32_t nodeIdx, uint32_t w, uint32_t h);

        // Subida a GPU simulada
        void upload_page(uint32_t page_idx);

        uint32_t m_next_handle = 1;
    };

    // Constructor en tiempo de ejecución del Atlas (para sprites dinámicos) ----------------------
    class RuntimeAtlasBuilder {
    public:
        explicit RuntimeAtlasBuilder(TextureAtlas& atlas) : m_atlas(atlas) {}

        // Planifica una textura para empaquetar
        void enqueue(const std::string& name, const uint8_t* rgba, uint32_t w, uint32_t h);

        // Procesa todos los paquetes pendients y vuelca a la GPU
        void commit();

        uint32_t pending_count() const { return (uint32_t)m_pending.size(); }

    private:
        TextureAtlas& m_atlas;

        struct PendingEntry {
            std::string          name;
            std::vector<uint8_t> pixels;

            uint32_t w, h;
        };
        std::vector<PendingEntry> m_pending;
    };

} // namespace anxiety::rendering2d
