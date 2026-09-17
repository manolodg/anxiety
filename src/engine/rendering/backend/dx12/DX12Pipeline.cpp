#ifdef ANXIETY_BACKEND_DX12

#include "DX12Pipeline.h"
#include "DX12Shader.h"
#include "DX12Helpers.h"
#include "Logger.h"

#include <vector>

namespace anxiety::rendering::backend::dx12 {
    static constexpr char k_category[] = "RHI";

    DX12Pipeline::DX12Pipeline(ID3D12Device* device, const rhi::PipelineDesc& desc) {
        if (!device) return;
        if (desc.debug_name) m_debug_name = desc.debug_name;

        m_layout   = desc.descriptor_layout;
        m_topology = to_D3D12_topology(desc.topology);

        // Root signature -------------------------------------------------------------------------
        // Construye un parámetro root por cada binding de descriptor.
        // UniformBuffers → CBV root inline (sin necesidad de heap, GPU VA alineada a 256 bytes).
        // Textures       → descriptor table con un rango SRV.

        std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
        ranges.reserve(desc.descriptor_layout.bindings.size());

        // Primera pasada: recolecta todos los rangos de descriptor SRV/textura para que el vector
        // quede estable (sin reasignaciones) antes de tomar punteros hacia él.
        for (const auto& b : desc.descriptor_layout.bindings) {
            if (b.type == rhi::DescriptorType::Texture) {
                D3D12_DESCRIPTOR_RANGE r{};
                r.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                r.NumDescriptors                    = b.count;
                r.BaseShaderRegister                = b.binding;
                r.RegisterSpace                     = 0;
                r.OffsetInDescriptorsFromTableStart = 0;
                ranges.push_back(r);
            }
        }

        // Segunda pasada: construye los parámetros root, referenciando de forma segura los rangos ya estables.
        std::vector<D3D12_ROOT_PARAMETER> params;
        params.reserve(desc.descriptor_layout.bindings.size());
        uint32_t texRangeIdx = 0;

        for (const auto& b : desc.descriptor_layout.bindings) {
            D3D12_ROOT_PARAMETER p{};
            if (b.type == rhi::DescriptorType::UniformBuffer) {
                p.ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
                p.Descriptor.ShaderRegister = b.binding;
                p.Descriptor.RegisterSpace  = 0;
                p.ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;
            } else if (b.type == rhi::DescriptorType::Texture) {
                p.ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                p.DescriptorTable.NumDescriptorRanges = 1;
                p.DescriptorTable.pDescriptorRanges   = &ranges[texRangeIdx++];
                p.ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;
            } else {
                // StorageBuffer / Sampler: usaría UAV o sampler table (stub — se omite)
                continue;
            }
            params.push_back(p);
        }

        // Samplers estáticos -------------------------------------------------------------------
        std::vector<D3D12_STATIC_SAMPLER_DESC> static_samplers;
        static_samplers.reserve(desc.static_samplers.size());
        for (const auto& sd : desc.static_samplers) {
            static_samplers.push_back(to_D3D12_static_sampler(sd));
        }

        D3D12_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.NumParameters     = static_cast<UINT>(params.size());
        rsDesc.pParameters       = params.empty() ? nullptr : params.data();
        rsDesc.NumStaticSamplers = static_cast<UINT>(static_samplers.size());
        rsDesc.pStaticSamplers   = static_samplers.empty() ? nullptr : static_samplers.data();
        rsDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> serialized, errBlob;
        HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errBlob);
        if (FAILED(hr)) {
            if (errBlob) LOGF_ERROR(k_category, "Falló la serialización del root signature: {}", static_cast<const char*>(errBlob->GetBufferPointer()));
            return;
        }
        hr = device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&m_root_signature));
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "CreateRootSignature falló: 0x{:08X}", static_cast<uint32_t>(hr));
            return;
        }

        // Input layout ---------------------------------------------------------------------------
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
        inputElements.reserve(desc.vertex_layout.attributes.size());
        for (const auto& attr : desc.vertex_layout.attributes) {
            inputElements.push_back({ 
                attr.semantic_name,
                attr.semantic_index,
                to_D3D12_vertex_format(attr.format),
                attr.input_slot,
                attr.byte_offset,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            });
        }

        // Bytecode de shader ------------------------------------------------------------------------
        const auto* vs = static_cast<const DX12Shader*>(desc.vertex_shader);
        const auto* ps = static_cast<const DX12Shader*>(desc.fragment_shader);

        D3D12_SHADER_BYTECODE vsBytecode = vs ? vs->bytecode() : D3D12_SHADER_BYTECODE{};
        D3D12_SHADER_BYTECODE psBytecode = ps ? ps->bytecode() : D3D12_SHADER_BYTECODE{};

        // Descriptor del PSO -----------------------------------------------------------------------
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature                        = m_root_signature.Get();
        psoDesc.VS                                    = vsBytecode;
        psoDesc.PS                                    = psBytecode;
                                                      
        psoDesc.InputLayout.pInputElementDescs        = inputElements.empty() ? nullptr : inputElements.data();
        psoDesc.InputLayout.NumElements               = static_cast<UINT>(inputElements.size());
                                                      
        psoDesc.PrimitiveTopologyType                 = to_D3D12_topology_type(desc.topology);

        // Render targets
        const auto rtCount = static_cast<UINT>(std::min(desc.render_target_fmts.size(), size_t{ 8 }));
        psoDesc.NumRenderTargets = rtCount;
        for (UINT i = 0; i < rtCount; ++i) {
            psoDesc.RTVFormats[i]                     = to_D3D12_format(desc.render_target_fmts[i]);
        }                                             
                                                      
        psoDesc.DSVFormat                             = to_D3D12_format(desc.depth_stencil.depth_format);
                                                      
        psoDesc.SampleDesc.Count                      = 1;
        psoDesc.SampleDesc.Quality                    = 0;
        psoDesc.SampleMask                            = UINT_MAX;
                                                      
        // Rasterizador
        psoDesc.RasterizerState.FillMode              = to_D3D12_fill_mode(desc.rasterizer.fill_mode);
        psoDesc.RasterizerState.CullMode              = to_D3D12_cull_mode(desc.rasterizer.cull_mode);
        psoDesc.RasterizerState.FrontCounterClockwise = desc.rasterizer.front_face_CCW ? TRUE : FALSE;
        psoDesc.RasterizerState.DepthClipEnable       = TRUE;
        psoDesc.RasterizerState.DepthBias             = D3D12_DEFAULT_DEPTH_BIAS;
        psoDesc.RasterizerState.DepthBiasClamp        = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        psoDesc.RasterizerState.SlopeScaledDepthBias  = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;

        // Profundidad / stencil
        psoDesc.DepthStencilState.DepthEnable         = desc.depth_stencil.depth_test_enable ? TRUE : FALSE;
        psoDesc.DepthStencilState.DepthWriteMask      = desc.depth_stencil.depth_write_enable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState.DepthFunc           = to_D3D12_compare_op(desc.depth_stencil.depth_compare_op);
        psoDesc.DepthStencilState.StencilEnable       = FALSE;
        psoDesc.DepthStencilState.FrontFace           = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
        psoDesc.DepthStencilState.BackFace            = psoDesc.DepthStencilState.FrontFace;

        // Blend — todos los render targets comparten el mismo estado
        psoDesc.BlendState.AlphaToCoverageEnable      = FALSE;
        psoDesc.BlendState.IndependentBlendEnable     = FALSE;
        for (auto& rt : psoDesc.BlendState.RenderTarget) {
            rt.BlendEnable                            = desc.blend.blend_enable ? TRUE : FALSE;
            rt.LogicOpEnable                          = FALSE;
            rt.SrcBlend                               = to_D3D12_blend_factor(desc.blend.src_color_factor);
            rt.DestBlend                              = to_D3D12_blend_factor(desc.blend.dst_color_factor);
            rt.BlendOp                                = to_D3D12_blend_op(desc.blend.color_blend_op);
            rt.SrcBlendAlpha                          = to_D3D12_blend_factor(desc.blend.src_alpha_factor);
            rt.DestBlendAlpha                         = to_D3D12_blend_factor(desc.blend.dst_alpha_factor);
            rt.BlendOpAlpha                           = to_D3D12_blend_op(desc.blend.alpha_blend_op);
            rt.LogicOp                                = D3D12_LOGIC_OP_NOOP;
            rt.RenderTargetWriteMask                  = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso));
        if (FAILED(hr)) {
            LOGF_ERROR(k_category, "CreateGraphicsPipelineState falló: 0x{:08X}", static_cast<uint32_t>(hr));
            m_root_signature.Reset();
        }
    }
} // namespace anxiety::rendering::backend::dx12

#endif ANXIETY_BACKEND_DX12