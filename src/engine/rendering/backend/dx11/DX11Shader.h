#ifdef ANXIETY_BACKEND_DX11

#pragma once

#include "../../rhi/ShaderTypes.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <string>
#include <vector>

namespace anxiety::rendering::backend::dx11 {
    using Microsoft::WRL::ComPtr;

    // DX11Shader — implementación de rhi::IShader para DirectX 11 --------------------------------
    // Posee el bytecode DXBC y el objeto ID3D11*Shader compilado.
    // El objeto de shader compilado se crea en el momento de la construcción a partir del bytecode,
    // de modo que queda listo para enlazarse de inmediato.
    class DX11Shader final : public rhi::IShader {
    public:
        DX11Shader(ID3D11Device* device, const rhi::ShaderDesc& desc, rhi::ShaderStage stage);
        ~DX11Shader() override = default;

        // rhi::IShader ---------------------------------------------------------------------------
        [[nodiscard]] std::string_view entry_point() const noexcept override { return m_entry_point; }
        [[nodiscard]] rhi::ShaderStage stage()       const noexcept override { return m_stage; }

        // Accesores DX11 -------------------------------------------------------------------------
        [[nodiscard]] bool                 is_valid()       const noexcept { return !m_bytecode.empty(); }
        [[nodiscard]] const void*          bytecode_data()  const noexcept { return m_bytecode.data(); }
        [[nodiscard]] size_t               bytecode_size()  const noexcept { return m_bytecode.size(); }
        [[nodiscard]] ID3D11VertexShader*  vertex_shader()  const noexcept { return m_vs.Get(); }
        [[nodiscard]] ID3D11PixelShader*   pixel_shader()   const noexcept { return m_ps.Get(); }
        [[nodiscard]] ID3D11ComputeShader* compute_shader() const noexcept { return m_cs.Get(); }

    private:
        std::vector<uint8_t>        m_bytecode;
        std::string                 m_entry_point;
        rhi::ShaderStage            m_stage;

        ComPtr<ID3D11VertexShader>  m_vs;
        ComPtr<ID3D11PixelShader>   m_ps;
        ComPtr<ID3D11ComputeShader> m_cs;
    };
} // namespace anxiety::rendering::backend::dx11

#endif // ANXIETY_BACKEND_DX11
