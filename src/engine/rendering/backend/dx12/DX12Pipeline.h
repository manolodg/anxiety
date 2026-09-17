#ifdef ANXIETY_BACKEND_DX12

#pragma once

#include "../../rhi/IPipeline.h"
#include "../../rhi/PipelineDesc.h"

#include <d3d12.h>
#include <wrl/client.h>
#include <string>

namespace anxiety::rendering::backend::dx12 {
    using Microsoft::WRL::ComPtr;

    // DX12Pipeline — implementación de rhi::IPipeline para DirectX 12 ----------------------------
    // Es propietario de un ID3D12PipelineState y su ID3D12RootSignature correspondiente. Creado por
    // DX12Device::create_pipeline(). Inmutable tras la construcción.
    // --------------------------------------------------------------------------------------------
    class DX12Pipeline final : public rhi::IPipeline {
    public:
        // Se construye a partir de un PipelineDesc. Comprueba is_valid() antes de usarlo.
        DX12Pipeline(ID3D12Device* device, const rhi::PipelineDesc& desc);
        ~DX12Pipeline() override = default;

        // rhi::IPipeline -------------------------------------------------------------------------
        [[nodiscard]] std::string_view debug_name() const noexcept override { return m_debug_name; }

        // Accesores DX12 (usados por DX12CommandBuffer) -------------------------------------------
        [[nodiscard]] bool                   is_valid()       const noexcept { return m_pso && m_root_signature; }
        [[nodiscard]] ID3D12PipelineState*   pso()            const noexcept { return m_pso.Get(); }
        [[nodiscard]] ID3D12RootSignature*   root_signature() const noexcept { return m_root_signature.Get(); }
        [[nodiscard]] D3D_PRIMITIVE_TOPOLOGY d3d_topology()   const noexcept { return m_topology; }

        // Layout guardado para que DX12DescriptorSet pueda calcular los índices root correctos.
        [[nodiscard]] const rhi::DescriptorSetLayout& descriptor_layout() const noexcept { return m_layout; }

    private:
        ComPtr<ID3D12PipelineState> m_pso;
        ComPtr<ID3D12RootSignature> m_root_signature;
        D3D_PRIMITIVE_TOPOLOGY      m_topology        = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        rhi::DescriptorSetLayout    m_layout;
        std::string                 m_debug_name;
    };
} // namespace anxiety::rendering::backend::dx12

#endif ANXIETY_BACKEND_DX12