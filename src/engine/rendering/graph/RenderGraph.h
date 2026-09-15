#pragma once

#include "ResourceHandle.h"
#include "RenderPass.h"
#include "../rhi/RHI.h"

#include <cstddef>
#include <string_view>
#include <vector>

namespace anxiety::rendering::graph {
    // RenderGraph --------------------------------------------------------------------------------
    // Grafo acíclico dirigido de pases de renderizado, con alcance de un único fotograma.
    //
    // Patrón Build → Compile → Execute → Reset (una vez por fotograma):
    //
    //   graph.reset();
    //
    //   // Importa recursos externos (p. ej. el backbuffer del swapchain).
    //   auto bb = graph.import_texture("backbuffer", bbHandle, ResourceState::Present);
    //
    //   // Declara los pases en cualquier orden; compile() resuelve el orden real.
    //   graph.addPass({
    //       .name    = "ClearPass",
    //       .writes  = { bb },
    //       .execute = [&](ICommandBuffer& cmd) {
    //           cmd.resource_barrier(bbHandle, Present, RenderTarget);
    //           cmd.clear_render_target(bbHandle, { 0.1f, 0.2f, 0.4f, 1.f });
    //           cmd.resource_barrier(bbHandle, RenderTarget, Present);
    //       }
    //   });
    //
    //   graph.compile();                           // construye el DAG, orden topológico
    //
    //   cmd.begin();
    //   graph.execute(cmd);                        // invoca los pases en el orden resuelto
    //   cmd.end();
    //
    // Semántica de dependencias:
    //   Si el pase A escribe la textura T y el pase B la lee, B depende de A (A se ejecuta primero).
    //   Los pases sin restricciones de orden se ejecutan en el orden en que se registraron.
    // --------------------------------------------------------------------------------------------
    class RenderGraph {
    public:
        // Fase de construcción --------------------------------------------------------------------

        // Importa una textura gestionada externamente (p. ej. el backbuffer del swapchain). currentState es el estado de GPU del recurso al inicio del fotograma.
        RGTextureHandle import_texture(std::string_view name, rhi::TextureHandle physical_handle, rhi::ResourceState current_state = anxiety::rendering::rhi::ResourceState::Undefined);
        // Registra un pase de renderizado. Puede llamarse en cualquier orden respecto a importTexture() — compile() resuelve el orden.
        void            add_pass(RenderPassDesc desc);

        // Fase de compilación --------------------------------------------------------------------
        // Construye el DAG de dependencias a partir de las lecturas/escrituras declaradas y calcula un orden de ejecución con orden topológico. Debe llamarse antes de execute().
        void            compile();

        // Fase de ejecución ------------------------------------------------------------------------
        // Invoca el callback execute de cada pase en el orden resuelto. cmd debe estar en estado de grabación (entre begin() y end()).
        void            execute(rhi::ICommandBuffer& cmd);

        // Reinicio --------------------------------------------------------------------------------
        // Limpia todos los pases y recursos importados para reutilizarlos en el siguiente fotograma.
        void            reset();

        // Introspección ---------------------------------------------------------------------------
        [[nodiscard]] std::size_t pass_count()             const noexcept { return m_passes.size(); }
        [[nodiscard]] std::size_t imported_texture_count() const noexcept { return m_textures.size(); }

    private:
        struct TextureEntry {
            std::string        name;
            rhi::TextureHandle physical_handle;
            rhi::ResourceState current_state;
        };

        struct PassNode {
            RenderPassDesc        desc;
            std::vector<uint32_t> deps;                 // índices en m_passes (aristas de precedencia)
        };

        void topological_sort();                        // algoritmo BFS de Kahn

        std::vector<PassNode>    m_passes;
        std::vector<TextureEntry>m_textures;
        std::vector<uint32_t>    m_sorted_indices;      // orden de ejecución tras compile()

        uint32_t m_next_handle_id = 1;                  // empieza en 1 para que 0 siempre sea inválido
    };
} // namespace anxiety::rendering::graph
