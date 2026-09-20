#pragma once

#include "SceneComponents.h"
#include "SceneMath.h"
#include "materials/MaterialManager.h"
#include "rhi/IDevice.h"
#include "rhi/IDescriptorSet.h"
#include "rhi/RHITypes.h"
#include "graph/RenderGraph.h"
#include "World.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace anxiety::rendering::scene {
    // MeshHandles --------------------------------------------------------------------------------
    // Devuelto por upload_mesh(); guarda vertex_buffer / index_buffer en el componente MeshRenderer.
    struct MeshHandles {
        rhi::BufferHandle vertex_buffer;
        rhi::BufferHandle index_buffer;
    };

    // SceneRenderer ------------------------------------------------------------------------------
    // Hace de puente entre el world de ECS y el pipeline de renderizado de la RHI.
    //
    // Flujo por fotograma (llamado desde RenderingModule::on_update):
    //   1. Consulta el world por Camera+Transform      → calcula la matriz de vista-proyección.
    //   2. Consulta el world por Transform+MeshRenderer → construye una lista de dibujado.
    //   3. Para cada entidad visible: crea de forma perezosa dos constant buffers por entidad
    //      (WVP en b0, parámetros de material en b1) y los actualiza.
    //   4. Añade ClearPass + ScenePass al render graph.
    //
    // El renderizado está totalmente dirigido por materiales: los pipelines vienen de MaterialManager.
    // Cuando material_instance es inválido en un MeshRenderer se usa automáticamente el material unlit
    // por defecto.
    //
    // En esta cabecera no aparece ningún tipo específico de DX12.
    // --------------------------------------------------------------------------------------------
    class SceneRenderer {
    public:
        // materials puede ser nullptr (en ese caso se omiten las entidades con malla).
        explicit SceneRenderer(rhi::IDevice& device, materials::MaterialManager* materials = nullptr);
        ~SceneRenderer();

        // Adjunta o desadjunta un world de ECS. Pasar nullptr lo desadjunta.
        void attach_world(ecs::World* world) noexcept { m_world = world; }

        // Sube geometría a memoria de GPU. Devuelve los handles a guardar en MeshRenderer. El vertex
        // layout debe coincidir con el pipeline del material:
        //   POSITION float3 (offset 0) | COLOR float4 (offset 12) — 28 bytes/vértice.
        // Los datos de índice deben ser uint32_t.
        [[nodiscard]] MeshHandles upload_mesh( const void* vertices, size_t vb_bytes, const void* indices, size_t ib_bytes);

        void destroy_mesh(MeshHandles h);

        // Por fotograma: añade un ClearPass y (cuando hay entidades) un ScenePass al grafo.
        // aspect_ratio sobrescribe Camera::aspect_ratio cuando es > 0.
        void build_passes(graph::RenderGraph& graph, graph::RGTextureHandle bb_handle, rhi::TextureHandle bb_physical, rhi::ClearColor clear_color, float aspect_ratio = 0.f);

        [[nodiscard]] bool        is_ready()    const noexcept { return m_world != nullptr; }
        [[nodiscard]] ecs::World* world()       const noexcept { return m_world; }

    private:
        // Recursos de GPU por entidad ----------------------------------------------------------
        // Dos constant buffers de 256 bytes por entidad:
        //   wvp_buffer        → b0 (PerObject:   world_view_proj)
        //   mat_params_buffer → b1 (PerMaterial: base_color)
        struct PerEntityData {
            rhi::BufferHandle                    wvp_buffer;
            rhi::BufferHandle                    mat_params_buffer;
            std::unique_ptr<rhi::IDescriptorSet> descriptor_set;
        };

        PerEntityData& ensure_entity_data(uint32_t entity_index);

        // Miembros ------------------------------------------------------------------------------
        rhi::IDevice&               m_device;
        materials::MaterialManager* m_materials = nullptr;
        ecs::World*                 m_world     = nullptr;

        // Instancia por defecto (unlit, tinte blanco) creada de forma perezosa para las entidades sin
        // material_instance. Se crea en la primera llamada a build_passes.
        materials::MaterialInstanceHandle           m_default_instance;

        // Datos de GPU por entidad, indexados por EntityId::index.
        std::unordered_map<uint32_t, PerEntityData> m_entity_data;
    };
} // namespace anxiety::rendering::scene
