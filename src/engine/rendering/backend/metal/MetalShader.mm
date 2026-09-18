// Objective-C++ — se compila solo en macOS/iOS con el framework Metal.
#ifdef ANXIETY_BACKEND_METAL

#import <Metal/Metal.h>

#include "MetalShader.h"

namespace anxiety::rendering::backend::metal {

    MetalShader::MetalShader(void* library, void* function, std::string entry, rhi::ShaderStage stage)
        : m_library (library)
        , m_function(function)
        , m_entry   (std::move(entry))
        , m_stage   (stage)
    {}

    MetalShader::~MetalShader() {
        if (m_function) {
            (void)(__bridge_transfer id<MTLFunction>)m_function;
            m_function = nullptr;
        }
        if (m_library) {
            (void)(__bridge_transfer id<MTLLibrary>)m_library;
            m_library = nullptr;
        }
    }

} // namespace anxiety::rendering::backend::metal

#endif // ANXIETY_BACKEND_METAL
