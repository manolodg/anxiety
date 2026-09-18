#include "DrmKmsWindow.h"
#include "Logger.h"

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <string>

namespace anxiety::platform {
    static constexpr std::string_view k_category = "DrmKmsWindow";

    // Constructor --------------------------------------------------------------------------------
    DrmKmsWindow::DrmKmsWindow(const IWindow::Desc& desc) {
        // Prueba /dev/dri/card0, card1 en orden
        static const char* k_cards[] = { "/dev/dri/card0", "/dev/dri/card1", nullptr };
        bool opened = false;
        for (int i = 0; k_cards[i]; ++i) {
            if (open_device(k_cards[i])) { opened = true; break; }
        }
        if (!opened) {
            LOG_ERROR(k_category, "No se encontró ningún dispositivo DRM utilizable bajo /dev/dri/card*.");
            return;
        }

        if (!find_connector_and_mode(desc)) {
            LOGF_ERROR(k_category, "No se encontró ninguna pantalla conectada en el dispositivo DRM.");
            ::close(m_handle.drm_fd);
            m_handle.drm_fd = -1;
            return;
        }

        // Crea el device y la superficie GBM
        m_handle.gbm_dev = gbm_create_device(m_handle.drm_fd);
        if (!m_handle.gbm_dev) {
            LOG_ERROR(k_category, "gbm_create_device falló.");
            ::close(m_handle.drm_fd); m_handle.drm_fd = -1;
            return;
        }

        m_handle.gbm_surf = gbm_surface_create(m_handle.gbm_dev, m_handle.width, m_handle.height, GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

        if (!m_handle.gbm_surf) {
            LOG_ERROR(k_category, "gbm_surface_create falló.");
            gbm_device_destroy(m_handle.gbm_dev);
            m_handle.gbm_dev = nullptr;
            ::close(m_handle.drm_fd);
            m_handle.drm_fd = -1;
            return;
        }

        m_open = true;
        LOGF_INFO(k_category, "Superficie DRM/KMS creada {}x{}.", m_handle.width, m_handle.height);
    }

    // Destructor ---------------------------------------------------------------------------------
    DrmKmsWindow::~DrmKmsWindow() {
        if (m_handle.gbm_surf) { gbm_surface_destroy(m_handle.gbm_surf); }
        if (m_handle.gbm_dev) { gbm_device_destroy(m_handle.gbm_dev); }
        if (m_handle.drm_fd >= 0) { ::close(m_handle.drm_fd); }
    }

    // IWindow ------------------------------------------------------------------------------------
    bool DrmKmsWindow::poll_events() {
        // DRM/KMS no tiene bucle de eventos de window manager; la entrada llega desde libinput.
        return m_open;
    }

    void     DrmKmsWindow::close() { m_open = false; }
    bool     DrmKmsWindow::is_open()       const noexcept { return m_open; }
    uint32_t DrmKmsWindow::width()         const noexcept { return m_handle.width; }
    uint32_t DrmKmsWindow::height()        const noexcept { return m_handle.height; }
    void* DrmKmsWindow::native_handle() const noexcept {
        // Devuelve un puntero al struct del handle — el renderer lo convierte a DrmKmsNativeHandle*
        return const_cast<DrmKmsNativeHandle*>(&m_handle);
    }

    // Helpers privados -------------------------------------------------------------------------------
    bool DrmKmsWindow::open_device(const char* path) {
        int fd = ::open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0) return false;

        // Verifica que soporte modesetting
        drmModeResPtr res = drmModeGetResources(fd);
        if (!res) { ::close(fd); return false; }
        drmModeFreeResources(res);

        // Adquiere los derechos de master DRM para que VK_EXT_acquire_drm_display de Vulkan pueda
        // enumerar y adquirir la pantalla. En un arranque headless ya deberíamos ser master, pero
        // llamar a drmSetMaster es idempotente e inofensivo.
        drmSetMaster(fd);

        m_handle.drm_fd = fd;
        LOGF_INFO(k_category, "Dispositivo DRM '{}' abierto.", path);
        return true;
    }

    bool DrmKmsWindow::find_connector_and_mode(const IWindow::Desc& desc) {
        drmModeResPtr res = drmModeGetResources(m_handle.drm_fd);
        if (!res) return false;

        bool found = false;
        for (int ci = 0; ci < res->count_connectors && !found; ++ci) {
            drmModeConnectorPtr conn = drmModeGetConnector(m_handle.drm_fd, res->connectors[ci]);
            if (!conn) continue;

            if (conn->connection == DRM_MODE_CONNECTED && conn->count_modes > 0) {
                m_handle.connector_id = conn->connector_id;
                // Elige el modo preferido (el primero, normalmente la resolución más alta)
                const drmModeModeInfo& mode = conn->modes[0];
                m_handle.width = static_cast<uint32_t>(mode.hdisplay);
                m_handle.height = static_cast<uint32_t>(mode.vdisplay);

                // Si el Desc pidió un tamaño más pequeño, intenta encontrar un modo que coincida
                if (desc.width && desc.height) {
                    for (int mi = 0; mi < conn->count_modes; ++mi) {
                        if (conn->modes[mi].hdisplay == desc.width && conn->modes[mi].vdisplay == desc.height) {
                            m_handle.width = desc.width;
                            m_handle.height = desc.height;
                            break;
                        }
                    }
                }
                found = true;
            }
            drmModeFreeConnector(conn);
        }
        drmModeFreeResources(res);
        return found;
    }

} // namespace anxiety::platform
