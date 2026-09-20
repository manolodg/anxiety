#pragma once

#include "MaterialHandle.h"
#include "rhi/IDescriptorSet.h"
#include "rhi/IPipeline.h"
#include "rhi/ShaderTypes.h"

#include <memory>
#include <string>
#include <string_view>

// Material ---------------------------------------------------------------------------------------
// Estado inmutable de pipeline + shaders de la GPU. Compartido por las MaterialInstance. Propiedad
// (como unique_ptr) de MaterialManager; no destruir directamente.
//
// Vertex layout (definido por el shader + el PipelineDesc usado al cargar):
//   unlit          - POSITION float3  COLOR float4                  (stride 28)
//   unlit_textured - POSITION float3, COLOR float4, TEXCOORD float2 (stride 36)
//
// Registros de constant buffer:
//   b0 — PerObject   (world_view_proj — se escribe por entidad y por fotograma)
//   b1 — PerMaterial (base_color + use_texture — se escribe por instancia y por fotograma)
//   t0 - (materiales con textura) Texture2D de albedo
//   s0 - (materiales con textura) sampler estático linear-wrap
// ------------------------------------------------------------------------------------------------
namespace anxiety::rendering::materials {
    class Material {
    public:
        [[nodiscard]] MaterialHandle                  handle()            const noexcept { return m_handle; }
        [[nodiscard]] std::string_view                name()              const noexcept { return m_name; }
        [[nodiscard]] rhi::IPipeline*                 pipeline()          const noexcept { return m_pipeline.get(); }
        [[nodiscard]] const rhi::DescriptorSetLayout& descriptor_layout() const noexcept { return m_ds_layout; }
        [[nodiscard]] bool                            is_valid()          const noexcept { return m_pipeline != nullptr; }

    private:
        friend class MaterialManager;

        MaterialHandle                  m_handle;
        std::string                     m_name;
        rhi::DescriptorSetLayout        m_ds_layout;
        std::unique_ptr<rhi::IShader>   m_vs;
        std::unique_ptr<rhi::IShader>   m_ps;
        std::unique_ptr<rhi::IPipeline> m_pipeline;
    };
} // namespace anxiety::rendering::materials
