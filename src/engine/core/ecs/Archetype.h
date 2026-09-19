#pragma once

#include "Entity.h"
#include "ComponentRegistry.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace anxiety::ecs {
	// Archetype ----------------------------------------------------------------------------------
	// Almacena todas las entidades que comparten un conjunto idéntico de tipos de componente.
	// Cada tipo de componente ocupa su propio array de bytes contiguo (Structure of Arrays) para
	// una iteración favorable a la caché.
	//
	// Todos los tipos de componente deben ser trivially copyable (se comprueba en la capa de
	// plantilla de World). Los movimientos internos usan memcpy; los destructores son no-ops.
	// --------------------------------------------------------------------------------------------
	class Archetype {
	public:
		// Descripción con el tipo borrado de un tipo de componente dentro de este archetype.
		struct ComponentMeta {
			ComponentTypeId type_id;
			uint32_t        size;						// sizeof(T)
			uint32_t        align;						// alignof(T)

			void (*default_init)(void*);				// placement-new con valor por defecto en dst
		};

		Archetype() = default;
		explicit Archetype(std::vector<ComponentMeta> metas);

		// Observadores -----------------------------------------------------------------------------
		[[nodiscard]] size_t size()  const noexcept { return m_entities.size(); }
		[[nodiscard]] bool   empty() const noexcept { return m_entities.empty(); }

		[[nodiscard]] const std::vector<ComponentTypeId>& type_ids()                   const noexcept { return m_type_ids; }
		[[nodiscard]] bool                                has_type(ComponentTypeId id) const noexcept;

		// Devuelve el índice de array para 'id', o k_n_pos si no está presente.
		[[nodiscard]] size_t type_index(ComponentTypeId id) const noexcept;

		static constexpr size_t k_n_pos = ~size_t{ 0 };

		// Mutaciones estructurales -------------------------------------------------------------------
		// Añade una entidad nueva; todos los huecos de componente se inicializan con su valor por
		// defecto. Devuelve el índice de fila de la nueva entidad.
		size_t   add_entity(EntityId e);
		// Swap-remove de la entidad en 'row'. Devuelve la entidad que se movió a 'row' (nula si row
		// era la última).
		EntityId remove_entity(size_t row);

		// Acceso a datos -----------------------------------------------------------------------------
		// Puntero crudo al componente 'type_id' de la entidad en 'row'.
		[[nodiscard]] void* component_raw(ComponentTypeId type_id, size_t row)       noexcept;
		[[nodiscard]] const void* component_raw(ComponentTypeId type_id, size_t row) const noexcept;
		// Puntero al inicio de todo el array SoA de 'type_id'. Devuelve nullptr si el archetype no
		// tiene entidades o carece del tipo. Lo usa QueryView<Ts...> para construir tuplas de
		// punteros tipados para la iteración en el camino caliente.
		[[nodiscard]] void* component_array_begin(ComponentTypeId type_id) noexcept;
		// Atajo tipado usado por los helpers de plantilla de World.
		template<typename T>
		[[nodiscard]] T* get(size_t row) noexcept { return static_cast<T*>(component_raw(ComponentRegistry::type_id<T>(), row)); }
		// Copia cada componente que existe TANTO en este archetype como en 'dst', desde this[src_row]
		// hasta dst[dst_row]. Se usa durante la migración de entidades.
		void copy_shared_components(size_t src_row, Archetype& dst, size_t dst_row) const noexcept;

		[[nodiscard]] const std::vector<EntityId>& entities() const noexcept { return m_entities; }

	private:
		std::vector<ComponentTypeId>        m_type_ids;	// ordenado - índice para búsqueda binaria
		std::vector<ComponentMeta>          m_metas;	// paralelo a m_type_ids
		std::vector<std::vector<std::byte>> m_arrays;	// almacenamiento de bytes SoA, paralelo a m_type_ids
		std::vector<EntityId>				m_entities;
	};
} // namespace anxiety::ecs