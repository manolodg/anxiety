#pragma once

#include "MaterialHandle.h"
#include "rhi/RHITypes.h"

// MaterialParams — parámetros en CPU de una instancia de material -------------------------------
// Se suben al constant buffer PerMaterial (b1) antes de cada dibujado.
//
// Layout (64 bytes, cabe en un CB de GPU de 256 bytes).
// Los nombres y offsets de los campos deben coincidir con el cbuffer PerMaterial de pbr.hlsl:
//
//   float4 baseColor     offset  0  (16 bytes) — tinte RGBA de albedo
//   int    useTexture    offset 16  ( 4 bytes) — distinto de cero → muestrea t0 (con alias useAlbedoTex en PBR)
//   float  metallic      offset 20  ( 4 bytes) — 0 = dieléctrico, 1 = metal
//   float  roughness     offset 24  ( 4 bytes) — 0 = espejo, 1 = totalmente rugoso
//   float  _pad0         offset 28  ( 4 bytes)
//   float3 emissive      offset 32  (12 bytes) — radiancia emisiva (RGB lineal)
//   int    useNormalTex  offset 44  ( 4 bytes) — distinto de cero → muestrea t1 (mapa de normales)
//   int    useOrmTex     offset 48  ( 4 bytes) — distinto de cero → muestrea t2 (mapa ORM)
//   float3 _pad1         offset 52  (12 bytes)
//
// Los shaders unlit solo leen los primeros 32 bytes; los campos PBR adicionales son inofensivos (el
// CB de GPU es más grande que lo que declara el HLSL unlit).
// ------------------------------------------------------------------------------------------------
namespace anxiety::rendering::materials {
    struct MaterialParams {
        float base_color[4]  = { 1.0f, 1.0f, 1.0f, 1.0f };          //        16 bytes
        int   use_texture    = 0;                                   //         4 bytes
        float metallic       = 0.0f;                                // offset 20:        4 bytes
        float roughness      = 0.5f;                                // offset 24:        4 bytes
        float _pad0          = 0.0f;                                // offset 28:        4 bytes
        float emissive[3]    = { 0.0f, 0.0f, 0.0f };                // offset 32:       12 bytes
        int   use_normal_tex = 0;                                   // offset 44:        4 bytes
        int   use_orm_tex    = 0;                                   // offset 48:        4 bytes
        float _pat1[3]       = {};                                  // offset 52:       12 bytes
    };                                                              // Total: 32 bytes
    static_assert(sizeof(MaterialParams) == 64);

    // MaterialInstance — overrides de material por objeto ---------------------------------------
    // Objeto ligero en CPU, propiedad de MaterialManager. La subida a GPU se difiere a SceneRenderer,
    // que es dueño del CB por entidad. Se referencia mediante un MaterialInstanceHandle guardado en
    // el componente MeshRenderer.
    // --------------------------------------------------------------------------------------------
    class MaterialInstance {
    public:
        [[nodiscard]] MaterialHandle        material()       const noexcept { return m_material; }
        [[nodiscard]] const MaterialParams& params()         const noexcept { return m_params; }
        [[nodiscard]] rhi::TextureHandle    albedo_texture() const noexcept { return m_albedo_texture; }
        [[nodiscard]] rhi::TextureHandle    normal_texture() const noexcept { return m_normal_texture; }
        [[nodiscard]] rhi::TextureHandle    orm_texture()    const noexcept { return m_orm_texture; }
        [[nodiscard]] bool                  is_dirty()       const noexcept { return m_dirty; }

        void clear_dirty() noexcept { m_dirty = false; }
        
        void set_base_color(float r, float g, float b, float a) noexcept {
            m_params.base_color[0] = r;
            m_params.base_color[1] = g;
            m_params.base_color[2] = b;
            m_params.base_color[3] = a;
            m_dirty = true;
        }

        void set_albedo_texture(rhi::TextureHandle h) noexcept {
            m_albedo_texture = h;
            m_params.use_texture = h.is_valid() ? 1 : 0;
            m_dirty = true;
        }

        void set_metallic(float v)  noexcept { m_params.metallic = v; m_dirty = true; }
        void set_roughness(float v) noexcept { m_params.roughness = v; m_dirty = true; }

        void set_emissive(float r, float g, float b) noexcept {
            m_params.emissive[0] = r;
            m_params.emissive[1] = g;
            m_params.emissive[2] = b;
            m_dirty = true;
        }

        void set_normal_texture(rhi::TextureHandle h) noexcept {
            m_normal_texture = h;
            m_params.use_normal_tex = h.is_valid() ? 1 : 0;
            m_dirty = true;
        }

        void set_orm_texture(rhi::TextureHandle h) noexcept {
            m_orm_texture = h;
            m_params.use_orm_tex = h.is_valid() ? 1 : 0;
            m_dirty = true;
        }

    private:
        friend class MaterialManager;

        MaterialHandle     m_material;
        MaterialParams     m_params;
        rhi::TextureHandle m_albedo_texture;
        rhi::TextureHandle m_normal_texture;
        rhi::TextureHandle m_orm_texture;
        bool               m_dirty = true;              // true al crearse → el primer fotograma siempre escribe
    };
} // namespace anxiety::rendering::materials
