#include "RenderGraph.h"
#include "Logger.h"

#include <queue>
#include <unordered_map>

namespace anxiety::rendering::graph {
    static constexpr char k_category[] = "RenderGraph";

    // Ayudantes internos ---------------------------------------------------------------------------
    rhi::TextureHandle RenderGraph::lookup_physical(RGTextureHandle h) const noexcept {
        // los ids de handle son de base 1; índice = id - 1.
        const uint32_t idx = h.id - 1;
        if (idx < m_textures.size()) return m_textures[idx].physical_handle;
        return rhi::TextureHandle{};
    }

    std::string_view RenderGraph::lookup_name(RGTextureHandle h) const noexcept {
        const uint32_t idx = h.id - 1;
        if (idx < m_textures.size()) return m_textures[idx].name;
        return "<unknown>";
    }

    // Fase de construcción -------------------------------------------------------------------------
    RGTextureHandle RenderGraph::import_texture(std::string_view name, rhi::TextureHandle physical, rhi::ResourceState initial_state, rhi::ResourceState final_state) {
        RGTextureHandle h{ m_next_handle_id++ };
        m_textures.push_back({ std::string(name), physical,  initial_state, final_state });
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
            for (const auto& acc : m_passes[i].desc.writes) {
                writers[acc.handle.id].push_back(i);
            }
        }

        // Para cada pase, por cada textura que LEE, añade una arista de orden desde cada escritor de esa textura hacia este pase (el escritor debe ejecutarse antes).
        for (uint32_t i = 0; i < n; ++i) {
            for (const auto& acc : m_passes[i].desc.reads) {
                auto it = writers.find(acc.handle.id);
                if (it == writers.end()) continue;

                for (uint32_t w : it->second) {
                    if (w != i) m_passes[i].deps.push_back(w);
                }
            }
        }

        validate_graph(writers);
        topological_sort();
        compile_barriers(writers);
    }

    void RenderGraph::validate_graph(const std::unordered_map<uint32_t, std::vector<uint32_t>>& writers) {
        const auto n = static_cast<uint32_t>(m_passes.size());

        for (uint32_t i = 0; i < n; ++i) {
            const auto& desc = m_passes[i].desc;

            // Lectura antes de escritura: textura leída pero nunca escrita Y importada con estado inicial Undefined --> el contenido es indefinido.
            for (const auto& acc : desc.reads) {
                const uint32_t idx = acc.handle.id - 1;
                if (idx < m_textures.size()) {
                    const bool never_written = writers.find(acc.handle.id) == writers.end();
                    if (never_written && m_textures[idx].initial_state == rhi::ResourceState::Undefined) {
                        LOGF_WARNING(k_category, "El pase '{}' lee la textura '{}' cuyo contenido es indefinido (nunca escrita, importada con estado Undefined).", desc.name, m_textures[idx].name);
                    }
                }
            }

            // Escrituras en conflicto: varios pases escriben la misma textura sin una arista de orden directa entre ellos -> el orden de ejecución es ambiguo.
            for (const auto& acc : desc.writes) {
                auto it = writers.find(acc.handle.id);
                if (it == writers.end() || it->second.size() < 2) continue;

                // Recorre el conjunto ordenado de escritores de esta textura.
                for (uint32_t w : it->second) {
                    if (w == i) continue;

                    // Comprueba si hay una arista de dependencia entre w e i.
                    bool edge_exists = false;

                    for (uint32_t dep : m_passes[i].deps) {
                        if (dep == w) {
                            edge_exists = true;
                            break;
                        }
                    }

                    for (uint32_t dep : m_passes[w].deps) {
                        if (dep == i) {
                            edge_exists = true;
                            break;
                        }
                    }

                    if (!edge_exists) LOGF_WARNING(k_category, "Los pases '{}' y '{}' escriben la textura '{}' sin ninguna arista de orden entre ellos - el orden de ejecución es ambiguo.", desc.name, m_passes[w].desc.name, lookup_name(acc.handle));
                }
            }
        }
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

        if (m_sorted_indices.size() != n) LOG_WARNING(k_category, "Ciclo detectado — uno o más pases no se ejecutarán.");
    }

    void RenderGraph::compile_barriers(const std::unordered_map<uint32_t, std::vector<uint32_t>>& /*writers*/) {
        // Siembra los estados rastreados a partir de los estados iniciales de las texturas importadas.
        m_tracked_states.clear();
        for (uint32_t i = 0; i < static_cast<uint32_t>(m_textures.size()); ++i) {
            // handle id = i + 1
            m_tracked_states[i + 1] = m_textures[i].initial_state;
        }

        m_compiled_passes.clear();
        m_compiled_passes.reserve(m_sorted_indices.size());

        for (const uint32_t pass_idx : m_sorted_indices) {
            const auto& desc = m_passes[pass_idx].desc;
            CompiledPass cp{ pass_idx, {} };

            // Accesos de lectura ------------------------------------------------------------------
            for (const auto& acc : desc.reads) {
                if (!acc.handle.is_valid()) continue;

                auto stIt = m_tracked_states.find(acc.handle.id);
                const rhi::ResourceState current = (stIt != m_tracked_states.end()) ? stIt->second : rhi::ResourceState::Undefined;

                // Avisa si el contenido es indefinido (sin escritura previa).
                if (current == rhi::ResourceState::Undefined) {
                    LOGF_WARNING(k_category, "Pase '{}': lectura de '{}' estando en estado Undefined — el contenido puede ser basura.", desc.name, lookup_name(acc.handle));
                }

                const rhi::ResourceState needed = acc.state;
                if (needed != rhi::ResourceState::Undefined && current != needed) {
                    const rhi::TextureHandle phys = lookup_physical(acc.handle);
                    if (phys.is_valid()) cp.barriers.push_back({ phys, current, needed });
                    m_tracked_states[acc.handle.id] = needed;
                }
            }

            // Accesos de escritura -----------------------------------------------------------------
            for (const auto& acc : desc.writes) {
                if (!acc.handle.is_valid()) continue;

                auto stIt = m_tracked_states.find(acc.handle.id);
                const rhi::ResourceState current = (stIt != m_tracked_states.end()) ? stIt->second : rhi::ResourceState::Undefined;

                const rhi::ResourceState needed = acc.state;
                if (needed != rhi::ResourceState::Undefined && current != needed) {
                    const rhi::TextureHandle phys = lookup_physical(acc.handle);
                    if (phys.is_valid()) cp.barriers.push_back({ phys, current, needed });
                    m_tracked_states[acc.handle.id] = needed;
                } else if (needed != rhi::ResourceState::Undefined &&
                    current == needed) {
                    // El estado ya es correcto — no hace falta barrier, pero conviene notar que dos
                    // escrituras consecutivas en el mismo estado pueden ser un bug.
                    // (validate_graph ya avisó de escrituras concurrentes sin orden.)
                }
            }

            m_compiled_passes.push_back(std::move(cp));
        }

        // Barriers de estado final -----------------------------------------------------------------
        // Devuelve los recursos importados a su finalState declarado.
        m_final_barriers.clear();
        for (uint32_t i = 0; i < static_cast<uint32_t>(m_textures.size()); ++i) {
            const rhi::ResourceState desired = m_textures[i].final_state;
            if (desired == rhi::ResourceState::Undefined) continue;

            const uint32_t handleId = i + 1;
            auto stIt = m_tracked_states.find(handleId);
            const rhi::ResourceState current = (stIt != m_tracked_states.end()) ? stIt->second : rhi::ResourceState::Undefined;

            if (current != desired) {
                const rhi::TextureHandle phys = m_textures[i].physical_handle;
                if (phys.is_valid()) m_final_barriers.push_back({ phys, current, desired });
            }
        }
    }

    // Fase de ejecución ----------------------------------------------------------------------------
    void RenderGraph::execute(rhi::ICommandBuffer& cmd) {
        for (const auto& cp : m_compiled_passes) {
            // Emite los barriers precompilados.
            for (const auto& b : cp.barriers) {
                cmd.resource_barrier(b.handle, b.before, b.after);
            }

            // Invoca el callback execute del pase.
            const auto& pass = m_passes[cp.pass_index];
            if (pass.desc.execute) pass.desc.execute(cmd);
        }

        // Emite los barriers de estado final (p. ej. RenderTarget → Present).
        for (const auto& b : m_final_barriers) {
            cmd.resource_barrier(b.handle, b.before, b.after);
        }
    }

    // Introspection ------------------------------------------------------------------------------
    const std::vector<RGBarrier>& RenderGraph::pass_barriers(uint32_t sortedIdx) const noexcept {
        static const std::vector<RGBarrier> k_empty;
        if (sortedIdx < m_compiled_passes.size()) return m_compiled_passes[sortedIdx].barriers;

        return k_empty;
    }

    const std::vector<RGBarrier>& RenderGraph::final_barriers() const noexcept { return m_final_barriers; }

    // Reinicio --------------------------------------------------------------------------------------
    void RenderGraph::reset() {
        m_passes.clear();
        m_textures.clear();
        m_sorted_indices.clear();
        m_compiled_passes.clear();
        m_final_barriers.clear();
        m_tracked_states.clear();
        m_next_handle_id = 1;
    }
} // namespace anxiety::rendering::graph
