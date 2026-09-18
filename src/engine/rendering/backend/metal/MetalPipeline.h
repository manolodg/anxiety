// MetalPipeline.h — implementación de IPipeline para Metal.
// void* envuelve los objetos ObjC para mantener el header seguro en C++.
#ifdef ANXIETY_BACKEND_METAL

#pragma once

#include "../../rhi/IPipeline.h"
#include "../../rhi/PipelineDesc.h"
#include <string>

namespace anxiety::rendering::backend::metal {

    class MetalPipeline final : public rhi::IPipeline {
    public:
        MetalPipeline(void* pipeline_state,void* depth_state, std::string name, rhi::PrimitiveTopology topology, rhi::CullMode cull_mode, rhi::FillMode fill_mode, bool front_face_ccw);
        ~MetalPipeline() override;

        // IPipeline
        [[nodiscard]] std::string_view debug_name() const noexcept override { return m_name; }

        // Accesores internos
        void*                  mtl_pipeline_state() const noexcept { return m_pipeline_state; }
        void*                  mtl_depth_state()    const noexcept { return m_depth_state; }
        rhi::PrimitiveTopology topology()           const noexcept { return m_topology; }
        rhi::CullMode          cull_mode()          const noexcept { return m_cull_mode; }
        rhi::FillMode          fill_mode()          const noexcept { return m_fill_mode; }
        bool                   front_face_ccw()     const noexcept { return m_front_face_ccw; }

    private:
        void*                  m_pipeline_state = nullptr;  // id<MTLRenderPipelineState>
        void*                  m_depth_state    = nullptr;  // id<MTLDepthStencilState>
        std::string            m_name;
        rhi::PrimitiveTopology m_topology;
        rhi::CullMode          m_cull_mode;
        rhi::FillMode          m_fill_mode;
        bool                   m_front_face_ccw = true;
    };

} // namespace anxiety::rendering::backend::metal

#endif // ANXIETY_BACKEND_METAL
