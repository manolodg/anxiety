#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace anxiety::rendering::rhi {
    // Shader stages ------------------------------------------------------------------------------
    enum class ShaderStage : uint32_t {
        Vertex,
        Fragment,
        Compute,
    };

    // ShaderDesc ---------------------------------------------------------------------------------
    // Describe un binario de shader compilado (DXIL en D3D12, SPIR-V en Vulkan). El puntero al
    // bytecode debe seguir siendo válido mientras dure la llamada a IDevice::create_shader(); no se
    // retiene después.
    struct ShaderDesc {
        const void* bytecode      = nullptr;
        size_t      bytecode_size = 0;
        const char* entry_point   = "main";
        const char* debug_name    = nullptr;
    };

    // IShader ------------------------------------------------------------------------------------
    // Objeto shader compilado, inmutable. Creado por IDevice::create_shader(). No puede compartirse
    // entre dispositivos.
    class IShader {
    public:
        virtual ~IShader() = default;

        [[nodiscard]] virtual std::string_view entry_point() const noexcept = 0;
        [[nodiscard]] virtual ShaderStage      stage()       const noexcept = 0;

    protected:
        IShader() = default;

        IShader(const IShader&)            = delete;
        IShader& operator=(const IShader&) = delete;
    };
} // namespace anxiety::rendering::rhi
