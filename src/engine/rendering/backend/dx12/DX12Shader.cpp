#ifdef ANXIETY_BACKEND_DX12

#include "DX12Shader.h"

namespace anxiety::rendering::backend::dx12 {
    DX12Shader::DX12Shader(const rhi::ShaderDesc& desc, rhi::ShaderStage stage) : m_stage(stage) {
        if (desc.entry_point) m_entry_point = desc.entry_point;

        if (desc.bytecode && desc.bytecode_size > 0) {
            const auto* begin = static_cast<const uint8_t*>(desc.bytecode);
            m_bytecode.assign(begin, begin + desc.bytecode_size);
        }
    }

    D3D12_SHADER_BYTECODE DX12Shader::bytecode() const noexcept {
        D3D12_SHADER_BYTECODE bc{};

        if (!m_bytecode.empty()) {
            bc.pShaderBytecode = m_bytecode.data();
            bc.BytecodeLength  = m_bytecode.size();
        }

        return bc;
    }
} // namespace anxiety::rendering::backend::dx12

#endif ANXIETY_BACKEND_DX12