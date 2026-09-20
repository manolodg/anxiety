// Objective-C++ — se compila solo en macOS/iOS con el framework Metal.
#ifdef ANXIETY_BACKEND_METAL

#import <Metal/Metal.h>

#include "MetalPipeline.h"

namespace anxiety::rendering::backend::metal {

    MetalPipeline::MetalPipeline(void* pipeline_state, void* depth_state, std::string name, rhi::PrimitiveTopology topology, rhi::CullMode cull_mode, rhi::FillMode fill_mode, bool front_face_ccw)
        : m_pipeline_state (pipeline_state)
        , m_depth_state    (depth_state)
        , m_name           (std::move(name))
        , m_topology       (topology)
        , m_cull_mode      (cull_mode)
        , m_fill_mode      (fill_mode)
        , m_front_face_ccw (front_face_ccw)
    {}

    MetalPipeline::~MetalPipeline() {
        for (auto& s : m_samplers) {
            if (s.second) (void)(__bridge_transfer id<MTLSamplerState>)s.second;
        }
        m_samplers.clear();
        if (m_depth_state) {
            (void)(__bridge_transfer id<MTLDepthStencilState>)m_depth_state;
            m_depth_state = nullptr;
        }
        if (m_pipeline_state) {
            (void)(__bridge_transfer id<MTLRenderPipelineState>)m_pipeline_state;
            m_pipeline_state = nullptr;
        }
    }

} // namespace anxiety::rendering::backend::metal

#endif // ANXIETY_BACKEND_METAL
