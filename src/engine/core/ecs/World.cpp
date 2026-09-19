#include "World.h"

namespace anxiety::ecs {
    EntityId World::create_entity() {
        uint32_t index{};

        if (!m_free_list.empty()) {
            index = m_free_list.back();
            m_free_list.pop_back();

            m_records[index].alive = true;
            m_records[index].archetype = nullptr;
            m_records[index].row = 0;
            // la generación ya se incrementó durante destroy_entity
        } else {
            index = static_cast<uint32_t>(m_records.size());
            m_records.push_back({ .generation = 1, .alive = true });
        }

        ++m_alive_count;
        return EntityId{ index, m_records[index].generation };
    }

    void World::destroy_entity(EntityId id) {
        if (!is_alive(id)) return;

        EntityRecord& rec = m_records[id.index];
        if (rec.archetype) {
            swap_remove(rec);
            rec.archetype = nullptr;
        }

        rec.alive = false;
        ++rec.generation;
        if (rec.generation == 0) rec.generation = 1;  // se salta el 0 (centinela nulo)
        m_free_list.push_back(id.index);
        --m_alive_count;
    }

    bool World::is_alive(EntityId id) const noexcept {
        if (!id.is_valid()) return false;
        if (id.index >= static_cast<uint32_t>(m_records.size())) return false;
        const auto& rec = m_records[id.index];
        return rec.alive && rec.generation == id.generation;
    }

    // --------------------------------------------------------------------------------------------
    Archetype* World::get_or_create_archetype(const ArchetypeKey& key) {
        auto it = m_archetypes.find(key);
        if (it != m_archetypes.end()) return it->second.get();

        std::vector<Archetype::ComponentMeta> metas;
        metas.reserve(key.size());
        for (ComponentTypeId tid : key) {
            auto mit = m_meta_registry.find(tid);
            assert(mit != m_meta_registry.end() && "Tipo ausente del registro de metas — hay que llamar antes a ensureMeta<T>()");

            metas.push_back(mit->second);
        }

        auto  arch = std::make_unique<Archetype>(std::move(metas));
        auto* ptr = arch.get();
        m_archetypes.emplace(key, std::move(arch));
        return ptr;
    }

    void World::swap_remove(EntityRecord& rec) {
        assert(rec.archetype);
        const EntityId moved = rec.archetype->remove_entity(rec.row);
        if (moved.is_valid()) {
            // La entidad que estaba en 'last' se ha movido a rec.row.
            m_records[moved.index].row = rec.row;
        }
    }
} // namespace anxiety::ecs
