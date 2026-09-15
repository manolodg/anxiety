#pragma once

#include "rhi/RHI.h"
#include "graph/RenderGraph.h"

#include "IModule.h"
#include "PlatformModule.h"

#include <memory>
#include <mutex>

namespace anxiety::rendering {
    // RenderingModule ----------------------------------------------------------------------------
    // IModule propietario del dispositivo de GPU, el swapchain y el render graph de cada fotograma.
    //
    // Dos modos de propiedad de la ventana:
    //
    //   1. Ventana propia del motor (demo standalone, p. ej. sandbox/main.cpp):
    //
    //        auto& platMod = engine.emplace_module<engine::platform::PlatformModule>(palCfg);
    //        auto& renMod  = engine.emplace_module<engine::rendering::RenderingModule>(platMod);
    //
    //      La dependencia "Platform" garantiza que PlatformModule se inicializa primero, y el
    //      swapchain se crea en on_init() contra la ventana que PlatformModule posee.
    //
    //   2. Ventana embebida, propiedad externa (p. ej. un HWND alojado por el editor Somatic vía
    //      anxiety_bridge — ver docs/somatic/architecture/viewport-engine-boundary.md):
    //
    //        auto& renMod = engine.emplace_module<engine::rendering::RenderingModule>();
    //        // ... más tarde, cuando el llamador externo ya tiene una ventana nativa lista:
    //        renMod.attach_window(native_handle, { width, height });
    //        // ... y al soltarla:
    //        renMod.detach_window();
    //
    //      Aquí no hay dependencia "Platform": el dispositivo se crea en on_init() igualmente,
    //      pero el swapchain no existe hasta que attach_window() lo crea, y puede volver a
    //      destruirse/recrearse en tiempo de ejecución cuantas veces haga falta. attach_window(),
    //      detach_window() y resize() son seguras de llamar desde un hilo distinto al que llama a
    //      on_update() (protegidas por un mutex interno); esa es precisamente la frontera que
    //      cruza el bridge nativo.
    //
    // Comportamiento por fotograma:
    //   on_update() adquiere el siguiente backbuffer, construye un pase de clear por defecto,
    //   compila el grafo, graba en un command buffer, lo envía, presenta y espera a que la GPU
    //   termine. Si todavía no hay ningún swapchain (modo headless, o modo embebido antes de
    //   attach_window()), on_update() no hace nada.
    //
    //   Los usuarios pueden ajustar el color de clear o inspeccionar el device/swapchain a través
    //   de los accesores de abajo para integrar sus propios pases de renderizado.
    // --------------------------------------------------------------------------------------------
    class RenderingModule final : public IModule {
    public:
        struct Config {
            bool            debug_layer = false;
            rhi::ClearColor clear_color = { 0.10f, 0.18f, 0.40f, 1.0f };
        };

        // Modo ventana propia del motor: crea el swapchain automáticamente en on_init() a partir
        // de la ventana de plat.
        explicit RenderingModule(platform::PlatformModule& plat, Config cfg = {});
        ~RenderingModule() override;

        // IModule --------------------------------------------------------------------------------
        [[nodiscard]] std::string_view              name()         const noexcept override { return "Rendering"; }

        [[nodiscard]] bool on_init(Engine& engine) override;
        void               on_update(float dt)     override;
        void               on_shutdown()           override;

        // Accesores -------------------------------------------------------------------------------
        // Válido tras que on_init() devuelva true.
        [[nodiscard]] rhi::IDevice& device()             noexcept { return *m_device; }
        // Nulo en modo headless, o en modo embebido antes de attach_window() / tras detach_window().
        [[nodiscard]] rhi::ISwapchain* swapchain()       noexcept { return m_swapchain.get(); }
        [[nodiscard]] graph::RenderGraph& graph()        noexcept { return m_graph; }

        void          set_clear_color(rhi::ClearColor c) noexcept { m_config.clear_color = c; }

    private:
        platform::PlatformModule* m_platform = nullptr;         // nulo en modo ventana embebida
        Config                    m_config;

        // C++ destruye los miembros en orden inverso al de su declaración.
        // Para conseguir: graph → cmdBuffer → swapchain → device (primero en destruirse → último en destruirse)
        // Se declaran en el orden opuesto: device primero, graph al final.
        std::unique_ptr<rhi::IDevice>        m_device;
        std::unique_ptr<rhi::ISwapchain>     m_swapchain;
        std::unique_ptr<rhi::ICommandBuffer> m_cmd_buffer;
        graph::RenderGraph                   m_graph;

        // Protege m_swapchain frente a acceso concurrente entre on_update() (hilo del motor) y
        // attach_window()/resize()/detach_window() (potencialmente llamados desde otro hilo, p.
        // ej. el hilo de UI de Avalonia a través de anxiety_bridge).
        std::mutex m_swapchain_mutex;
    };
} // namespace anxiety::rendering