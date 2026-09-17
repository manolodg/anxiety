#ifdef ANXIETY_BACKEND_DX11

#include "DX11Pipeline.h"
#include "DX11Shader.h"
#include "DX11Helpers.h"
#include "Logger.h"

namespace anxiety::rendering::backend::dx11 {
    static constexpr char k_category[] = "RHI";

    DX11Pipeline::DX11Pipeline(ID3D11Device* device, const rhi::PipelineDesc& desc) {
        if (desc.debug_name) m_debug_name = desc.debug_name;

        // Objetos de shader ------------------------------------------------------------------------
        if (!desc.vertex_shader || !desc.fragment_shader) {
            LOG_ERROR(k_category, "DX11Pipeline: VS o PS es nulo.");
            return;
        }

        auto* vs = static_cast<const DX11Shader*>(desc.vertex_shader);
        auto* ps = static_cast<const DX11Shader*>(desc.fragment_shader);

        if (!vs->is_valid() || !ps->is_valid()) {
            LOG_ERROR(k_category, "DX11Pipeline: bytecode de shader inválido.");
            return;
        }

        m_vs = vs->vertex_shader();
        m_ps = ps->pixel_shader();

        // Layout de entrada ------------------------------------------------------------------------
        if (!desc.vertex_layout.attributes.empty()) {
            std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
            elements.reserve(desc.vertex_layout.attributes.size());

            for (const auto& attr : desc.vertex_layout.attributes) {
                D3D11_INPUT_ELEMENT_DESC el{};
                el.SemanticName         = attr.semantic_name;
                el.SemanticIndex        = attr.semantic_index;
                el.Format               = to_DX11_vertex_format(attr.format);
                el.InputSlot            = attr.input_slot;
                el.AlignedByteOffset    = attr.byte_offset;
                el.InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA;
                el.InstanceDataStepRate = 0;
                elements.push_back(el);
            }

            HRESULT hr = device->CreateInputLayout(elements.data(), static_cast<UINT>(elements.size()), vs->bytecode_data(), vs->bytecode_size(), &m_input_layout);
            if (FAILED(hr)) {
                LOGF_ERROR(k_category, "CreateInputLayout falló: 0x{:08X}", static_cast<uint32_t>(hr));
                return;
            }
        }

        // Topología de primitivas -------------------------------------------------------------------
        m_topology = to_DX11_topology(desc.topology);

        // Estado del rasterizador --------------------------------------------------------------------
        {
            D3D11_RASTERIZER_DESC rd{};
            rd.FillMode              = to_DX11_fill_mode(desc.rasterizer.fill_mode);
            rd.CullMode              = to_DX11_cull_mode(desc.rasterizer.cull_mode);
            rd.FrontCounterClockwise = desc.rasterizer.front_face_CCW ? TRUE : FALSE;
            rd.DepthClipEnable       = TRUE;
            rd.MultisampleEnable     = FALSE;
            rd.AntialiasedLineEnable = FALSE;

            HRESULT hr = device->CreateRasterizerState(&rd, &m_raster_state);
            if (FAILED(hr)) {
                LOGF_ERROR(k_category, "CreateRasterizerState falló: 0x{:08X}", static_cast<uint32_t>(hr));
                return;
            }
        }

        // Estado de profundidad / plantilla ----------------------------------------------------------
        {
            D3D11_DEPTH_STENCIL_DESC dsd{};
            dsd.DepthEnable    = desc.depth_stencil.depth_test_enable ? TRUE : FALSE;
            dsd.DepthWriteMask = desc.depth_stencil.depth_write_enable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
            dsd.DepthFunc      = to_DX11_compare_func(desc.depth_stencil.depth_compare_op);
            dsd.StencilEnable  = FALSE;

            HRESULT hr = device->CreateDepthStencilState(&dsd, &m_depth_state);
            if (FAILED(hr)) {
                LOGF_ERROR(k_category, "CreateDepthStencilState falló: 0x{:08X}", static_cast<uint32_t>(hr));
                return;
            }
        }

        // Estado de blending -------------------------------------------------------------------------
        {
            D3D11_BLEND_DESC bd{};
            bd.AlphaToCoverageEnable  = FALSE;
            bd.IndependentBlendEnable = FALSE;

            auto& rt = bd.RenderTarget[0];
            rt.BlendEnable           = desc.blend.blend_enable ? TRUE : FALSE;
            rt.SrcBlend              = to_DX11_blend_factor(desc.blend.src_color_factor);
            rt.DestBlend             = to_DX11_blend_factor(desc.blend.dst_color_factor);
            rt.BlendOp               = to_DX11_blend_op(desc.blend.color_blend_op);
            rt.SrcBlendAlpha         = to_DX11_blend_factor(desc.blend.src_alpha_factor);
            rt.DestBlendAlpha        = to_DX11_blend_factor(desc.blend.dst_alpha_factor);
            rt.BlendOpAlpha          = to_DX11_blend_op(desc.blend.alpha_blend_op);
            rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

            HRESULT hr = device->CreateBlendState(&bd, &m_blend_state);
            if (FAILED(hr)) {
                LOGF_ERROR(k_category, "CreateBlendState falló: 0x{:08X}", static_cast<uint32_t>(hr));
                return;
            }
        }

        // Samplers estáticos --------------------------------------------------------------------------
        m_samplers.reserve(desc.static_samplers.size());
        for (const auto& sd : desc.static_samplers) {
            D3D11_SAMPLER_DESC d{};
            d.Filter         = to_DX11_filter(sd.filter);
            d.AddressU       = to_DX11_address_mode(sd.address_U);
            d.AddressV       = to_DX11_address_mode(sd.address_V);
            d.AddressW       = to_DX11_address_mode(sd.address_W);
            d.MipLODBias     = sd.mip_lod_bias;
            d.MaxAnisotropy  = sd.max_anisotropy;
            d.ComparisonFunc = D3D11_COMPARISON_NEVER;
            d.MinLOD         = sd.min_lod;
            d.MaxLOD         = sd.max_lod;

            ComPtr<ID3D11SamplerState> ss;
            HRESULT hr = device->CreateSamplerState(&d, &ss);
            if (FAILED(hr)) {
                LOGF_ERROR(k_category, "CreateSamplerState falló: 0x{:08X}", static_cast<uint32_t>(hr));
                return;
            }
            m_samplers.push_back(std::move(ss));
        }

        m_layout = desc.descriptor_layout;
        m_valid = true;
    }
} // namespace anxiety::rendering::backend::dx11

#endif // ANXIETY_BACKEND_DX11
