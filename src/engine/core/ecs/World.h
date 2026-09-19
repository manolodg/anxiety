#pragma once

#include "Entity.h"
#include "ComponentRegistry.h"
#include "Archetype.h"
#include "Query.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace anxiety::ecs {
    // World --------------------------------------------------------------------------------------
    // World de ECS basado en archetypes. Las entidades se agrupan por su composición exacta de
    // componentes; cada grupo (Archetype) almacena los componentes en layout SoA para una
    // iteración favorable a la caché.
    //
    // Contrato de los componentes
    // -------------------
    //   Los componentes deben ser trivially copyable (se comprueba en tiempo de compilación) y
    //   construibles por defecto. Las migraciones internas usan memcpy.
    //
    // Mutación durante la iteración
    // -------------------------
    //   NO añadas/quites componentes ni destruyas entidades mientras se está iterando un QueryView
    //   — los archetypes pueden reasignar memoria, invalidando los punteros.
    // --------------------------------------------------------------------------------------------
    class World {
    public:
        World() = default;
        ~World() = default;

        World(const World&) = delete;
        World& operator=(const World&) = delete;

        // Gestión de entidades ---------------------------------------------------------------------
        [[nodiscard]] EntityId create_entity();
        void                   destroy_entity(EntityId id);
        [[nodiscard]] bool     is_alive(EntityId id)        const noexcept;
        [[nodiscard]] uint32_t entity_count()               const noexcept { return m_alive_count; }

        // Gestión de componentes -------------------------------------------------------------------
        template<typename T>
        void add_component(EntityId id, T value = {}) {
            static_assert(std::is_trivially_copyable_v<T>, "Los componentes ECS deben ser trivially copyable");
            validate_entity(id);

            const ComponentTypeId tid = ComponentRegistry::type_id<T>();
            EntityRecord& rec = m_records[id.index];

            // Calcula la nueva clave de archetype (tipos actuales + T, ordenados).
            ArchetypeKey new_key = rec.archetype ? rec.archetype->type_ids() : ArchetypeKey{};

            {
                auto it = std::lower_bound(new_key.begin(), new_key.end(), tid);
                if (it != new_key.end() && *it == tid) return;              // ya está presente
                new_key.insert(it, tid);
            }

            ensure_meta<T>();                                               // registra el meta de T antes de buscar el archetype

            Archetype* dst = get_or_create_archetype(new_key);
            size_t     dst_row = dst->add_entity(id);

            if (rec.archetype) {
                rec.archetype->copy_shared_components(rec.row, *dst, dst_row);
                swap_remove(rec);                                           // rec.archetype / rec.row todavía son los valores viejos aquí
            }

            *dst->get<T>(dst_row) = value;

            rec.archetype = dst;
            rec.row = static_cast<uint32_t>(dst_row);
        }

        template<typename T>
        void remove_component(EntityId id) {
            static_assert(std::is_trivially_copyable_v<T>, "Los componentes ECS deben ser trivially copyable");
            if (!is_alive(id)) return;

            const ComponentTypeId tid = ComponentRegistry::type_id<T>();
            EntityRecord& rec = m_records[id.index];
            if (!rec.archetype || !rec.archetype->has_type(tid)) return;

            // Nueva clave = tipos actuales menos T.
            ArchetypeKey new_key = rec.archetype->type_ids();
            new_key.erase(std::remove(new_key.begin(), new_key.end(), tid), new_key.end());

            if (new_key.empty()) {
                // La entidad no tiene componentes restantes - se desvincula por completo.
                swap_remove(rec);
                rec.archetype = nullptr;
                rec.row = 0;

                return;
            }

            Archetype* dst = get_or_create_archetype(new_key);
            size_t     dst_row = dst->add_entity(id);
            rec.archetype->copy_shared_components(rec.row, *dst, dst_row);
            swap_remove(rec);

            rec.archetype = dst;
            rec.row = static_cast<uint32_t>(dst_row);
        }

        template<typename T>
        [[nodiscard]] T* get_component(EntityId id) noexcept {
            if (!is_alive(id)) return nullptr;

            Archetype* arch = m_records[id.index].archetype;
            if (!arch) return nullptr;

            return arch->get<T>(m_records[id.index].row);
        }

        template<typename T>
        [[nodiscard]] const T* get_component(EntityId id) const noexcept {
            if (!is_alive(id)) return nullptr;

            const Archetype* arch = m_records[id.index].archetype;
            if (!arch) return nullptr;

            return static_cast<const T*>(arch->component_raw(ComponentRegistry::type_id<T>(), m_records[id.index].row));
        }

        template<typename T>
        [[nodiscard]] bool has_component(EntityId id) const noexcept {
            if (!is_alive(id)) return false;

            const Archetype* arch = m_records[id.index].archetype;

            return arch && arch->has_type(ComponentRegistry::type_id<T>());
        }

        // Consultas --------------------------------------------------------------------------------
        // Devuelve una vista sobre todos los archetypes que contienen cada T de Ts. La vista es
        // válida hasta la siguiente mutación estructural.
        template<typename... Ts>
        [[nodiscard]] QueryView<Ts...> query() {
            std::vector<Archetype*> matching;

            const std::array<ComponentTypeId, sizeof...(Ts)> required{ ComponentRegistry::type_id<Ts>()... };

            for (auto& [key, arch] : m_archetypes) {
                bool has_all = true;
                for (ComponentTypeId rid : required) {
                    if (!arch->has_type(rid)) {
                        has_all = false;
                        break;
                    }
                }

                if (has_all) matching.push_back(arch.get());
            }

            return QueryView<Ts...> { std::move(matching) };
        }

    private:
        // Tipos internos -------------------------------------------------------------------------
        struct EntityRecord {
            uint32_t   generation = 0;
            bool       alive = false;
            Archetype* archetype = nullptr;                    // nullptr - la entidad no tiene componentes
            uint32_t   row = 0;
        };

        using ArchetypeKey = std::vector<ComponentTypeId>;      // ordenado

        struct ArchetypeKeyHash {
            size_t operator()(const ArchetypeKey& k) const noexcept {
                size_t seed = k.size();
                for (ComponentTypeId id : k) {
                    seed ^= id + 0x9e3779b9u + (seed << 6) + (seed >> 2);
                }

                return seed;
            }
        };

        // Ayudantes --------------------------------------------------------------------------------
        void validate_entity(EntityId id) const { if (!is_alive(id)) throw std::invalid_argument("EntityId is not alive"); }

        // Registra el ComponentMeta de T en el registro a nivel de World (idempotente).
        template<typename T>
        void ensure_meta() {
            const ComponentTypeId tid = ComponentRegistry::type_id<T>();

            if (m_meta_registry.count(tid)) return;
            m_meta_registry.emplace(tid, Archetype::ComponentMeta{
                .type_id = tid,
                .size = static_cast<uint32_t>(sizeof(T)),
                .align = static_cast<uint32_t>(alignof(T)),
                .default_init = [](void* dst) { new (dst) T{}; },
                });
        }

        // Encuentra o crea el archetype para 'key'. Todos los tipos en 'key' ya deben estar
        // registrados en m_meta_registry.
        Archetype* get_or_create_archetype(const ArchetypeKey& key);

        // Swap-remove de 'rec.row' en rec.archetype; actualiza el registro de la entidad movida.
        // rec.archetype y rec.row quedan apuntando al hueco viejo (ya inválido) — quien llame debe
        // actualizarlos después.
        void swap_remove(EntityRecord& rec);

        // Datos -----------------------------------------------------------------------------------
        std::vector<EntityRecord>  m_records;           // indexado por EntityId::index
        std::vector<uint32_t>      m_free_list;
        uint32_t                   m_alive_count{ 0 };

        std::unordered_map<ArchetypeKey, std::unique_ptr<Archetype>, ArchetypeKeyHash> m_archetypes;
        std::unordered_map<ComponentTypeId, Archetype::ComponentMeta>                  m_meta_registry;
    };
} // namespace anxiety::ecs
