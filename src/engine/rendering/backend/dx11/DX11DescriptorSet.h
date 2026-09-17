#ifdef ANXIETY_BACKEND_DX11

#pragma once

#include "../../rhi/IDescriptorSet.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

namespace anxiety::rendering::backend::dx11 {
    using Microsoft::WRL::ComPtr;

    class DX11Device;  // forward

    // DX11DescriptorSet — implementación de rhi::IDescriptorSet para DirectX 11 ------------------
    // Almacena referencias a recursos de la GPU (buffers, SRVs) indexadas por slot de binding.
    // Cuando el command buffer llama a bind_descriptor_set(), apply() envía todos los bindings a
    // las etapas VS y PS del device context dado.
    //
    // DX11 no tiene descriptor heap — los recursos se enlazan por slot directamente en el context vía:
    //   VSSetConstantBuffers / PSSetConstantBuffers  — para UniformBuffer / StorageBuffer
    //   PSSetShaderResources / VSSetShaderResources  — para Texture
    class DX11DescriptorSet final : public rhi::IDescriptorSet {
    public:
        DX11DescriptorSet(DX11Device& device, const rhi::DescriptorSetLayout& layout);
        ~DX11DescriptorSet() override = default;

        // rhi::IDescriptorSet --------------------------------------------------------------------
        void update(const std::vector<rhi::DescriptorWrite>& writes) override;

        // Interno: llamado por DX11CommandBuffer::bind_descriptor_set ---------------------------
        void apply(ID3D11DeviceContext* ctx) const;

    private:
        struct CBVSlot { uint32_t binding; ID3D11Buffer* buf; };
        struct SRVSlot { uint32_t binding; ID3D11ShaderResourceView* srv; };

        DX11Device&              m_device;
        rhi::DescriptorSetLayout m_layout;
        std::vector<CBVSlot>     m_cbvs;
        std::vector<SRVSlot>     m_srvs;
    };
} // namespace anxiety::rendering::backend::dx11

#endif // ANXIETY_BACKEND_DX11
