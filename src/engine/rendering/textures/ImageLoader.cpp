#include "ImageLoader.h"
#include "Logger.h"

// Implementación de stb_image — se compila una sola vez, en esta unidad de traducción. ----------
#ifdef ANXIETY_HAVE_STB_IMAGE
#  define STB_IMAGE_IMPLEMENTATION
#  define STBI_ONLY_PNG
#  define STBI_ONLY_JPEG
#  define STBI_NO_STDIO       // gestionamos la E/S de fichero nosotros para poder usar std::string_view
#  include "stb/stb_image.h"  // relativo a la ruta de inclusión de third_party
#  undef  STBI_NO_STDIO
#endif

#include <fstream>

namespace anxiety::rendering::textures {
    static constexpr char k_category[] = "ImageLoader";

    // load_from_memory ---------------------------------------------------------------------------
    ImageData load_from_memory(const uint8_t* data, int size_bytes) {
#ifdef ANXIETY_HAVE_STB_IMAGE
        if (!data || size_bytes <= 0) return {};

        int w = 0, h = 0, comp = 0;
        stbi_uc* raw = stbi_load_from_memory(data, size_bytes, &w, &h, &comp, 4);
        if (!raw) {
            LOGF_WARNING(k_category, "stbi_load_from_memory falló: {}", stbi_failure_reason() ? stbi_failure_reason() : "desconocido");
            return {};
        }

        ImageData img;
        img.width    = w;
        img.height   = h;
        img.channels = 4;
        img.pixels.assign(raw, raw + w * h * 4);
        stbi_image_free(raw);
        return img;
#else
        (void)data; (void)size_bytes;
        LOG_WARNING(k_category, "load_from_memory: stb_image no disponible (ANXIETY_HAVE_STB_IMAGE sin definir).");
        return {};
#endif
    }

    // load_from_file -----------------------------------------------------------------------------
    ImageData load_from_file(std::string_view path) {
#ifdef ANXIETY_HAVE_STB_IMAGE
        // Leemos el fichero a mano para poder pasar una ruta como std::string_view.
        const std::string path_str(path);
        std::ifstream f(path_str, std::ios::binary | std::ios::ate);
        if (!f.is_open()) {
            LOGF_WARNING(k_category, "load_from_file: no se puede abrir '{}'.", path_str);
            return {};
        }
        const auto file_size = static_cast<int>(f.tellg());
        f.seekg(0);
        std::vector<uint8_t> buf(static_cast<size_t>(file_size));
        if (!f.read(reinterpret_cast<char*>(buf.data()), file_size)) {
            LOGF_WARNING(k_category, "load_from_file: error de lectura en '{}'.", path_str);
            return {};
        }
        return load_from_memory(buf.data(), file_size);
#else
        (void)path;
        LOG_WARNING(k_category, "load_from_file: stb_image no disponible (ANXIETY_HAVE_STB_IMAGE sin definir).");
        return {};
#endif
    }
} // namespace anxiety::rendering::textures
