#ifdef ANXIETY_BACKEND_DX12

#pragma once

#include "../../rhi/IDescriptorSet.h"

#include <d3d12.h>
#include <unordered_map>

namespace anxiety::rendering::backend::dx12 {
    class DX12Device;                                   // fwd — el tipo completo solo hace falta en el .cpp

    // DX12DescriptorSet — implementación de rhi::IDescriptorSet para DirectX 12 -------------------
    // Guarda el estado de GPU por binding (direcciones CBV para root CBVs inline; handles de
    // descriptor de GPU para descriptor tables de SRV).
    //
    // Creado por DX12Device::create_descriptor_set(). Llama a update() para vincular recursos
    // concretos; llama a bind() desde DX12CommandBuffer::bind_descriptor_set() para volcar los
    // bindings en los argumentos root de la command list.
    // --------------------------------------------------------------------------------------------
    class DX12DescriptorSet final : public rhi::IDescriptorSet {
    public:
        DX12DescriptorSet(DX12Device& device, const rhi::DescriptorSetLayout& layout);
        ~DX12DescriptorSet() override = default;

        // rhi::IDescriptorSet --------------------------------------------------------------------
        void update(const std::vector<rhi::DescriptorWrite>& writes) override;

        // Interno de DX12 -------------------------------------------------------------------------
        // Llamado por DX12CommandBuffer::bind_descriptor_set(). Itera los bindings del layout en
        // orden de declaración (= índice de parámetro root) y llama a SetGraphicsRootConstantBufferView / SetGraphicsRootDescriptorTable.
        void bind(ID3D12GraphicsCommandList* cmdList) const;

    private:
        DX12Device& m_device;
        anxiety::rendering::rhi::DescriptorSetLayout m_layout;

        // Slot de binding → estado de GPU cacheado
        std::unordered_map<uint32_t, D3D12_GPU_VIRTUAL_ADDRESS>   m_cbv_addresses;
        std::unordered_map<uint32_t, D3D12_GPU_DESCRIPTOR_HANDLE> m_srv_handles;
    };
} // namespace anxiety::rendering::backend::dx12

#endif ANXIETY_BACKEND_DX12