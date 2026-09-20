#pragma once

#include "rhi/RHI.h"
#include "graph/RenderGraph.h"
#include "IModule.h"
#include "PlatformModule.h"

#include <memory>
#include <mutex>

// Declaraciones adelantadas para no arrastrar aquí las cabeceras de ECS y de escena.
namespace anxiety::ecs                  { class World; }
namespace anxiety::rendering::scene     { class SceneRenderer; }
namespace anxiety::rendering::materials { class MaterialManager; }
namespace anxiety::rendering::textures  { class TextureManager; }

namespace anxiety::rendering {
    // RenderingModule ----------------------------------------------------------------------------
    // IModule propietario del dispositivo de GPU, el swapchain, el estado de pipeline y el render
    // graph de cada fotograma.
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
    // Comportamiento por fotograma (modo con ventana):
    //   on_update() adquiere el siguiente backbuffer. Si hay un world de ECS adjuntado (vía
    //   set_world()), delega en SceneRenderer::build_passes() para construir ClearPass + ScenePass
    //   a partir de las entidades Camera/Transform/MeshRenderer. Si no, cae al triángulo RGB
    //   incorporado (ClearPass + TrianglePass con el pipeline por defecto). El grafo se compila,
    //   se graba en un command buffer, se envía, presenta y espera a que la GPU termine. Si
    //   todavía no hay ningún swapchain (modo headless, o modo embebido antes de attach_window()),
    //   on_update() no hace nada.
    //
    //   Los usuarios pueden ajustar el color de clear o inspeccionar el device/swapchain a través
    //   de los accesores de abajo para integrar sus propios pases de renderizado.
    // --------------------------------------------------------------------------------------------
    class RenderingModule final : public IModule {
    public:
        struct Config {
            bool            debug_layer       = false;                          // Activa las capas de validación D3D12/Vulkan
            rhi::ClearColor clear_color       = { 0.39f, 0.58f, 0.93f, 1.f };   // azul aciano
            rhi::RHIBackend preferred_backend = rhi::RHIBackend::Unknown;       // Unknown = selección automática
        };

        // Modo ventana propia del motor: crea el swapchain automáticamente en on_init() a partir
        // de la ventana de plat.
        explicit RenderingModule(anxiety::platform::PlatformModule& plat);
        explicit RenderingModule(anxiety::platform::PlatformModule& plat, Config cfg);
        // Modo ventana embebida: sin ventana propia. El swapchain no existe hasta que se llame a
        // attach_window().
        explicit RenderingModule(Config cfg = {});
        ~RenderingModule() override;

        // IModule --------------------------------------------------------------------------------
        [[nodiscard]] std::string_view              name()         const noexcept override { return "Rendering"; }
        [[nodiscard]] std::vector<std::string_view> dependencies() const          override { return m_plat ? std::vector<std::string_view>{ "Platform" } : std::vector<std::string_view>{}; }

        [[nodiscard]] bool on_init(Engine& engine) override;
        void               on_update(float dt)     override;
        void               on_shutdown()           override;

        // Ventana embebida (modo 2 de arriba) -----------------------------------------------------
        // Crea (o recrea) el swapchain contra una ventana nativa cuya propiedad es externa al
        // motor. El tipo de superficie (rhi::NativeSurfaceType) se infiere del SO de compilación —
        // ver current_platform_surface_type() en el .cpp — igual que ya se hacía para el modo
        // "ventana propia": el llamador nunca lo decide, porque este binario está compilado para un
        // único SO de todos modos. Seguro de llamar desde cualquier hilo, en cualquier momento tras
        // on_init(). Falso si el dispositivo todavía no está listo o si la creación del swapchain falla.
        [[nodiscard]] bool attach_window(void* native_window_handle, rhi::Extent2D extent);
        // Redimensiona el swapchain adjuntado con attach_window(). No-op si no hay ninguno.
        void               resize(rhi::Extent2D new_extent);
        // Destruye el swapchain adjuntado con attach_window(); on_update() vuelve a no hacer nada
        // hasta la siguiente llamada a attach_window().
        void               detach_window();

        // Accesores -------------------------------------------------------------------------------
        // Válido tras que on_init() devuelva true.
        [[nodiscard]] rhi::IDevice&       device()      noexcept { return *m_device; }
        // Nulo en modo headless, o en modo embebido antes de attach_window() / tras detach_window().
        [[nodiscard]] rhi::ISwapchain*    swapchain()   noexcept { return m_swapchain.get(); }
        [[nodiscard]] graph::RenderGraph& graph()       noexcept { return m_graph; }

        void set_clear_color(rhi::ClearColor c) noexcept { m_cfg.clear_color = c; }

        // Adjunta un world de ECS para activar el renderizado de escena. Llamar antes del primer
        // fotograma renderizado. Pasar nullptr revierte al triángulo incorporado como fallback.
        void set_world(anxiety::ecs::World* world);

        // Accede al SceneRenderer (válido tras on_init() con un swapchain).
        [[nodiscard]] scene::SceneRenderer*       scene_renderer() noexcept { return m_scene_renderer.get(); }
        // Accede al gestor de materiales (válido tras on_init()).
        [[nodiscard]] materials::MaterialManager* material_manager() noexcept { return m_material_manager.get(); }
        // Accede al gestor de texturas (válido tras on_init()).
        [[nodiscard]] textures::TextureManager*   texture_manager()  noexcept { return m_texture_manager.get(); }

    private:
        anxiety::platform::PlatformModule* m_plat = nullptr;      // nulo en modo ventana embebida
        Config                              m_cfg;

        // Orden de destrucción: recursos de pipeline → graph → cmdBuffer → swapchain → device.
        std::unique_ptr<rhi::IShader>        m_vertex_shader;
        std::unique_ptr<rhi::IShader>        m_fragment_shader;
        std::unique_ptr<rhi::IPipeline>      m_pipeline;
        rhi::BufferHandle                    m_vertex_buffer;
        rhi::BufferHandle                    m_constant_buffer;
        std::unique_ptr<rhi::IDescriptorSet> m_descriptor_set;

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

        // SceneRenderer opcional — activo cuando se ha llamado a set_world().
        std::unique_ptr<materials::MaterialManager> m_material_manager;
        std::unique_ptr<textures::TextureManager>   m_texture_manager;
        std::unique_ptr<scene::SceneRenderer>       m_scene_renderer;

        bool init_pipeline();                       // llamado desde on_init() cuando hay un swapchain
    };
} // namespace anxiety::rendering
