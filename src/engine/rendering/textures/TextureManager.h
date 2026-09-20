#pragma once

// TextureManager — caché de texturas de GPU ---------------------------------------------------
// Carga ficheros de imagen desde disco (vía ImageLoader/stb_image), sube los datos de píxeles a la
// GPU y mantiene una caché ruta→TextureHandle para que un mismo fichero solo se suba una vez.
//
// Siempre hay disponible una textura nula RGBA 1×1 blanca opaca vía null_texture(); úsala allí
// donde se requiera un TextureHandle válido pero no haya ninguna imagen asignada.
//
// Agnóstica de backend: toda la interacción con la GPU pasa por rhi::IDevice.
// ------------------------------------------------------------------------------------------------
#include "rhi/RHITypes.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Directorio de assets en tiempo de compilación (fijado por CMake).
#ifndef ANXIETY_ASSETS_DIR
#  define ANXIETY_ASSETS_DIR ""
#endif

namespace anxiety::rendering::rhi { class IDevice; }

namespace anxiety::rendering::textures {
    class TextureManager {
    public:
        // assets_dir — directorio base para las rutas relativas (por defecto, el define de CMake).
        explicit TextureManager(rhi::IDevice& device, std::string_view assets_dir = ANXIETY_ASSETS_DIR);
        ~TextureManager();

        // Carga desde fichero ------------------------------------------------------------------
        // Carga (o devuelve la cacheada) una textura desde un fichero PNG/JPG. path se resuelve
        // relativo a assets_dir salvo que sea absoluta. Devuelve la textura nula ante un fallo.
        [[nodiscard]] rhi::TextureHandle load(std::string_view path);

        // Carga programática ------------------------------------------------------------------
        // Sube píxeles RGBA8 en crudo como una nueva textura de GPU. No se cachea por ruta.
        // debug_name es una etiqueta opcional para las capturas de GPU.
        [[nodiscard]] rhi::TextureHandle load_from_memory(const uint8_t* rgba8, int width, int height, std::string_view debug_name = "ProgrammaticTexture");

        // Accesores ------------------------------------------------------------------------------
        // Textura 1×1 blanca opaca — fallback seguro cuando no hay ninguna textura asignada.
        [[nodiscard]] rhi::TextureHandle null_texture() const noexcept { return m_null_texture; }

        // Destruye una textura devuelta por load() / load_from_memory(). Elimina la entrada de
        // caché si existe.
        void destroy(rhi::TextureHandle handle);

    private:
        [[nodiscard]] std::string resolve_path(std::string_view path) const;

        rhi::IDevice&  m_device;
        std::string    m_assets_dir;

        rhi::TextureHandle                                  m_null_texture;
        std::unordered_map<std::string, rhi::TextureHandle> m_cache;                        // ruta → handle
        std::vector<rhi::TextureHandle>                     m_owned;                        // todos los handles en propiedad
    };
} // namespace anxiety::rendering::textures
