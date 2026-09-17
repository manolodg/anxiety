#ifdef ANXIETY_BACKEND_DX11

#include "DX11DescriptorSet.h"
#include "DX11Device.h"
#include "Logger.h"

#include <algorithm>

namespace anxiety::rendering::backend::dx11 {
    static constexpr char k_category[] = "RHI";

    DX11DescriptorSet::DX11DescriptorSet(DX11Device& device, const rhi::DescriptorSetLayout& layout) : m_device(device), m_layout(layout) {}

    void DX11DescriptorSet::update(const std::vector<rhi::DescriptorWrite>& writes) {
        for (const auto& w : writes) {
            switch (w.type) {
            case rhi::DescriptorType::UniformBuffer:
            case rhi::DescriptorType::StorageBuffer: {
                ID3D11Buffer* buf = m_device.lookup_buffer(w.buffer);
                // Reemplaza el binding existente o inserta uno nuevo.
                auto it = std::find_if(m_cbvs.begin(), m_cbvs.end(), [&](const CBVSlot& s) { return s.binding == w.binding; });
                if (it != m_cbvs.end()) {
                    it->buf = buf;
                } else {
                    m_cbvs.push_back({ w.binding, buf });
                }
                break;
            }
            case rhi::DescriptorType::Texture: {
                ID3D11ShaderResourceView* srv = m_device.lookup_SRV(w.texture);
                auto it = std::find_if(m_srvs.begin(), m_srvs.end(), [&](const SRVSlot& s) { return s.binding == w.binding; });
                if (it != m_srvs.end()) {
                    it->srv = srv;
                } else {
                    m_srvs.push_back({ w.binding, srv });
                }
                break;
            }
            default:
                LOGF_WARNING(k_category, "DX11DescriptorSet::update: tipo de descriptor no soportado {}.", static_cast<uint32_t>(w.type));
                break;
            }
        }
    }

    void DX11DescriptorSet::apply(ID3D11DeviceContext* ctx) const {
        for (const auto& cbv : m_cbvs) {
            ID3D11Buffer* buf = cbv.buf;
            ctx->VSSetConstantBuffers(cbv.binding, 1, &buf);
            ctx->PSSetConstantBuffers(cbv.binding, 1, &buf);
            ctx->CSSetConstantBuffers(cbv.binding, 1, &buf);
        }
        for (const auto& srv : m_srvs) {
            ID3D11ShaderResourceView* s = srv.srv;
            ctx->PSSetShaderResources(srv.binding, 1, &s);
            ctx->VSSetShaderResources(srv.binding, 1, &s);
        }
    }
} // namespace anxiety::rendering::backend::dx11

#endif // ANXIETY_BACKEND_DX11
