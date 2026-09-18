// MetalShader.h — implementación de IShader para Metal.
// void* envuelve los objetos ObjC para mantener el header seguro en C++.
#ifdef ANXIETY_BACKEND_METAL

#pragma once

#include "../../rhi/ShaderTypes.h"
#include <string>

namespace anxiety::rendering::backend::metal {

    class MetalShader final : public rhi::IShader {
    public:
        MetalShader(void* library, void* function, std::string entry, rhi::ShaderStage stage);
        ~MetalShader() override;

        // IShader
        [[nodiscard]] std::string_view entry_point() const noexcept override { return m_entry; }
        [[nodiscard]] rhi::ShaderStage stage()       const noexcept override { return m_stage; }

        // Accesores internos
        void* mtl_library()   const noexcept { return m_library; }
        void* mtl_function()  const noexcept { return m_function; }

    private:
        void*            m_library  = nullptr;  // id<MTLLibrary>
        void*            m_function = nullptr;  // id<MTLFunction>
        std::string      m_entry;
        rhi::ShaderStage m_stage;
    };

} // namespace anxiety::rendering::backend::metal

#endif // ANXIETY_BACKEND_METAL
