#pragma once

#include "SceneComponents.h"
#include "SceneMath.h"

#include "rhi/IDevice.h"
#include "rhi/IDescriptorSet.h"
#include "rhi/IPipeline.h"
#include "rhi/RHITypes.h"
#include "graph/RenderGraph.h"
#include "World.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace anxiety::rendering::scene {
    // MeshHandles --------------------------------------------------------------------------------
    // Devuelto por upload_mesh(); guarda estos handles en el componente MeshRenderer.
    struct MeshHandles {
        rhi::BufferHandle vertex_buffer;
        rhi::BufferHandle index_buffer;
    };

    // SceneRenderer ------------------------------------------------------------------------------
    // Hace de puente entre el world de ECS y el pipeline de renderizado de la RHI.
    //
    // Flujo por fotograma (llamado desde RenderingModule::on_update):
    //   1. Consulta el world por Camera+Transform → calcula la matriz de vista-proyección.
    //   2. Consulta el world por Transform+MeshRenderer → construye una lista de dibujado.
    //   3. Para cada entidad visible: crea de forma perezosa un constant buffer por entidad y lo
    //      actualiza con la matriz WorldViewProjection de la entidad.
    //   4. Añade ClearPass + ScenePass al render graph.
    //
    // En esta cabecera no aparece ningún interno de ECS ni ningún tipo específico de DX12.
    // --------------------------------------------------------------------------------------------
    class SceneRenderer {
    public:
        explicit SceneRenderer(rhi::IDevice& device);
        ~SceneRenderer();

        // Attach or detach an ECS world.  Passing nullptr detaches.
        void attach_world(ecs::World* world) noexcept { m_world = world; }

        // Sube geometría a memoria de GPU. Devuelve los handles a guardar en MeshRenderer.
        // Los datos de vértice deben coincidir con el vertex layout del pipeline de escena:
        //   POSITION float3 (offset 0) | COLOR float4 (offset 12) — 28 bytes/vértice.
        // Los datos de índice deben ser uint32_t.
        [[nodiscard]] MeshHandles upload_mesh( const void* vertices, size_t vb_bytes, const void* indices, size_t ib_bytes);

        void destroy_mesh(MeshHandles h);

        // Por fotograma: añade un ClearPass y (cuando hay entidades) un ScenePass al grafo.
        // bbHandle   — RGTextureHandle previamente importado en el grafo.
        // bbPhysical — TextureHandle física para clear_render_target.
        // clearColor — color de fondo para el clear.
        // aspectRatio — usado para sobrescribir Camera::aspect_ratio este fotograma.
        void build_passes(graph::RenderGraph& graph, graph::RGTextureHandle bb_handle, rhi::TextureHandle bb_physical, rhi::ClearColor clear_color, float aspect_ratio = 0.f);

        [[nodiscard]] bool        is_ready()    const noexcept { return m_world != nullptr; }
        [[nodiscard]] ecs::World* world()       const noexcept { return m_world; }

    private:
        bool init_pipeline();

        // Per-entity GPU resources ---------------------------------------------------------------
        struct PerEntityData {
            rhi::BufferHandle                    constant_buffer;
            std::unique_ptr<rhi::IDescriptorSet> descriptor_set;
        };

        // Garantiza que existe la entrada de la entidad; crea de forma perezosa el CB y el descriptor set.
        PerEntityData& ensure_entity_data(uint32_t entity_index);

        // Members --------------------------------------------------------------------------------
        rhi::IDevice&                   m_device;
        ecs::World*                     m_world = nullptr;

        std::unique_ptr<rhi::IShader>   m_vs;
        std::unique_ptr<rhi::IShader>   m_ps;
        std::unique_ptr<rhi::IPipeline> m_pipeline;

        // Keyed by EntityId::index (uint32_t) — stable across entity lifecycle.
        std::unordered_map<uint32_t, PerEntityData> m_entity_data;
    };
} // namespace anxiety::rendering::scene
