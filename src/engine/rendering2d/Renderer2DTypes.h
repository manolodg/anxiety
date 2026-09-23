#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace anxiety::rendering2d {
	// Primitivas matematicas 2D ------------------------------------------------------------------
	struct Vec2 {
		float x = 0.0f, y = 0.0f;

		Vec2() = default;
        constexpr Vec2(float x, float y) : x(x), y(y) {}

        Vec2  operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
        Vec2  operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
        Vec2  operator*(float s)       const { return { x * s, y * s }; }
        Vec2  operator/(float s)       const { float r = 1.f / s; return { x * r,y * r }; }
        Vec2  operator-()              const { return { -x,-y }; }
        Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
        Vec2& operator*=(float s) { x *= s; y *= s; return *this; }
        
        float dot(const Vec2& o)       const { return x * o.x + y * o.y; }
        float cross(const Vec2& o)     const { return x * o.y - y * o.x; }
        
        float length_sq()              const { return x * x + y * y; }
        float length()                 const { return std::sqrtf(length_sq()); }
        
        Vec2  normalized()             const { float l = length(); return l > 1e-6f ? (*this / l) : Vec2{}; }
        
        bool  operator==(const Vec2& o)const { return x == o.x && y == o.y; }
    };

    struct Vec4  { float x, y, z, w; };
    struct Rect  { float x, y, w, h; };                 // x/y = top-left, w/h = size
    struct Color { float r, g, b, a; static Color white() { return{ 1, 1, 1, 1 }; } static Color clear() { return{ 0, 0, 0, 0 }; } };

    // RGBA empaquetado en u32 (ABGR en memoría)
    inline uint32_t pack_color(const Color& c) {
        uint8_t r = (uint8_t)std::clamp(c.r * 255.0f, 0.0f, 255.0f);
        uint8_t g = (uint8_t)std::clamp(c.g * 255.0f, 0.0f, 255.0f);
        uint8_t b = (uint8_t)std::clamp(c.b * 255.0f, 0.0f, 255.0f);
        uint8_t a = (uint8_t)std::clamp(c.a * 255.0f, 0.0f, 255.0f);
        return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
    }

    // Manejadores opacos -------------------------------------------------------------------------
    using R2DTextureHandle  = uint32_t;
    using R2DAtlasHandle    = uint32_t;
    using R2DMaterialHandle = uint32_t;

    static constexpr uint32_t k_r_2D_invalid = 0u;

    // Capa de ordenación -------------------------------------------------------------------------
    struct SortingLayer {
        int16_t  layer = 0;                             // capa lógica (background = -100, UI = 1000)
        int16_t  order = 0;                             // orden dentro de la capa
        uint16_t flags = 0;
        bool operator<(const SortingLayer& o) const {
            if (layer != o.layer) return layer < o.layer;
            return order < o.order;
        }
    };

    // Región de textura (UV dentro del atlas) ----------------------------------------------------
    struct TextureRegion {
        R2DTextureHandle texture = k_r_2D_invalid;
        float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;   // UV normalizada
        uint16_t pixel_w = 0, pixel_h = 0;                  // dimensiones del pixel fuente
        bool     rotated = false;                           // flag de rotación del atlas
    };

    // Vertice de sprite (entrelazado, 20 bytes) --------------------------------------------------
    struct SpriteVertex {
        float    x, y;                                      // posición
        float    u, v;                                      // UV
        uint32_t color;                                     // RGBA empaquetado
    };
    static_assert(sizeof(SpriteVertex) == 20);

    // Instancia de sprite (GPU instanciada, 64 bytes) --------------------------------------------
    struct alignas(16) SpriteInstance {
        float    pos_x,   pos_y;                            // posición del mundo
        float    scale_x, scale_y;                          // escala
        float    rotation;                                  // radianes
        float    pivot_x, pivot_y;                          // pivot normalizado [0,1]
        uint32_t color;                                     // RGBA
        float    u0, v0, u1, v1;                            // rectangulo UV
        uint32_t texture_slot;                              // slot del array de textura
        float    z_order;                                   // profundida en la capa (para ordenación transparente)
        float    _pad[2];
    };
    static_assert(sizeof(SpriteInstance) == 64);

    // Clave de llamada de dibujado (para el batch) -----------------------------------------------
    struct R2DDrawKey {
        R2DTextureHandle  texture;
        R2DMaterialHandle material;
        int16_t layer, order;

        bool operator<(const R2DDrawKey& o) const {
            if (layer != o.layer)      return layer < o.layer;
            if (order != o.order)      return order < o.order;
            if (texture != o.texture)  return texture < o.texture;

            return material < o.material;
        }
    };

    // Camara 2D ----------------------------------------------------------------------------------
    struct Camera2D {
        Vec2  position = {};
        float zoom     = 1.0f;
        float rotation = 0.0f;                              // radianes
        float near_z   = -100.0f;
        float far_z    = 100.0f;
        // Viewport en pixeles de pantalla
        float vp_x = 0, vp_y = 0, vp_w = 1280, vp_h = 720;

        // Calcula la proyección ortografíca (column-major, NDC -1..+1)
        void build_ortho_matrix(float out[16])   const;
        Vec2 world_to_screen(const Vec2& world)  const;
        Vec2 screen_to_world(const Vec2& screen) const;
    };

    // Estados del render 2D ----------------------------------------------------------------------
    struct R2DStats {
        uint32_t sprite_count    = 0;
        uint32_t batch_count     = 0;
        uint32_t draw_call_count = 0;
        uint32_t texture_slots   = 0;
        uint32_t particle_count  = 0;
        uint32_t tile_count      = 0;
        float    atlas_memory_MB = 0.0f;
        float    render_ms       = 0.0f;
    };

    // Modos de mezcla ----------------------------------------------------------------------------
    enum class BlendMode : uint8_t {
        Alpha,                                              // standard alpha blending
        Additive,                                           // fire/glow
        Multiply,                                           // shadow/darkening
        Premul,                                             // pre-multiplied alpha
        Opaque                                              // no blending
    };

    // Capa de escalabilidad ----------------------------------------------------------------------
    enum class R2DTier : uint8_t {
        RPi     = 0,                                        // GLES 2.0 / no instancing
        Mobile  = 1,                                        // GLES 3.0 / limited instancing
        Desktop = 2,                                        // full GPU instancing
    };
} // namespace anxiety::rendering2d