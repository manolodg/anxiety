#include "TextureManager.h"
#include "ImageLoader.h"
#include "rhi/IDevice.h"
#include "Logger.h"

#include <algorithm>

namespace anxiety::rendering::textures {
    static constexpr char k_category[] = "TextureManager";

    // Construcción / destrucción ---------------------------------------------------------------
    TextureManager::TextureManager(rhi::IDevice& device, std::string_view assets_dir) : m_device(device), m_assets_dir(assets_dir) {
        // Crea la textura nula 1×1 blanca.
        constexpr uint8_t kWhite[4] = { 255, 255, 255, 255 };
        m_null_texture = load_from_memory(kWhite, 1, 1, "NullTexture");
        if (!m_null_texture.is_valid()) {
            LOG_WARNING(k_category, "Falló la creación de la textura nula.");
        } else {
            LOG_INFO(k_category, "TextureManager en línea (textura nula lista).");
        }
    }

    TextureManager::~TextureManager() {
        for (auto h : m_owned) {
            if (h.is_valid()) m_device.destroy_texture(h);
        }
    }

    // Resolución de rutas ----------------------------------------------------------------------
    std::string TextureManager::resolve_path(std::string_view path) const {
        if (!path.empty() && (path[0] == '/' || (path.size() > 1 && path[1] == ':'))) return std::string(path);
        if (m_assets_dir.empty()) return std::string(path);

        return m_assets_dir + "/" + std::string(path);
    }

    // load — desde fichero, cacheada ----------------------------------------------------------
    rhi::TextureHandle TextureManager::load(std::string_view path) {
        const std::string full_path = resolve_path(path);

        // Acierto de caché
        auto it = m_cache.find(full_path);
        if (it != m_cache.end()) return it->second;

        // Decodifica la imagen de disco
        ImageData img = load_from_file(full_path);
        if (!img.is_valid()) {
            LOGF_WARNING(k_category, "load: falló la decodificación de '{}'. Se usa la textura nula.", full_path);
            return m_null_texture;
        }

        rhi::TextureHandle h = load_from_memory(img.pixels.data(), img.width, img.height, full_path);
        if (h.is_valid()) {
            m_cache.emplace(full_path, h);
            LOGF_INFO(k_category, "Textura '{}' cargada ({}×{}).", full_path, img.width, img.height);
        }
        return h;
    }

    // load_from_memory — crea + sube una textura a partir de píxeles RGBA8 en crudo -----------
    rhi::TextureHandle TextureManager::load_from_memory(const uint8_t* rgba8, int width, int height, std::string_view debug_name) {
        if (!rgba8 || width <= 0 || height <= 0) return {};

        rhi::TextureDesc desc;
        desc.extent     = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        desc.format     = rhi::Format::RGBA8_Unorm;
        desc.mip_levels = 1;
        desc.array_size = 1;
        desc.debug_name = debug_name.empty() ? nullptr : debug_name.data();

        rhi::TextureHandle h = m_device.create_texture(desc);
        if (!h.is_valid()) {
            LOGF_ERROR(k_category, "load_from_memory: create_texture falló para '{}'.", debug_name);
            return {};
        }

        m_device.upload_texture_data(h, rgba8, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        m_owned.push_back(h);
        return h;
    }

    // destroy ------------------------------------------------------------------------------------
    void TextureManager::destroy(rhi::TextureHandle handle) {
        if (!handle.is_valid()) return;
        // Elimina de la caché de rutas si está presente
        for (auto it = m_cache.begin(); it != m_cache.end(); ) {
            if (it->second.id == handle.id) {
                it = m_cache.erase(it);
                break;
            } else {
                ++it;
            }
        }
        // Elimina de la lista de handles en propiedad
        auto oit = std::find_if(m_owned.begin(), m_owned.end(), [&](rhi::TextureHandle h) { return h.id == handle.id; });
        if (oit != m_owned.end()) {
            m_owned.erase(oit);
            m_device.destroy_texture(handle);
        }
    }
} // namespace anxiety::rendering::textures
