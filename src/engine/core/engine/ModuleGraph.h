#pragma once

#include "IModule.h"

#include <memory>
#include <string>
#include <vector>

namespace anxiety {
    // ModuleGraph --------------------------------------------------------------------------------
    // Construye un grafo dirigido acíclico a partir de las dependencias declaradas por un conjunto
    // de módulos, lo valida y devuelve un orden de inicialización ordenado topológicamente.
    //
    // Algoritmo: ordenación topológica mediante BFS de Kahn.
    //   - Tiempo O(V + E), espacio O(V + E).
    //   - La detección de ciclos es implícita: si la salida es más corta que la entrada, los nodos
    //     restantes (con grado de entrada positivo) participan en un ciclo.
    //   - El desempate entre módulos independientes sigue el orden de registro, preservando un
    //     comportamiento predecible cuando no existen dependencias.
    //
    // Esta clase no guarda estado - resolve() es una función pura.
    // --------------------------------------------------------------------------------------------
    class ModuleGraph {
    public:
        ModuleGraph() = delete;

        // Resultado de un intento de resolución de dependencias.
        struct SortResult {
            bool                  success{ false };
            std::string           error;					// no vacío solo cuando success == false.
            std::vector<IModule*> order;					// válido y no vacío solo cuando success == true.
        };

        // Resuelve y ordena topológicamente 'modules'.
        //
        // Condiciones de fallo (success == false):
        //   1. Dos módulos comparten el mismo nombre.
        //   2. Un módulo declara una dependencia cuyo nombre no está registrado.
        //   3. Existe un ciclo en el grafo de dependencias.
        //
        // En caso de éxito, result.order contiene punteros en bruto (no propietarios) hacia
        // 'modules' en la secuencia de inicialización correcta.
        [[nodiscard]] static SortResult resolve(const std::vector<std::unique_ptr<IModule>>& modules);
    };
} // namespace anxiety::core