#ifdef ANXIETY_BACKEND_OPENGL

#include "GLPipeline.h"

namespace anxiety::rendering::backend::opengl {

    GLPipeline::~GLPipeline() {
        if (m_vao != 0) {
            glDeleteVertexArrays(1, &m_vao);
            m_vao = 0;
        }
        if (m_program != 0) {
            glDeleteProgram(m_program);
            m_program = 0;
        }
    }

} // namespace anxiety::rendering::backend::opengl

#endif // ANXIETY_BACKEND_OPENGL
