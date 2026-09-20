#pragma once

#include "Material.h"
#include "MaterialInstance.h"
#include "rhi/IDevice.h"
#include "rhi/RHITypes.h"
#include "rhi/VertexLayout.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// MaterialManager --------------------------------------------------------------------------------
// Factoría, propietario y caché de todos los objetos Material y MaterialInstance.
//
// En la construcción se cargan dos materiales incorporados:
//   "unlit"          - vértice POSITION+COLOR    (stride 28). Solo CBs b0+b1.
//   "unlit_textured" - vértice POSITION+COLOR+UV (stride 36). CBs b0+b1 + SRV t0.
//
// Ambos se cargan desde assets/shaders/. load_material() / load_textured_material() se cachean por
// nombre, así que el mismo PSO no se crea dos veces.
//
// Uso típico:
//   MaterialManager mgr(device);
//
//   // Malla de color opaco
//   MaterialInstanceHandle red  = mgr.create_instance(mgr.default_unlit());
//   mgr.set_base_color(red, 1.0f, 0.0f, 0.0f, 1.0f);
//
//   // Malla con textura
//   MaterialInstanceHandle tex = mgr.create_instance(mgr.default_unlit_textured());
//   mgr.set_albedo_texture(tex, my_texture_handle);       // TextureHandle de TextureManager
// ------------------------------------------------------------------------------------------------

// Directorio de assets en tiempo de compilación (fijado vía CMake; puede sobrescribirse en runtime).
#ifndef ANXIETY_ASSETS_DIR
#  define ANXIETY_ASSETS_DIR ""
#endif

namespace anxiety::rendering::materials {
    class MaterialManager {
    public:
        // assets_dir toma por defecto el valor del define de compilación ANXIETY_ASSETS_DIR.
        explicit MaterialManager(rhi::IDevice& device, std::string_view assets_dir = ANXIETY_ASSETS_DIR);
        ~MaterialManager() = default;

        // Carga de materiales -----------------------------------------------------------------
        // Carga (o devuelve el cacheado) un material unlit. Vertex layout: POSITION float3 + COLOR float4 (stride 28).
        [[nodiscard]] MaterialHandle load_material(std::string_view name, std::string_view shader_path, std::string_view vs_entry = "VSMain", std::string_view ps_entry = "PSMain");
        // Carga (o devuelve el cacheado) un material con textura. Vertex layout: POSITION float3 + COLOR float4 + TEXCOORD float2 (stride 36). Incluye un sampler estático linear-wrap en s0.
        [[nodiscard]] MaterialHandle load_textured_material(std::string_view name, std::string_view shader_path, std::string_view vs_entry = "VSMain", std::string_view ps_entry = "PSMain");
        // Carga (o devuelve el cacheado) un material PBR.
        // Vertex layout: POSITION float3 + NORMAL float3 + TEXCOORD float2 + TANGENT float4 (stride 48).
        // DS layout:     b0 (PerObject 128 B) + b1 (PerMaterial 64 B) + b2 (LightsCB 576 B) + t0 (albedo) + t1 (normal) + t2 (ORM).
        // Sampler estático s0 linear-wrap.
        [[nodiscard]] MaterialHandle load_PBR_material(std::string_view name, std::string_view shader_path, std::string_view vs_entry = "VSMain", std::string_view ps_entry = "PSMain");

        // Gestión de instancias ---------------------------------------------------------------
        [[nodiscard]] MaterialInstanceHandle create_instance(MaterialHandle mat);

        // Accesores -------------------------------------------------------------------------------
        [[nodiscard]] Material*         get_material(MaterialHandle h)         const noexcept;
        [[nodiscard]] MaterialInstance* get_instance(MaterialInstanceHandle h) const noexcept;

        // Setter de conveniencia — equivalente a get_instance(h)->set_base_color(...).
        void set_base_color(MaterialInstanceHandle h, float r, float g, float b, float a) noexcept;
        void set_albedo_texture(MaterialInstanceHandle h, rhi::TextureHandle texture)     noexcept;
        void set_metallic(MaterialInstanceHandle h, float v)                              noexcept;
        void set_roughness(MaterialInstanceHandle h, float v)                             noexcept;
        void set_emissive(MaterialInstanceHandle h, float r, float g, float b)            noexcept;
        void set_normal_texture(MaterialInstanceHandle h, rhi::TextureHandle texture)     noexcept;
        void set_orm_texture(MaterialInstanceHandle h, rhi::TextureHandle texture)        noexcept;

        // Handles de los materiales incorporados (pueden ser inválidos si la carga falló).
        [[nodiscard]] MaterialHandle default_unlit()          const noexcept { return m_unlit_handle; }
        [[nodiscard]] MaterialHandle default_unlit_textured() const noexcept { return m_unlit_textured_handle; }
        [[nodiscard]] MaterialHandle default_PBR()            const noexcept { return m_pbr_handle; }

    private:
        [[nodiscard]] std::string resolve_path(std::string_view path) const;

        // Función interna que hace el trabajo: compila shaders + crea el PSO.
        [[nodiscard]] MaterialHandle load_material_internal(std::string_view name, std::string_view shader_path, std::string_view vs_entry, std::string_view ps_entry, const rhi::DescriptorSetLayout& ds_layout, const rhi::VertexLayout& vertex_layout, const std::vector<rhi::SamplerDesc>& samplers);

        rhi::IDevice&   m_device;
        std::string     m_assets_dir;
        MaterialHandle  m_unlit_handle;
        MaterialHandle  m_unlit_textured_handle;
        MaterialHandle  m_pbr_handle;

        std::vector<std::unique_ptr<Material>>          m_materials;  // indexado por id-1
        std::vector<std::unique_ptr<MaterialInstance>>  m_instances;  // indexado por id-1
        std::unordered_map<std::string, MaterialHandle> m_name_cache;
    };
} // namespace anxiety::rendering::materials
