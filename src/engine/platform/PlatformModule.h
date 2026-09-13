#pragma once

#include "IPlatform.h"
#include "IModule.h"

#include <memory>

namespace anxiety::platform {
	// PlatformModule -----------------------------------------------------------------------------
	// IModule propietario del backend IPlatform y de la IWindow principal.
	//
	// Registra este módulo antes que cualquier módulo que dependa de servicios de plataforma. El
	// resto de módulos deberían declarar "Platform" como dependencia para garantizar que se
	// inicializan después de este.
	//
	// Modo headless (engine.config().headless == true):
	//   El backend de plataforma se inicializa por completo (las APIs de timer / thread son
	//   utilizables). La creación de ventana se OMITE — window() devuelve nullptr.
	//
	// Cierre de ventana:
	//   Cuando poll_events() señala que la ventana se ha cerrado, on_update() llama
	//   automáticamente a engine.request_stop().
	// --------------------------------------------------------------------------------------------
    class PlatformModule final : public anxiety::IModule {
    public:
        struct Config {
            IWindow::Desc window;   // Se ignora en modo headless.
        };

        explicit PlatformModule(Config cfg = {});
        ~PlatformModule() override;

        // IModule
        [[nodiscard]] std::string_view name()            const noexcept override { return "Platform"; }
        [[nodiscard]] bool             on_init(anxiety::Engine& engine) override;
        void                           on_update(float dt)              override;
        void                           on_shutdown()                    override;

        // Accesores --------------------------------------------------------------------------------
        // Nulo en modo headless o antes de on_init().
        [[nodiscard]] IWindow*   window()   const noexcept { return m_window.get(); }
        // Válido tras on_init(); es el mismo objeto expuesto vía IPlatform::current().
        [[nodiscard]] IPlatform& platform() const noexcept { return *m_platform; }

    private:
        Config                     m_config;
        std::unique_ptr<IPlatform> m_platform;
        std::unique_ptr<IWindow>   m_window;
        anxiety::Engine*           m_engine{ nullptr };
    };
} // namespace anxiety::platform