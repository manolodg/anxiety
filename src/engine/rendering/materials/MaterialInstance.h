#pragma once

#include "MaterialHandle.h"
#include "rhi/RHITypes.h"

// MaterialParams — parámetros en CPU de una instancia de material -------------------------------
// Se suben al constant buffer PerMaterial (b1) antes de cada dibujado.
//
// Distribución (32 bytes, cabe en un CB de GPU de 256 bytes):
//   float4 base_color  — tinte RGBA multiplicado con el color de vértice y la textura.
//   int    use_texture — distinto de cero → muestrea t0; cero → usa blanco (1,1,1,1).
//   float3 _pad        — relleno explícito para que el tamaño del struct quede bien definido.
//
// El shader unlit solo lee base_color (los primeros 16 bytes); los campos extra son inocuos porque
// el CB es mayor que lo que declara el HLSL.
// ------------------------------------------------------------------------------------------------
namespace anxiety::rendering::materials {
    struct MaterialParams {
        float base_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };           //        16 bytes
        int   use_texture   = 0;                                    //         4 bytes
        float _pad[3]       = {};                                   //        12 bytes
    };                                                              // Total: 32 bytes
    static_assert(sizeof(MaterialParams) == 32);

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
        [[nodiscard]] bool                  is_dirty()       const noexcept { return m_dirty; }

        void clear_dirty() noexcept { m_dirty = false; }
        
        void set_albedo_texture(rhi::TextureHandle h) noexcept {
            m_albedo_texture = h;
            m_params.use_texture = h.is_valid() ? 1 : 0;
            m_dirty = true;
        }

        void set_base_color(float r, float g, float b, float a) noexcept {
            m_params.base_color[0] = r;
            m_params.base_color[1] = g;
            m_params.base_color[2] = b;
            m_params.base_color[3] = a;
            m_dirty = true;
        }

    private:
        friend class MaterialManager;

        MaterialHandle     m_material;
        MaterialParams     m_params;
        rhi::TextureHandle m_albedo_texture;
        bool               m_dirty = true;              // true al crearse → el primer fotograma siempre escribe
    };
} // namespace anxiety::rendering::materials
