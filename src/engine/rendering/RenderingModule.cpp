#include "RenderingModule.h"
#include "Logger.h"

// Selección de backend — en las cabeceras de esta unidad de traducción no se permite ningún tipo de D3D12/Vulkan.
#ifdef _WIN32
#include "backend/dx12/DX12Device.h"
#endif

namespace anxiety::rendering {
    static constexpr char k_category[] = "Rendering";

    // Construcción / destrucción --------------------------------------------------------------------
    RenderingModule::RenderingModule(platform::PlatformModule& platform, Config config) : m_platform(&platform), m_config(config) {}
    RenderingModule::~RenderingModule() = default;

    // Ciclo de vida de IModule ------------------------------------------------------------------------
    bool RenderingModule::on_init(Engine& /*engine*/) {
        // Dispositivo del backend - seleccionado automáticamente vía RHIFactory --------------------
        {
            const rhi::RHIBackend sel = rhi::RHIFactory::select_backend(m_config.preferred_backend);

            // OpenGL necesita un contexto de renderizado vinculado a una ventana antes de que
            // gladLoadGL() pueda cargar los punteros a función. Se pasa el handle de ventana nativo
            // para que el backend pueda crearlo. DX12, DX11, Vulkan y Metal ignoran este parámetro.
            void* native_window = nullptr;
            if (m_platform && (sel == rhi::RHIBackend::OpenGL || sel == rhi::RHIBackend::OpenGLES)) {
                if (const auto* w = m_platform->window()) native_window = w->native_handle();
            }

            m_device = rhi::RHIFactory::create_device(sel, m_config.debug_layer, native_window);
            if (!m_device) {
                LOG_FATAL(k_category, "No hay ningún backend de renderizado disponible en esta plataforma.");
                return false;
            }
        }

        LOGF_INFO(k_category, "Backend: {}.", m_device->backend_name());

        // Crea el swapchain a partir de la ventana propia del motor, si la hay (modo 1) ----------
        if (m_platform) {
            const auto* window = m_platform->window();
            if (window && window->is_open()) {
                rhi::SwapchainDesc sc_desc;
                sc_desc.native_window_handle = window->native_handle();
                sc_desc.extent               = { window->width(), window->height() };
                sc_desc.image_count          = 2;
                sc_desc.vsync                = true;

                m_swapchain = m_device->create_swapchain(sc_desc);
                if (!m_swapchain) {
                    LOG_FATAL(k_category, "No se pudo crear el swapchain.");
                    return false;
                }
            } else {
                LOG_INFO(k_category, "Modo headless — se omite el swapchain.");
            }
        } else {
            // Modo ventana embebida (modo 2): sin ventana todavía. attach_window() creará el
            // swapchain más tarde, cuando el llamador externo tenga una ventana nativa lista.
            LOG_INFO(k_category, "Sin ventana propia — esperando a que se adjunte una ventana externa (attach_window()).");
        }

        // Crea el command buffer persistente ------------------------------------------------------
        m_cmd_buffer = m_device->create_command_buffer();
        if (!m_cmd_buffer) {
            LOG_FATAL(k_category, "No se pudo crear el command buffer.");
            return false;
        }

        LOG_INFO(k_category, "RenderingModule en línea.");
        return true;
    }

    void RenderingModule::on_update(float /*dt*/) {
        std::lock_guard lock(m_swapchain_mutex);

        // Nada que renderizar en modo headless, o en modo embebido antes de attach_window().
        if (!m_swapchain) return;

        // Construye el render graph de este fotograma ---------------------------------------------
        m_graph.reset();

        m_swapchain->acquire_next_image();
        const rhi::TextureHandle bb = m_swapchain->current_backbuffer();

        // Importa el backbuffer del swapchain como recurso del grafo.
        const auto bb_graph = m_graph.import_texture("backbuffer", bb, rhi::ResourceState::Present);

        // Pase de clear por defecto — azul aciano.
        const rhi::ClearColor clear_color = m_config.clear_color;
        m_graph.add_pass({
            .name    = "ClearPass",
            .writes  = { bb_graph },
            .execute = [bb, clear_color](rhi::ICommandBuffer& cmd) {
                cmd.resource_barrier(bb, rhi::ResourceState::Present, rhi::ResourceState::RenderTarget);
                cmd.clear_render_target(bb, clear_color);
                cmd.resource_barrier(bb, rhi::ResourceState::RenderTarget, rhi::ResourceState::Present);
            }
            });

        m_graph.compile();

        // Grabación --------------------------------------------------------------------------------
        m_cmd_buffer->begin();
        m_graph.execute(*m_cmd_buffer);
        m_cmd_buffer->end();

        // Envío → presentación → sincronización ----------------------------------------------------
        m_device->submit(*m_cmd_buffer);
        m_swapchain->present();
        m_device->wait_idle();                      // sincronización simple por fotograma; el triple buffering queda como TODO futuro
    }

    void RenderingModule::on_shutdown() {
        if (m_device) {
            m_device->wait_idle();
            // Libera explícitamente los recursos de GPU mientras el HWND sigue vivo (modo 1:
            // PlatformModule::on_shutdown() se ejecuta DESPUÉS de esto y llama a DestroyWindow(); si
            // IDXGISwapChain3 sigue vivo en ese momento, el hook MakeWindowAssociation de DXGI
            // dispara una limpieza WM_NCDESTROY que puede corromper la cola de comandos D3D12).
            // En modo 2, el llamador ya debería haber llamado a detach_window() antes de destruir
            // su ventana; esto es una red de seguridad adicional.
            std::lock_guard lock(m_swapchain_mutex);
            m_cmd_buffer.reset();
            m_swapchain.reset();
        }
        LOG_INFO(k_category, "RenderingModule desconectado.");
    }
} // namespace anxiety::rendering
