#pragma once

#include "MaterialHandle.h"

// MaterialParams — parámetros en CPU de una instancia de material -------------------------------
// Se suben al constant buffer PerMaterial (b1 en unlit.hlsl) antes de cada dibujado. Se mantiene en
// 16 bytes para que quepa holgadamente dentro de un CB de GPU de 256 bytes.
// ------------------------------------------------------------------------------------------------
namespace anxiety::rendering::materials {
    struct MaterialParams {
        float base_color[4] = { 1.f, 1.f, 1.f, 1.f };           // tinte RGBA (por defecto: blanco)
    };
    static_assert(sizeof(MaterialParams) <= 256);

    // MaterialInstance — overrides de material por objeto ---------------------------------------
    // Objeto ligero en CPU, propiedad de MaterialManager. La subida a GPU se difiere a SceneRenderer,
    // que es dueño del CB por entidad. Se referencia mediante un MaterialInstanceHandle guardado en
    // el componente MeshRenderer.
    // --------------------------------------------------------------------------------------------
    class MaterialInstance {
    public:
        [[nodiscard]] MaterialHandle        material() const noexcept { return m_material; }
        [[nodiscard]] const MaterialParams& params()   const noexcept { return m_params; }
        [[nodiscard]] bool                  is_dirty() const noexcept { return m_dirty; }

        void clear_dirty() noexcept { m_dirty = false; }

        void set_base_color(float r, float g, float b, float a) noexcept {
            m_params.base_color[0] = r;
            m_params.base_color[1] = g;
            m_params.base_color[2] = b;
            m_params.base_color[3] = a;
            m_dirty = true;
        }

    private:
        friend class MaterialManager;

        MaterialHandle m_material;
        MaterialParams m_params;
        bool           m_dirty = true;              // true al crearse → el primer fotograma siempre escribe
    };
} // namespace anxiety::rendering::materials
