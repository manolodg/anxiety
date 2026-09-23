#include "AnxietyBridge.h"

#include "Engine.h"
#include "Logger.h"
#include "RenderingModule.h"

#include <memory>
#include <string>
#include <thread>

struct AnxietyEngineHandle_ {
    std::unique_ptr<anxiety::Engine>     engine;
    anxiety::rendering::RenderingModule* rendering     = nullptr;   // propiedad de engine
    std::string                          backend_name;
    std::thread                          run_thread;
};

AnxietyEngineHandle anxiety_bridge_create(void) {
    auto handle = std::make_unique<AnxietyEngineHandle_>();

    handle->engine = std::make_unique<anxiety::Engine>(anxiety::EngineConfig{ .app_name = "Somatic", .headless = true });

    // Modo ventana embebida: sin PlatformModule. El backend es el por defecto de la plataforma
    // (RHIBackend::Unknown = selección automática) y el swapchain no existe hasta attach_window().
    handle->rendering = &handle->engine->emplace_module<anxiety::rendering::RenderingModule>();

    if (!handle->engine->init()) return nullptr;

    handle->backend_name = std::string(handle->rendering->device().backend_name());

    anxiety::Engine* engine = handle->engine.get();
    handle->run_thread = std::thread([engine] { engine->run(); });

    return handle.release();
}

void anxiety_bridge_destroy(AnxietyEngineHandle handle) {
    if (!handle) return;

    handle->engine->request_stop();
    if (handle->run_thread.joinable()) handle->run_thread.join();

    delete handle;
}

int anxiety_bridge_attach_window(AnxietyEngineHandle handle, void* native_window, uint32_t width, uint32_t height) {
    if (!handle || !native_window || width == 0 || height == 0) return 0;
    return handle->rendering->attach_window(native_window, { width, height }) ? 1 : 0;
}

void anxiety_bridge_resize(AnxietyEngineHandle handle, uint32_t width, uint32_t height) {
    if (!handle) return;
    handle->rendering->resize({ width, height });
}

void anxiety_bridge_detach_window(AnxietyEngineHandle handle) {
    if (!handle) return;
    handle->rendering->detach_window();
}

const char* anxiety_bridge_backend_name(AnxietyEngineHandle handle) {
    return handle ? handle->backend_name.c_str() : nullptr;
}
