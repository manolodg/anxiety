#include "HlslCompiler.h"
#include "Logger.h"

#include <cstring>

#ifdef ANXIETY_HAVE_SHADERC
#include <shaderc/shaderc.hpp>
#endif

namespace anxiety::rendering::shader {

    std::vector<uint8_t> compile_hlsl_to_spirv(const char* source, const char* entry_point, rhi::ShaderStage stage) {
#ifdef ANXIETY_HAVE_SHADERC
        if (!source || source[0] == '\0') {
            LOG_ERROR("RHI", "compile_hlsl_to_spirv: source nulo/vacío.");
            return {};
        }

        shaderc_shader_kind kind;
        switch (stage) {
        case rhi::ShaderStage::Vertex:   kind = shaderc_vertex_shader;   break;
        case rhi::ShaderStage::Fragment: kind = shaderc_fragment_shader; break;
        case rhi::ShaderStage::Compute:  kind = shaderc_compute_shader;  break;
        default: return {};
        }

        shaderc::CompileOptions options;
        options.SetSourceLanguage(shaderc_source_language_hlsl);
        options.SetAutoBindUniforms(true);
        options.SetHlslIoMapping(true);
        options.SetBindingBase(shaderc_uniform_kind_buffer,         k_cbv_binding_base);
        options.SetBindingBase(shaderc_uniform_kind_texture,        k_srv_binding_base);
        options.SetBindingBase(shaderc_uniform_kind_sampler,        k_sampler_binding_base);
        options.SetBindingBase(shaderc_uniform_kind_storage_buffer, k_uav_binding_base);
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);

        shaderc::Compiler compiler;
        shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, std::strlen(source), kind, "shader.hlsl", entry_point, options);
        if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
            LOGF_ERROR("RHI", "Error de compilación de shader: {}", result.GetErrorMessage());
            return {};
        }

        const auto* begin = reinterpret_cast<const uint8_t*>(result.cbegin());
        const auto* end   = reinterpret_cast<const uint8_t*>(result.cend());
        return { begin, end };
#else
        (void)source; (void)entry_point; (void)stage;
        LOG_ERROR("RHI", "compile_hlsl_to_spirv: compilado sin shaderc — la compilación de HLSL no está disponible.");
        return {};
#endif
    }

} // namespace anxiety::rendering::shader
