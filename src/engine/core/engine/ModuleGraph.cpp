#include "ModuleGraph.h"

#include <format>
#include <queue>
#include <unordered_map>

namespace anxiety {
	ModuleGraph::SortResult ModuleGraph::resolve(const std::vector<std::unique_ptr<IModule>>& modules) {
		const size_t N = modules.size();
		if (N == 0) return { .success = true };

		// Paso 1: construir el mapa nombre - índice; imponer unicidad --------------------------
		std::unordered_map<std::string_view, size_t> name_to_idx;
		name_to_idx.reserve(N);

		for (size_t i = 0; i < N; ++i) {
			const std::string_view name = modules[i]->name();
			auto [it, inserted] = name_to_idx.emplace(name, i);
			if (!inserted) return { .success = false, .error = std::format("Nombre de módulo duplicado: '{}'.", name) };
		}

		// Paso 2: construir la lista de adyacencia y el array de grado de entrada --------------
		// Semántica de las aristas: dep --> dependiente (dep debe terminar antes de que empiece dependiente).
		// adj[dep_index] = lista de índices de módulos que declaran dep como dependencia.
		std::vector<std::vector<size_t>> adj(N);
		std::vector<uint32_t>            in_deg(N, 0u);

		for (size_t i = 0; i < N; ++i) {
			for (std::string_view dep_name : modules[i]->dependencies()) {
				const auto it = name_to_idx.find(dep_name);
				if (it == name_to_idx.end()) return { .success = false, .error = std::format("El módulo '{}' depende de '{}', que no está registrado.", modules[i]->name(), dep_name) };

				const size_t dep_idx = it->second;

				// Protección contra autorreferencias (A depende de A).
				if (dep_idx == i) return { .success = false, .error = std::format("El módulo '{}' declara una dependencia sobre sí mismo.", modules[i]->name()) };

				adj[dep_idx].push_back(i);
				++in_deg[i];
			}
		}

		// Paso 3: sembrar la cola de listos con todos los nodos de grado de entrada cero -------
		// Se itera en orden de registro para desempatar de forma determinista.
		std::queue<size_t> ready;
		for (size_t i = 0; i < N; ++i) {
			if (in_deg[i] == 0u) ready.push(i);
		}

		// Paso 4: BFS de Kahn - emitir los nodos en orden topológico ---------------------------
		std::vector<IModule*> sorted;
		sorted.reserve(N);

		while (!ready.empty()) {
			const size_t u = ready.front();
			ready.pop();

			sorted.push_back(modules[u].get());

			// Por cada módulo que declara a u como dependencia, reduce su grado de entrada. La lista
			// de adyacencia de u se construyó en orden de registro, así que los nodos recién listos
			// se encolan en orden de registro - preservando el desempate.
			for (const size_t v : adj[u]) {
				if (--in_deg[v] == 0u) ready.push(v);
			}
		}

		// Paso 5: detección de ciclos -----------------------------------------------------------
		// Si sorted contiene menos nodos que N, los nodos restantes (in_deg > 0) forman uno o más ciclos.
		if (sorted.size() < N) {
			std::string participants;
			for (size_t i = 0; i < N; ++i) {
				if (in_deg[i] > 0u) {
					if (!participants.empty()) participants += ", ";
					participants += '\'';
					participants += modules[i]->name();
					participants += '\'';
				}
			}

			return { .success = false, .error = std::format("Dependencia cíclica detectada. Módulo(s) implicado(s): [{}].", participants) };
		}

		return { .success = true, .order = std::move(sorted) };
	}
} // namespace anxiety::core