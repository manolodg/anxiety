#pragma once

#ifdef ANXIETY_BACKEND_OPENGL

#include "GLHelpers.h"
#include "../../rhi/ShaderTypes.h"

#include <string>

namespace anxiety::rendering::backend::opengl {

    // GLShader — envuelve un único objeto de shader de GL compilado (vértice, fragmento o compute).
    //
    // El objeto de shader lo compila IDevice::create_shader(). Varios GLShader se enlazan
    // en un programa de GL mediante GLDevice::create_pipeline().
    class GLShader final : public rhi::IShader {
    public:
        GLShader(GLuint shader, std::string entry_point, rhi::ShaderStage stage) noexcept : m_shader(shader), m_entry(std::move(entry_point)), m_stage(stage) {}

        ~GLShader() override {
            if (m_shader != 0)
                glDeleteShader(m_shader);
        }

        // IShader
        [[nodiscard]] std::string_view entry_point() const noexcept override { return m_entry; }
        [[nodiscard]] rhi::ShaderStage stage()       const noexcept override { return m_stage; }

        // Objeto de shader de GL puro — usado por GLDevice::create_pipeline().
        [[nodiscard]] GLuint gl_name() const noexcept { return m_shader; }

    private:
        GLuint           m_shader = 0;
        std::string      m_entry;
        rhi::ShaderStage m_stage;
    };

} // namespace anxiety::rendering::backend::opengl

#endif // ANXIETY_BACKEND_OPENGL
