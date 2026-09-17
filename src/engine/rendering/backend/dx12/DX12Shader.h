#ifdef ANXIETY_BACKEND_DX12

#pragma once

#include "../../rhi/ShaderTypes.h"

#include <d3d12.h>
#include <cstdint>
#include <string>
#include <vector>

namespace anxiety::rendering::backend::dx12 {
    // DX12Shader — implementación de rhi::IShader para DirectX 12 --------------------------------
    // Es propietario de una copia del bytecode DXIL compilado y lo expone como un D3D12_SHADER_BYTECODE
    // para la creación del PSO.
    // --------------------------------------------------------------------------------------------
    class DX12Shader final : public rhi::IShader {
    public:
        DX12Shader(const rhi::ShaderDesc& desc, rhi::ShaderStage stage);
        ~DX12Shader() override = default;

        // rhi::IShader ---------------------------------------------------------------------------
        [[nodiscard]] std::string_view entry_point() const noexcept override { return m_entry_point; }
        [[nodiscard]] rhi::ShaderStage stage()       const noexcept override { return m_stage; }

        // DX12 accessor --------------------------------------------------------------------------
        [[nodiscard]] D3D12_SHADER_BYTECODE bytecode() const noexcept;

        [[nodiscard]] bool is_valid() const noexcept { return !m_bytecode.empty(); }

    private:
        std::vector<uint8_t> m_bytecode;
        std::string          m_entry_point;
        rhi::ShaderStage     m_stage;
    };
} // namespace anxiety::rendering::backend::dx12

#endif ANXIETY_BACKEND_DX12