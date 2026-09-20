#pragma once

// ShaderLoader — utilidad header-only de carga desde disco + compilación ------------------------
// read_text_file    : lee un fichero de texto UTF-8 desde disco → std::string.
// compile_from_file : lee + IDevice::compile_shader_from_source en una sola llamada.
//
// Ambas devuelven un resultado vacío ante un fallo (fichero inexistente, error de compilación). No
// lanzan excepciones.
// ------------------------------------------------------------------------------------------------
#include "rhi/IDevice.h"

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace anxiety::rendering::materials {
    [[nodiscard]] inline std::string read_text_file(std::string_view path) {
        const std::string path_str(path);
        std::ifstream f(path_str);
        if (!f.is_open()) return {};
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    [[nodiscard]] inline std::vector<uint8_t> compile_from_file(rhi::IDevice& device, std::string_view path, std::string_view entry_point, rhi::ShaderStage stage) {
        const std::string src = read_text_file(path);
        if (src.empty()) return {};
        return device.compile_shader_from_source(src.c_str(), std::string(entry_point).c_str(), stage);
    }
} // namespace anxiety::rendering::materials
