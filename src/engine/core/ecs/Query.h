#pragma once

#include "Archetype.h"
#include "ComponentRegistry.h"
#include "JobSystem.h"

#include <tuple>
#include <vector>

namespace anxiety::ecs {
	// QueryView<Ts...> -----------------------------------------------------------------------------
	// Una vista evaluada de forma perezosa sobre cada Archetype que contiene TODOS los Ts. La
	// devuelve World::query<Ts...>(); itérala de inmediato o guárdala para reutilizarla (pero NO la
	// conserves a través de mutaciones estructurales — los archetypes pueden reasignar memoria).
	//
	// Garantías del camino caliente
	// --------------------
	//   • Sin llamadas virtuales en forEach / forEachParallel.
	//   • Sin asignaciones de memoria en el heap por entidad.
	//   • makePointers() construye una tupla de punteros T* crudos una vez por archetype, y luego el
	//     bucle interno es un simple std::apply indexado por puntero.
	// --------------------------------------------------------------------------------------------
	template<typename... Ts>
	class QueryView {
	public:
		explicit QueryView(std::vector<Archetype*> archetypes) : m_archetypes(std::move(archetypes)) {}

		// Iteración en un solo hilo --------------------------------------------------------------
		// Llama a fn(TS&...) una vez por cada entidad en los archetypes que coinciden.
		template<typename Fn>
		void for_each(Fn&& fn) const {
			for (Archetype* arch : m_archetypes) {
				const size_t n = arch->size();
				if (n == 0) continue;

				const auto ptrs = make_pointers(arch);
				for (size_t i = 0; i < n; ++i) {
					std::apply([&](auto*... ps) { fn(ps[i]...); }, ptrs);
				}
			}
		}
		// Llama a fn(EntityId, Ts&...) - igual que for_each pero también pasa la entidad.
		template<typename Fn>
		void for_each_with_entity(Fn&& fn) const {
			for (Archetype* arch : m_archetypes) {
				const size_t n = arch->size();
				if (n == 0) continue;

				const auto& ents = arch->entities();
				const auto  ptrs = make_pointers(arch);

				for (size_t i = 0; i < n; ++i) {
					std::apply([&](auto*... ps) { fn(ents[i], ps[i]...); }, ptrs);
				}
			}
		}

		// Iteración en paralelo --------------------------------------------------------------------
		// Divide cada archetype que coincide en fragmentos de 'chunkSize' entidades y envía un
		// trabajo por fragmento a 'js'. Bloquea hasta que todos los fragmentos terminan.
		//
		// fn(Ts&...) se llama desde varios hilos a la vez; cada invocación es dueña de un rango de
		// memoria disjunto — no hace falta ninguna sincronización dentro de fn.
		static constexpr size_t k_default_chunk = 256;

		template<typename Fn>
		void for_each_parallel(jobs::JobSystem& js, Fn fn, size_t chunk_size = k_default_chunk) const {
			std::vector<jobs::JobHandle> handles;

			for (Archetype* arch : m_archetypes) {
				const size_t n = arch->size();
				if (n == 0) continue;

				const auto ptrs = make_pointers(arch);

				for (size_t offset = 0; offset < n; offset += chunk_size) {
					const size_t end = std::min(offset + chunk_size, n);
					handles.push_back(js.submit([ptrs, offset, end, fn]() {
						for (size_t i = offset; i < end; ++i) {
							std::apply([&](auto*... ps) { fn(ps[i]...); }, ptrs);
						}
						}));
				}
			}

			for (const auto& h : handles) {
				js.wait(h);
			}
		}

		// Meta -----------------------------------------------------------------------------------
		[[nodiscard]] size_t matching_archetypes() const noexcept { return m_archetypes.size(); }
		[[nodiscard]] size_t entity_count()        const noexcept {
			size_t total = 0;
			for (const Archetype* a : m_archetypes) {
				total += a->size();
			}

			return total;
		}

	private:
		std::vector<Archetype*> m_archetypes;

		// Construye una tupla (T0*, T1*, ...) que apunta al primer elemento del array SoA de cada
		// componente dentro de 'arch'. Se expande en tiempo de compilación.
		static std::tuple<Ts*...> make_pointers(Archetype* arch) noexcept {
			return { static_cast<Ts*>(arch->component_array_begin(ComponentRegistry::type_id<Ts>()))... };
		}
	};
} // namespace anxiety::ecs