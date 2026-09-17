#ifdef ANXIETY_BACKEND_DX12

#include "DX12DescriptorSet.h"
#include "DX12Device.h"
#include "Logger.h"

namespace anxiety::rendering::backend::dx12 {
    DX12DescriptorSet::DX12DescriptorSet(DX12Device& device, const rhi::DescriptorSetLayout& layout) : m_device(device), m_layout(layout) {}
    void DX12DescriptorSet::update(const std::vector<rhi::DescriptorWrite>& writes) {
        for (const auto& w : writes) {
            switch (w.type) {
            case rhi::DescriptorType::UniformBuffer:
            case rhi::DescriptorType::StorageBuffer:
                if (w.buffer.is_valid()) {
                    const D3D12_GPU_VIRTUAL_ADDRESS va = m_device.buffer_GPU_VA(w.buffer);
                    if (va) m_cbv_addresses[w.binding] = va;
                }
                break;

            case rhi::DescriptorType::Texture:
                if (w.texture.is_valid()) {
                    auto* resource = m_device.lookup_texture(w.texture);
                    if (!resource) break;
                    auto alloc = m_device.allocate_SRV_descriptor();
                    if (alloc.cpu.ptr == 0) {
                        LOG_WARNING("RHI", "DX12DescriptorSet: heap de SRV agotado.");
                        break;
                    }
                    // Crea un SRV por defecto (todos los niveles de mip, todos los slices del array).
                    m_device.d3d_device()->CreateShaderResourceView(resource, nullptr, alloc.cpu);
                    m_srv_handles[w.binding] = alloc.gpu;
                }
                break;

            default:
                break;
            }
        }
    }

    void DX12DescriptorSet::bind(ID3D12GraphicsCommandList* cmd_list) const {
        // Índice de parámetro root = posición del binding en el orden de declaración del layout.
        // Debe coincidir con el root signature construido en DX12Pipeline.
        for (uint32_t i = 0; i < static_cast<uint32_t>(m_layout.bindings.size()); ++i) {
            const auto& b = m_layout.bindings[i];
            switch (b.type) {
            case rhi::DescriptorType::UniformBuffer:
            case rhi::DescriptorType::StorageBuffer: {
                auto it = m_cbv_addresses.find(b.binding);
                if (it != m_cbv_addresses.end()) cmd_list->SetGraphicsRootConstantBufferView(i, it->second);
                break;
            }
            case rhi::DescriptorType::Texture: {
                auto it = m_srv_handles.find(b.binding);
                if (it != m_srv_handles.end()) cmd_list->SetGraphicsRootDescriptorTable(i, it->second);
                break;
            }
            default:
                break;
            }
        }
    }

} // namespace anxiety::rendering::backend::dx12

#endif ANXIETY_BACKEND_DX12