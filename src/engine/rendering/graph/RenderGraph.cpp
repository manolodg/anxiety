#include "RenderGraph.h"
#include "Logger.h"

#include <queue>
#include <unordered_map>

namespace anxiety::rendering::graph {
    // Fase de construcción -------------------------------------------------------------------------
    RGTextureHandle RenderGraph::import_texture(std::string_view name, rhi::TextureHandle physical, rhi::ResourceState state) {
        RGTextureHandle h{ m_next_handle_id++ };
        m_textures.push_back({ std::string(name), physical, state });
        return h;
    }

    void RenderGraph::add_pass(RenderPassDesc desc) { m_passes.push_back({ std::move(desc), {} }); }

    // Fase de compilación ------------------------------------------------------------------------
    void RenderGraph::compile() {
        const auto n = static_cast<uint32_t>(m_passes.size());

        // Construye un mapa: RGTextureHandle.id → todos los índices de pase que la ESCRIBEN.
        std::unordered_map<uint32_t, std::vector<uint32_t>> writers;
        writers.reserve(n);
        for (uint32_t i = 0; i < n; ++i) {
            for (const auto& h : m_passes[i].desc.writes) {
                writers[h.id].push_back(i);
            }
        }

        // Para cada pase, por cada textura que LEE, añade una arista de orden desde cada escritor de esa textura hacia este pase (el escritor debe ejecutarse antes).
        for (uint32_t i = 0; i < n; ++i) {
            for (const auto& h : m_passes[i].desc.reads) {
                auto it = writers.find(h.id);
                if (it == writers.end()) continue;

                for (uint32_t w : it->second) {
                    if (w != i) m_passes[i].deps.push_back(w);
                }
            }
        }

        topological_sort();
    }

    void RenderGraph::topological_sort() {
        const auto n = static_cast<uint32_t>(m_passes.size());

        // BFS de Kahn: construye la lista de adyacencia + el vector de grado de entrada.
        std::vector<int>                    in_degree(n, 0);
        std::vector<std::vector<uint32_t>>  adj(n);

        for (uint32_t i = 0; i < n; ++i) {
            for (uint32_t dep : m_passes[i].deps) {
                adj[dep].push_back(i);                  // dep → i (dep debe ejecutarse antes que i)
                ++in_degree[i];
            }
        }

        std::queue<uint32_t> ready;
        for (uint32_t i = 0; i < n; ++i) {
            if (in_degree[i] == 0) ready.push(i);
        }

        m_sorted_indices.clear();
        m_sorted_indices.reserve(n);
        while (!ready.empty()) {
            const uint32_t u = ready.front(); ready.pop();
            m_sorted_indices.push_back(u);
            for (uint32_t v : adj[u]) {
                if (--in_degree[v] == 0) ready.push(v);
            }
        }

        if (m_sorted_indices.size() != n) LOG_WARNING("RenderGraph", "Ciclo detectado — uno o más pases no se ejecutarán.");
    }

    // Fase de ejecución ----------------------------------------------------------------------------
    void RenderGraph::execute(rhi::ICommandBuffer& cmd) {
        for (const uint32_t idx : m_sorted_indices) {
            const auto& pass = m_passes[idx];
            if (pass.desc.execute) pass.desc.execute(cmd);
        }
    }

    // Reinicio --------------------------------------------------------------------------------------
    void RenderGraph::reset() {
        m_passes.clear();
        m_textures.clear();
        m_sorted_indices.clear();
        m_next_handle_id = 1;
    }
} // namespace anxiety::rendering::graph
