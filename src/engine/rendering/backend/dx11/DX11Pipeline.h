#ifdef ANXIETY_BACKEND_DX11

#pragma once

#include "../../rhi/IPipeline.h"
#include "../../rhi/PipelineDesc.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>

namespace anxiety::rendering::backend::dx11 {
    using Microsoft::WRL::ComPtr;

    class DX11Shader;  // forward

    // DX11Pipeline — implementación de rhi::IPipeline para DirectX 11 ----------------------------
    // Posee todos los objetos de estado inmutables de D3D11 que en conjunto definen un PSO:
    //   Etapa IA  — InputLayout, PrimitiveTopology
    //   VS / PS   — punteros crudos a objetos DX11Shader (no poseídos aquí)
    //   Etapa RS  — RasterizerState
    //   Etapa OM  — DepthStencilState, BlendState
    //   Samplers  — un ID3D11SamplerState por cada sampler estático del PipelineDesc
    class DX11Pipeline final : public rhi::IPipeline {
    public:
        DX11Pipeline(ID3D11Device* device, const rhi::PipelineDesc& desc);
        ~DX11Pipeline() override = default;

        // rhi::IPipeline -------------------------------------------------------------------------
        [[nodiscard]] std::string_view debug_name() const noexcept override { return m_debug_name; }

        // Accesores DX11 (usados por DX11CommandBuffer) ------------------------------------------
        [[nodiscard]] bool                       is_valid()         const noexcept { return m_valid; }
        [[nodiscard]] ID3D11VertexShader*        vs()               const noexcept { return m_vs; }
        [[nodiscard]] ID3D11PixelShader*         ps()               const noexcept { return m_ps; }
        [[nodiscard]] ID3D11InputLayout*         input_layout()     const noexcept { return m_input_layout.Get(); }
        [[nodiscard]] D3D11_PRIMITIVE_TOPOLOGY   topology()         const noexcept { return m_topology; }
        [[nodiscard]] ID3D11RasterizerState*     rasterizer_state() const noexcept { return m_raster_state.Get(); }
        [[nodiscard]] ID3D11DepthStencilState*   depth_state()      const noexcept { return m_depth_state.Get(); }
        [[nodiscard]] ID3D11BlendState*          blend_state()      const noexcept { return m_blend_state.Get(); }

        [[nodiscard]] const std::vector<ComPtr<ID3D11SamplerState>>& sampler_states() const noexcept { return m_samplers; }
        [[nodiscard]] const rhi::DescriptorSetLayout& descriptor_layout()             const noexcept { return m_layout; }

    private:
        bool        m_valid = false;
        std::string m_debug_name;

        // Punteros crudos (sin propiedad) a los objetos de shader pasados vía PipelineDesc.
        ID3D11VertexShader*  m_vs = nullptr;
        ID3D11PixelShader*   m_ps = nullptr;

        ComPtr<ID3D11InputLayout>       m_input_layout;
        D3D11_PRIMITIVE_TOPOLOGY        m_topology   = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        ComPtr<ID3D11RasterizerState>   m_raster_state;
        ComPtr<ID3D11DepthStencilState> m_depth_state;
        ComPtr<ID3D11BlendState>        m_blend_state;

        std::vector<ComPtr<ID3D11SamplerState>> m_samplers;
        rhi::DescriptorSetLayout                m_layout;
    };
} // namespace anxiety::rendering::backend::dx11

#endif // ANXIETY_BACKEND_DX11
