#pragma once

#include "ResourceHandle.h"
#include "RenderPass.h"
#include "../rhi/RHI.h"

#include <cstddef>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace anxiety::rendering::graph {
    // RGBarrier ----------------------------------------------------------------------------------
    // Una única transición de estado de recurso, emitida antes del pase al que pertenece. Se
    // calcula en compile(); nunca se modifica durante execute().
    // --------------------------------------------------------------------------------------------
    struct RGBarrier {
        rhi::TextureHandle handle;
        rhi::ResourceState before;
        rhi::ResourceState after;
    };

    // RenderGraph --------------------------------------------------------------------------------
    // Grafo acíclico dirigido de pases de renderizado, con ámbito de un fotograma.
    //
    // Patrón Build → Compile → Execute → Reset (una vez por fotograma):
    //
    //   graph.reset();
    //
    //   // Importa recursos externos (p. ej. el backbuffer del swapchain).
    //   // initial_state: estado de GPU al inicio del fotograma.
    //   // final_state:   estado de GPU deseado tras terminar todos los pases (p. ej. Present).
    //   auto bb = graph.import_texture("backbuffer", bbHandle, ResourceState::Present, ResourceState::Present);
    //
    //   // Declara los pases; compile() resuelve el orden e inserta barriers automáticamente.
    //   graph.addPass({
    //       .name    = "ClearPass",
    //       .writes  = {{ bb, ResourceState::RenderTarget }},
    //       .execute = [&](ICommandBuffer& cmd) {
    //           // El barrier Present-RenderTarget ya lo emitió el grafo.
    //           cmd.clear_render_target(bbHandle, { 0.1f, 0.2f, 0.4f, 1.f });
    //           // El barrier RenderTarget-Present se emite como barrier final.
    //       }
    //   });
    //
    //   graph.compile();                           // construye el DAG, orden topológico, compila barriers
    //
    //   cmd.begin();
    //   graph.execute(cmd);                        // emite barriers + invoca pases + barriers finales
    //   cmd.end();
    //
    // Reglas automáticas de barriers:
    //   Antes de cada pase el grafo emite un barrier por cada acceso de lectura/escritura cuyo estado
    //   rastreado actual difiera del estado de acceso declarado.
    //   Tras todos los pases el grafo emite barriers para devolver los recursos importados a su
    //   final_state declarado (si != Undefined y != el estado rastreado actual).
    //
    // Validación (solo avisos, no aborta la ejecución):
    //   * Lectura antes de escritura: un pase lee una textura que nunca se ha escrito (el estado
    //     actual es Undefined).
    //   * Escrituras en conflicto: dos pases escriben la misma textura sin ninguna arista de orden
    //     entre ellos — el orden de ejecución es ambiguo.
    //   * Escrituras redundantes: dos pases consecutivos escriben la misma textura en el mismo
    //     estado — la segunda escritura puede sobrescribir la primera sin que medie lectura.
    // --------------------------------------------------------------------------------------------
    class RenderGraph {
    public:
        // Fase de construcción --------------------------------------------------------------------

        // Importa una textura gestionada externamente (p. ej. el backbuffer del swapchain).
        //   initial_state - estado de GPU al inicio del fotograma.
        //   final_state   - estado de GPU deseado tras terminar todos los pases. Pasa Undefined para
        //                   omitir el barrier de estado final.
        RGTextureHandle import_texture(std::string_view name, rhi::TextureHandle physical_handle, rhi::ResourceState initial_state = rhi::ResourceState::Undefined, rhi::ResourceState final_state = rhi::ResourceState::Undefined);
        // Registra un pase de renderizado. Puede llamarse en cualquier orden respecto a importTexture() — compile() resuelve el orden.
        void            add_pass(RenderPassDesc desc);

        // Fase de compilación --------------------------------------------------------------------
        // Construye el DAG de dependencias, calcula un orden de ejecución topológicamente ordenado,
        // valida el uso de recursos, y precompila todos los barriers de recursos.
        // Debe llamarse antes de execute().
        void            compile();

        // Fase de ejecución ------------------------------------------------------------------------
        // Para cada pase en orden: emite los barriers precompilados, luego invoca el callback execute del pase.
        // Tras todos los pases, emite los barriers de estado final.
        // cmd debe estar en estado de grabación (entre begin() y end()).
        void            execute(rhi::ICommandBuffer& cmd);

        // Reinicio --------------------------------------------------------------------------------
        // Limpia todos los pases, recursos importados y estado compilado para reutilizar en el siguiente fotograma.
        void            reset();

        // Introspección ---------------------------------------------------------------------------
        [[nodiscard]] std::size_t pass_count()             const noexcept { return m_passes.size(); }
        [[nodiscard]] std::size_t imported_texture_count() const noexcept { return m_textures.size(); }

        // Accede a los barriers precompilados de un pase (índice en el orden ordenado, base 0).
        [[nodiscard]] const std::vector<RGBarrier>& pass_barriers(uint32_t sorted_idx) const noexcept;
        // Barriers de estado final emitidos tras todos los pases.
        [[nodiscard]] const std::vector<RGBarrier>& final_barriers() const noexcept;

    private:
        struct TextureEntry {
            std::string        name;
            rhi::TextureHandle physical_handle;
            rhi::ResourceState initial_state;
            rhi::ResourceState final_state;
        };

        struct PassNode {
            RenderPassDesc        desc;
            std::vector<uint32_t> deps;                 // índices en m_passes (aristas de precedencia obligatoria)
        };

        struct CompiledPass {
            uint32_t               pass_index;
            std::vector<RGBarrier> barriers;            // emitidos antes de este pase
        };

        void topological_sort();                        // algoritmo BFS de Kahn

        void validate_graph(const std::unordered_map<uint32_t, std::vector<uint32_t>>& writers);
        void compile_barriers(const std::unordered_map<uint32_t, std::vector<uint32_t>>& writers);

        [[nodiscard]] rhi::TextureHandle lookup_physical(RGTextureHandle h) const noexcept;
        [[nodiscard]] std::string_view   lookup_name(RGTextureHandle h) const noexcept;

        std::vector<PassNode>     m_passes;
        std::vector<TextureEntry> m_textures;
        std::vector<uint32_t>     m_sorted_indices;      // orden de ejecución tras compile()
        std::vector<CompiledPass> m_compiled_passes;
        std::vector<RGBarrier>    m_final_barriers;

        // Estados de recurso vivos, rastreados durante compile_barriers(); indexados por RGTextureHandle.id.
        std::unordered_map<uint32_t, rhi::ResourceState> m_tracked_states;

        uint32_t m_next_handle_id = 1;                  // empieza en 1 para que 0 sea siempre inválido
    };
} // namespace anxiety::rendering::graph
