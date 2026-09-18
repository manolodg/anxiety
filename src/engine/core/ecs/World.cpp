#include "World.h"

namespace anxiety::ecs {
    EntityId World::create_entity() {
        uint32_t index{};

        if (!m_free_list.empty()) {
            index = m_free_list.back();
            m_free_list.pop_back();
            m_entities[index].alive = true;
        } else {
            index = static_cast<uint32_t>(m_entities.size());
            m_entities.push_back({ .generation = 0, .alive = true });
        }

        ++m_alive_count;
        return EntityId{ index, m_entities[index].generation };
    }

    void World::destroy_entity(EntityId id) {
        if (!is_alive(id)) return;

        // Eliminar todos los componentes de esta entidad
        for (auto& [tid, storage] : m_storages) {
            storage->remove(id.index);
        }

        m_entities[id.index].alive = false;
        ++m_entities[id.index].generation;              // invalidar handles existentes
        m_free_list.push_back(id.index);
        --m_alive_count;
    }

    bool World::is_alive(EntityId id) const noexcept {
        if (!id.is_valid()) return false;
        if (id.index >= static_cast<uint32_t>(m_entities.size())) return false;
        const auto& rec = m_entities[id.index];
        return rec.alive && rec.generation == id.generation;
    }
} // namespace anxiety::ecs