#include "PlatformModule.h"
#include "Engine.h"
#include "Logger.h"

#if defined(ANXIETY_PLATFORM_WINDOWS)
#include "win32/Win32Platform.h"
#endif

namespace anxiety::platform {
    static constexpr std::string_view k_category = "Platform";

    // --------------------------------------------------------------------------------------------
    PlatformModule::PlatformModule(Config cfg) : m_config(std::move(cfg)) {}
    PlatformModule::~PlatformModule() = default;

    // --------------------------------------------------------------------------------------------
    bool PlatformModule::on_init(anxiety::Engine& engine) {
        m_engine = &engine;

        // Crear el backend de plataforma -----------------------------------------------------------
#if defined(ANXIETY_PLATFORM_WINDOWS)
        m_platform = std::make_unique<Win32Platform>();
#else
        LOG_ERROR(k_category, "No hay backend de plataforma disponible para este SO.");
        return false;
#endif

        IPlatform::set_current(m_platform.get());
        LOGF_INFO(k_category, "Backend de plataforma: {}.", m_platform->name());

        // Crear la ventana principal (se omite en modo headless) -----------------------------------
        if (!engine.config().headless) {
            m_window = m_platform->create_window(m_config.window);
            if (!m_window) {
                LOG_ERROR(k_category, "No se pudo crear la ventana principal.");
                return false;
            }
            LOGF_INFO(k_category, "Ventana '{}' creada ({}x{}).", m_config.window.title, m_config.window.width, m_config.window.height);
        } else {
            LOG_INFO(k_category, "Modo headless — se omite la creación de ventana.");
        }

        return true;
    }

    // --------------------------------------------------------------------------------------------
    void PlatformModule::on_update(float /*dt*/) {
        if (m_window && !m_window->poll_events()) {
            LOG_INFO(k_category, "Ventana cerrada — solicitando la detención del motor.");
            m_engine->request_stop();
        }
    }

    // --------------------------------------------------------------------------------------------
    void PlatformModule::on_shutdown() {
        m_window.reset();
        IPlatform::set_current(nullptr);
        m_platform.reset();
        LOG_INFO(k_category, "Backend de plataforma apagado.");
    }
} // namespace anxiety::platform
