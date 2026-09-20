#pragma once

#include "SceneComponents.h"
#include "SceneMath.h"
#include "LightData.h"
#include "materials/MaterialManager.h"
#include "rhi/IDevice.h"
#include "rhi/IDescriptorSet.h"
#include "rhi/RHITypes.h"
#include "graph/RenderGraph.h"
#include "World.h"

#include <memory>
#include <unordered_map>
#include <vector>

// Declaración adelantada — TextureManager es una dependencia opcional.
namespace anxiety::rendering::textures { class TextureManager; }

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
    //   3. Para cada entidad: crea de forma perezosa dos constant buffers por entidad (WVP en b0,
    //      parámetros de material en b1) y los actualiza. Si el material tiene un binding de textura
    //      (t0), vincula la albedo_texture de la instancia, o recurre a TextureManager::null_texture().
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
        explicit SceneRenderer(rhi::IDevice& device, materials::MaterialManager* materials = nullptr, textures::TextureManager* textures = nullptr);
        ~SceneRenderer();

        // Adjunta o desadjunta un world de ECS. Pasar nullptr lo desadjunta.
        void attach_world(ecs::World* world) noexcept { m_world = world; }

        // Sube geometría a memoria de GPU. Devuelve los handles a guardar en MeshRenderer. El vertex
        // layout debe coincidir con el pipeline del material. Strides habituales:
        //   unlit          : 28 bytes (POSITION float3, COLOR float4)
        //   unlit_textured : 36 bytes (+ TEXCOORD float2)
        // Los datos de índice deben ser uint32_t.
        [[nodiscard]] MeshHandles upload_mesh( const void* vertices, size_t vb_bytes, const void* indices, size_t ib_bytes);

        void destroy_mesh(MeshHandles h);

        // Por fotograma: añade un ClearPass y (cuando hay entidades) un ScenePass al grafo.
        // aspect_ratio sobrescribe Camera::aspect_ratio cuando es > 0.
        void build_passes(rendering::graph::RenderGraph& graph, graph::RGTextureHandle bb_handle, rhi::TextureHandle bb_physical, rhi::ClearColor clear_color, float aspect_ratio = 0.f);

        [[nodiscard]] bool        is_ready()    const noexcept { return m_world != nullptr; }
        [[nodiscard]] ecs::World* world()       const noexcept { return m_world; }

    private:
        // Recursos de GPU por entidad ----------------------------------------------------------
        // Dos constant buffers de 256 bytes por entidad:
        //   per_object_buffer → b0 (GpuPerObject: world_view_proj + world_matrix = 128 bytes)
        //   mat_params_buffer → b1 (PerMaterial : PBR params                     =  64 bytes)
        //
        // El layout del descriptor set depende del material: 2 bindings para unlit, 3 para
        // unlit_textured. last_material registra qué layout está reservado ahora mismo para recrear
        // el DS cuando cambia el material.
        struct PerEntityData {
            rhi::BufferHandle                    per_object_buffer;         // b0: GpuPerObject
            rhi::BufferHandle                    mat_params_buffer;         // b1: PerMaterial
            std::unique_ptr<rhi::IDescriptorSet> descriptor_set;
            materials::MaterialHandle            last_material;             // {} = DS aún sin construir
        };

        // Garantiza que existen los constant buffers; devuelve el slot de datos.
        PerEntityData& ensure_entity_data(uint32_t entity_index);

        // Recibe el MaterialInstance* completo para poder vincular las tres texturas (albedo,
        // normal, ORM) en el caso de los materiales PBR.
        void refresh_entity_DS(PerEntityData& data, materials::Material* mat, const materials::MaterialInstance* inst);

        // Miembros ------------------------------------------------------------------------------
        rhi::IDevice&               m_device;
        materials::MaterialManager* m_materials = nullptr;
        textures::TextureManager*   m_textures = nullptr;
        ecs::World*                 m_world     = nullptr;

        // Instancia por defecto (unlit, tinte blanco) creada de forma perezosa para las entidades sin
        // material_instance. Se crea en la primera llamada a build_passes.
        materials::MaterialInstanceHandle           m_default_instance;

        // Datos de GPU por entidad, indexados por EntityId::index.
        std::unordered_map<uint32_t, PerEntityData> m_entity_data;
        // Constant buffer compartido por fotograma para todas las luces (b2, 1024 bytes).
        // Se crea de forma perezosa en la primera llamada a build_passes cuando existen luces o
        // materiales PBR.
        rhi::BufferHandle                           m_lights_buffer;
    };
} // namespace anxiety::rendering::scene
