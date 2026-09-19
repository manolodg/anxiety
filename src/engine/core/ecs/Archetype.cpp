#include "Archetype.h"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace anxiety::ecs {
    // --------------------------------------------------------------------------------------------
    Archetype::Archetype(std::vector<ComponentMeta> metas) : m_metas(std::move(metas)) {
        // Ordena por typeId para que la búsqueda binaria funcione correctamente.
        std::sort(m_metas.begin(), m_metas.end(), [](const ComponentMeta& a, const ComponentMeta& b) { return a.type_id < b.type_id; });

        m_type_ids.reserve(m_metas.size());
        m_arrays.resize(m_metas.size());
        for (const auto& m : m_metas) {
            m_type_ids.push_back(m.type_id);
        }
    }

    // --------------------------------------------------------------------------------------------
    bool Archetype::has_type(ComponentTypeId id) const noexcept { return std::binary_search(m_type_ids.begin(), m_type_ids.end(), id); }

    size_t Archetype::type_index(ComponentTypeId id) const noexcept {
        const auto it = std::lower_bound(m_type_ids.begin(), m_type_ids.end(), id);
        if (it == m_type_ids.end() || *it != id) return k_n_pos;
        return static_cast<size_t>(it - m_type_ids.begin());
    }

    // --------------------------------------------------------------------------------------------
    size_t Archetype::add_entity(EntityId e) {
        const size_t row = m_entities.size();
        m_entities.push_back(e);

        for (size_t i = 0; i < m_metas.size(); ++i) {
            const uint32_t stride = m_metas[i].size;
            const size_t   oldSize = m_arrays[i].size();
            m_arrays[i].resize(oldSize + stride);
            m_metas[i].default_init(m_arrays[i].data() + oldSize);
        }
        return row;
    }

    EntityId Archetype::remove_entity(size_t row) {
        assert(!m_entities.empty());
        const size_t last = m_entities.size() - 1;
        EntityId     moved = EntityId::null();

        if (row != last) {
            // Sobrescribe 'row' con los datos de 'last' (todo trivially copyable).
            moved = m_entities[last];
            for (size_t i = 0; i < m_metas.size(); ++i) {
                const uint32_t stride = m_metas[i].size;
                void* dst = m_arrays[i].data() + row * stride;
                void* src = m_arrays[i].data() + last * stride;
                std::memcpy(dst, src, stride);
            }
            m_entities[row] = moved;
        }

        // Descarta el último hueco.
        for (size_t i = 0; i < m_metas.size(); ++i) {
            m_arrays[i].resize(m_arrays[i].size() - m_metas[i].size);
        }
        m_entities.pop_back();
        return moved;
    }

    // --------------------------------------------------------------------------------------------
    void* Archetype::component_raw(ComponentTypeId typeId, size_t row) noexcept {
        const size_t idx = type_index(typeId);
        if (idx == k_n_pos) return nullptr;
        return m_arrays[idx].data() + row * m_metas[idx].size;
    }

    const void* Archetype::component_raw(ComponentTypeId typeId, size_t row) const noexcept {
        const size_t idx = type_index(typeId);
        if (idx == k_n_pos) return nullptr;
        return m_arrays[idx].data() + row * m_metas[idx].size;
    }

    void* Archetype::component_array_begin(ComponentTypeId typeId) noexcept {
        const size_t idx = type_index(typeId);
        if (idx == k_n_pos || m_arrays[idx].empty()) return nullptr;
        return m_arrays[idx].data();
    }

    // --------------------------------------------------------------------------------------------
    void Archetype::copy_shared_components(size_t srcRow,
        Archetype& dst, size_t dstRow) const noexcept {
        // Recorre ambas listas de tipos ordenadas en paralelo (merge-join) para encontrar los tipos compartidos.
        size_t si = 0, di = 0;
        while (si < m_metas.size() && di < dst.m_metas.size()) {
            const ComponentTypeId sId = m_metas[si].type_id;
            const ComponentTypeId dId = dst.m_metas[di].type_id;
            if (sId == dId) {
                const uint32_t stride = m_metas[si].size;
                const void* src = m_arrays[si].data() + srcRow * stride;
                void* out = dst.m_arrays[di].data() + dstRow * stride;
                std::memcpy(out, src, stride);
                ++si; ++di;
            }
            else if (sId < dId) {
                ++si;
            }
            else {
                ++di;
            }
        }
    }
} // namespace anxiety::ecs
