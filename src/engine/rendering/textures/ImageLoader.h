#pragma once

// ImageLoader — decodificador de PNG/JPG en CPU ------------------------------------------------
// load_from_file   : decodifica un PNG o JPG de disco → ImageData.
// load_from_memory : decodifica desde un buffer en memoria → ImageData.
//
// Ambas producen siempre salida RGBA8 de 4 canales (alfa = 255 para orígenes RGB). Ante cualquier
// error se devuelve un ImageData vacío (is_valid() == false).
//
// Requiere stb_image; hay que definir ANXIETY_HAVE_STB_IMAGE en tiempo de compilación. Sin él,
// load_from_file devuelve siempre datos vacíos y se registra un aviso.
// ------------------------------------------------------------------------------------------------

#include <cstdint>
#include <string_view>
#include <vector>

namespace anxiety::rendering::textures {
    // ImageData ----------------------------------------------------------------------------------
    // Contenedor POD de datos de píxeles decodificados. pixels siempre va empaquetado en RGBA8
    // (4 bytes/píxel, row-major).
    struct ImageData {
        int                  width = 0;
        int                  height = 0;
        int                  channels = 4;                      // siempre 4 (RGBA)
        std::vector<uint8_t> pixels;                            // tamaño = width * height * 4

        [[nodiscard]] bool is_valid() const noexcept { return width > 0 && height > 0 && !pixels.empty(); }
    };

    // Funciones libres -------------------------------------------------------------------------

    // Decodifica un fichero PNG/JPG en la ruta indicada. Devuelve un ImageData vacío ante un error.
    [[nodiscard]] ImageData load_from_file(std::string_view path);
    // Decodifica datos PNG/JPG que ya están en memoria. Devuelve un ImageData vacío ante un error.
    [[nodiscard]] ImageData load_from_memory(const uint8_t* data, int size_bytes);
} // namespace anxiety::rendering::textures
