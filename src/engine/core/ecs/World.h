#pragma once

#include "Entity.h"
#include "ComponentRegistry.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <cassert>
#include <stdexcept>

namespace anxiety::ecs {
	// IComponentStorage - base con borrado de tipo para los pools de componentes -------------------
	struct IComponentStorage {
		virtual ~IComponentStorage() = default;
		virtual void remove(uint32_t entity_index) = 0;
	};

	// ComponentStorage<T> ------------------------------------------------------------------------
	//	Almacenamiento sparse-set: un array de índices (entity_index -> dense_index) y un array
	//	denso y compacto con los valores de los componentes.
	// --------------------------------------------------------------------------------------------
	template<typename T>
	class ComponentStorage final : public IComponentStorage {
	public:
		void add(uint32_t entity_index, T component) {
			if (entity_index >= m_sparse.size()) m_sparse.resize(entity_index + 1, k_none);

			assert(m_sparse[entity_index] == k_none && "Component already exists for entity");

			m_sparse[entity_index] = static_cast<uint32_t>(m_dense.size());
			m_dense_entities.push_back(entity_index);
			m_dense.push_back(std::move(component));
		}

		void remove(uint32_t entity_index) override {
			if (entity_index >= m_sparse.size()) return;
			if (m_sparse[entity_index] == k_none) return;

			const uint32_t dense_idx  = m_sparse[entity_index];
			const uint32_t last_dense = static_cast<uint32_t>(m_dense.size()) - 1;

			if (dense_idx != last_dense) {
				// Intercambiar con el último elemento para mantener el array compacto
				m_dense[dense_idx]                    = std::move(m_dense[last_dense]);
				m_dense_entities[dense_idx]           = m_dense_entities[last_dense];
				m_sparse[m_dense_entities[dense_idx]] = dense_idx;
			}

			m_dense.pop_back();
			m_dense_entities.pop_back();
			m_sparse[entity_index] = k_none;
		}

		[[nodiscard]] T* get(uint32_t entity_index) noexcept {
			if (entity_index >= m_sparse.size()) return nullptr;
			if (m_sparse[entity_index] == k_none) return nullptr;

			return &m_dense[m_sparse[entity_index]];
		}

		[[nodiscard]] const T* get(uint32_t entity_index) const noexcept {
			if (entity_index >= m_sparse.size()) return nullptr;
			if (m_sparse[entity_index] == k_none) return nullptr;

			return &m_dense[m_sparse[entity_index]];
		}

		[[nodiscard]] bool has(uint32_t entity_index) const noexcept { return (entity_index < m_sparse.size()) && (m_sparse[entity_index] != k_none); }

		[[nodiscard]] std::vector<T>&       components()        noexcept { return m_dense; }
		[[nodiscard]] const std::vector<T>& components() const  noexcept { return m_dense; }

	private:
		static constexpr uint32_t k_none = std::numeric_limits<uint32_t>::max();

		std::vector<uint32_t> m_sparse;					// entity_index -> dense_index
		std::vector<uint32_t> m_dense_entities;			// dense_index  -> entity_index
		std::vector<T>        m_dense;
	};

	// World --------------------------------------------------------------------------------------
    class World {
    public:
        World()  = default;
        ~World() = default;

        World(const World&)            = delete;
        World& operator=(const World&) = delete;

        // Gestión de entidades -------------------------------------------------------------------
        EntityId           create_entity();
        void               destroy_entity(EntityId id);
        [[nodiscard]] bool is_alive(EntityId id)       const noexcept;

        // Gestión de componentes ------------------------------------------------------------------
        template<typename T>
        void add_component(EntityId id, T component) {
            validate_entity(id);
            auto& storage = get_or_create_storage<T>();
            storage.add(id.index, std::move(component));
        }

        template<typename T>
        void remove_component(EntityId id) {
            validate_entity(id);
            if (auto* s = find_storage<T>()) s->remove(id.index);
        }

        template<typename T>
        [[nodiscard]] T* get_component(EntityId id) noexcept {
            if (!is_alive(id)) return nullptr;
            auto* s = find_storage<T>();
            return s ? s->get(id.index) : nullptr;
        }

        template<typename T>
        [[nodiscard]] const T* get_component(EntityId id) const noexcept {
            if (!is_alive(id)) return nullptr;
            const auto* s = find_storage<T>();
            return s ? s->get(id.index) : nullptr;
        }

        template<typename T>
        [[nodiscard]] bool has_component(EntityId id) const noexcept {
            if (!is_alive(id)) return false;
            const auto* s = find_storage<T>();
            return s && s->has(id.index);
        }

        // Iteración --------------------------------------------------------------------------------
        template<typename T, typename Fn>
        void each(Fn&& fn) {
            if (auto* s = find_storage<T>()) {
                for (auto& comp : s->components()) {
                    fn(comp);
                }
            }
        }

        [[nodiscard]] uint32_t entity_count() const noexcept { return m_alive_count; }

    private:
        // Pool de entidades
        struct EntityRecord {
            uint32_t generation = 0;
            bool     alive      = false;
        };

        std::vector<EntityRecord>                                               m_entities;
        std::vector<uint32_t>                                                   m_free_list;
        uint32_t                                                                m_alive_count{ 0 };
        std::unordered_map<ComponentTypeId, std::unique_ptr<IComponentStorage>> m_storages;

        void validate_entity(EntityId id) const { if (!is_alive(id)) throw std::invalid_argument("EntityId is not alive"); }

        template<typename T>
        ComponentStorage<T>& get_or_create_storage() {
            const ComponentTypeId tid = ComponentRegistry::type_id<T>();
            auto it = m_storages.find(tid);
            if (it == m_storages.end()) {
                auto ptr = std::make_unique<ComponentStorage<T>>();
                auto* raw = ptr.get();
                m_storages.emplace(tid, std::move(ptr));
                return *raw;
            }
            return *static_cast<ComponentStorage<T>*>(it->second.get());
        }

        template<typename T>
        ComponentStorage<T>* find_storage() noexcept {
            const ComponentTypeId tid = ComponentRegistry::type_id<T>();
            auto it = m_storages.find(tid);
            return (it != m_storages.end())
                ? static_cast<ComponentStorage<T>*>(it->second.get())
                : nullptr;
        }

        template<typename T>
        const ComponentStorage<T>* find_storage() const noexcept {
            const ComponentTypeId tid = ComponentRegistry::type_id<T>();
            auto it = m_storages.find(tid);
            return (it != m_storages.end()) ? static_cast<const ComponentStorage<T>*>(it->second.get()) : nullptr;
        }
    };
} // namespace anxiety::ecs