#ifdef ANXIETY_BACKEND_DX11

#include "DX11Shader.h"
#include "Logger.h"

namespace anxiety::rendering::backend::dx11 {
    static constexpr char k_category[] = "RHI";

    DX11Shader::DX11Shader(ID3D11Device* device, const rhi::ShaderDesc& desc, rhi::ShaderStage stage) : m_stage(stage), m_entry_point(desc.entry_point ? desc.entry_point : "main") {
        if (!desc.bytecode || desc.bytecode_size == 0) {
            LOG_ERROR(k_category, "DX11Shader: bytecode vacío.");
            return;
        }

        const uint8_t* data = static_cast<const uint8_t*>(desc.bytecode);
        m_bytecode.assign(data, data + desc.bytecode_size);

        HRESULT hr = S_OK;
        switch (stage) {
        case rhi::ShaderStage::Vertex:
            hr = device->CreateVertexShader(m_bytecode.data(), m_bytecode.size(), nullptr, &m_vs);
            if (FAILED(hr)) LOGF_ERROR(k_category, "CreateVertexShader falló: 0x{:08X}", static_cast<uint32_t>(hr));
            break;

        case rhi::ShaderStage::Fragment:
            hr = device->CreatePixelShader(m_bytecode.data(), m_bytecode.size(), nullptr, &m_ps);
            if (FAILED(hr)) LOGF_ERROR(k_category, "CreatePixelShader falló: 0x{:08X}", static_cast<uint32_t>(hr));
            break;

        case rhi::ShaderStage::Compute:
            hr = device->CreateComputeShader(m_bytecode.data(), m_bytecode.size(), nullptr, &m_cs);
            if (FAILED(hr)) LOGF_ERROR(k_category, "CreateComputeShader falló: 0x{:08X}", static_cast<uint32_t>(hr));
            break;
        }
    }
} // namespace anxiety::rendering::backend::dx11

#endif // ANXIETY_BACKEND_DX11
