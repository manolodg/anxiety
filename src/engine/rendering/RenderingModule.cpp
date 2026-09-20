#include "RenderingModule.h"
#include "materials/MaterialManager.h"
#include "scene/SceneRenderer.h"
#include "Logger.h"

// Selección de backend — en las cabeceras de esta unidad de traducción no se permite ningún tipo de D3D12/Vulkan.
#ifdef _WIN32
#include "backend/dx12/DX12Device.h"
#endif

namespace anxiety::rendering {
    static constexpr char k_category[] = "Rendering";

    namespace {
        // current_platform_surface_type -----------------------------------------------------------
        // rhi::NativeSurfaceType del SO en el que se compiló este binario. Usado tanto por el modo
        // "ventana propia" (on_init()) como por el modo "ventana embebida" (attach_window()): en
        // ambos casos el handle que llega es siempre del mismo tipo de superficie, porque este
        // binario en concreto solo se compila contra un backend de plataforma (ver PlatformModule) —
        // no hace falta que nadie lo declare explícitamente en cada llamada. Deuda menor: IWindow
        // todavía no expone su propio NativeSurfaceType (ver
        // docs/somatic/architecture/viewport-engine-boundary.md), así que se infiere aquí en vez de
        // preguntárselo a la ventana.
        rhi::NativeSurfaceType current_platform_surface_type() {
#if defined(_WIN32)
            return rhi::NativeSurfaceType::Win32;
#elif defined(__APPLE__)
            return rhi::NativeSurfaceType::MacOS;
#else
            return rhi::NativeSurfaceType::X11;
#endif
        }
    } // namespace

    // Inline HLSL shaders ------------------------------------------------------------------------
    static constexpr const char* k_vertex_shader_HLSL = R"hlsl(
struct VSIn  { float3 pos : POSITION; float4 col : COLOR; };
struct VSOut { float4 pos : SV_Position; float4 col : COLOR; };
cbuffer SceneCB : register(b0) { float4 tint; };
VSOut VSMain(VSIn i) {
    VSOut o;
    o.pos = float4(i.pos, 1.0);
    o.col = i.col * tint;
    return o;
}
)hlsl";

    static constexpr const char* k_pixel_shader_HLSL = R"hlsl(
struct VSOut { float4 pos : SV_Position; float4 col : COLOR; };
float4 PSMain(VSOut i) : SV_Target { return i.col; }
)hlsl";

    // Triangle geometry --------------------------------------------------------------------------
    struct TriVertex { float x, y, z; float r, g, b, a; };
    static constexpr TriVertex k_triangle_verts[] = {
        {  0.0f,  0.5f, 0.0f,   1.f, 0.f, 0.f, 1.f },   // top     — red
        {  0.5f, -0.5f, 0.0f,   0.f, 1.f, 0.f, 1.f },   // right   — green
        { -0.5f, -0.5f, 0.0f,   0.f, 0.f, 1.f, 1.f },   // left    — blue
    };

    // Construcción / destrucción --------------------------------------------------------------------
    RenderingModule::RenderingModule(anxiety::platform::PlatformModule& plat) : RenderingModule(plat, Config{}) {}
    RenderingModule::RenderingModule(anxiety::platform::PlatformModule& plat, Config cfg) : m_plat(&plat), m_cfg(cfg) {}
    RenderingModule::RenderingModule(Config cfg) : m_plat(nullptr), m_cfg(cfg) {}
    RenderingModule::~RenderingModule() = default;

    // Ciclo de vida de IModule ------------------------------------------------------------------------
    bool RenderingModule::on_init(Engine& /*engine*/) {
        // Dispositivo del backend - seleccionado automáticamente vía RHIFactory --------------------
        {
            const rhi::RHIBackend sel = rhi::RHIFactory::select_backend(m_cfg.preferred_backend);

            // OpenGL necesita un contexto de renderizado vinculado a una ventana antes de que
            // gladLoadGL() pueda cargar los punteros a función. Se pasa el handle de ventana nativo
            // para que el backend pueda crearlo. DX12, DX11, Vulkan y Metal ignoran este parámetro.
            void* native_window = nullptr;
            if (m_plat && (sel == rhi::RHIBackend::OpenGL || sel == rhi::RHIBackend::OpenGLES)) {
                if (const auto* w = m_plat->window()) native_window = w->native_handle();
            }

            m_device = rhi::RHIFactory::create_device(sel, m_cfg.debug_layer, native_window);
            if (!m_device) {
                LOG_FATAL(k_category, "No hay ningún backend de renderizado disponible en esta plataforma.");
                return false;
            }
        }

        LOGF_INFO(k_category, "Backend: {}.", m_device->backend_name());

        // Crea el swapchain a partir de la ventana propia del motor, si la hay (modo 1) ----------
        if (m_plat) {
            const auto* window = m_plat->window();
            if (window && window->is_open()) {
                rhi::SwapchainDesc sc_desc;
                sc_desc.surface_type         = current_platform_surface_type();
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

        // Pipeline + geometría (solo con ventana) --------------------------------------------------
        if (m_swapchain && !init_pipeline()) {
            LOG_WARNING(k_category, "Falló la inicialización del pipeline — pase de triángulo desactivado.");
            // No es fatal: el clear pass sigue funcionando.
        }

        // Gestor de materiales — en la construcción carga desde disco los shaders incorporados.
        m_material_manager = std::make_unique<materials::MaterialManager>(*m_device);
        // SceneRenderer — recibe un puntero al gestor de materiales.
        m_scene_renderer = std::make_unique<scene::SceneRenderer>(*m_device, m_material_manager.get());

        LOG_INFO(k_category, "RenderingModule en línea.");
        return true;
    }

    bool RenderingModule::init_pipeline() {
        // Compila los shaders
        auto vsBc = m_device->compile_shader_from_source(k_vertex_shader_HLSL, "VSMain", rhi::ShaderStage::Vertex);
        auto psBc = m_device->compile_shader_from_source(k_pixel_shader_HLSL, "PSMain", rhi::ShaderStage::Fragment);
        if (vsBc.empty() || psBc.empty()) {
            LOG_ERROR(k_category, "Falló la compilación de shaders.");
            return false;
        }

        m_vertex_shader = m_device->create_shader({ vsBc.data(), vsBc.size(), "VSMain" }, rhi::ShaderStage::Vertex);
        m_fragment_shader = m_device->create_shader({ psBc.data(), psBc.size(), "PSMain" }, rhi::ShaderStage::Fragment);
        if (!m_vertex_shader || !m_fragment_shader) return false;

        // Descriptor del pipeline
        rhi::PipelineDesc pd;
        pd.vertex_shader = m_vertex_shader.get();
        pd.fragment_shader = m_fragment_shader.get();
        pd.vertex_layout = {
            .attributes = {
                { "POSITION", 0, rhi::VertexFormat::Float3, 0,  0 },
                { "COLOR",    0, rhi::VertexFormat::Float4, 0, 12 },
            },
            .stride_bytes = sizeof(TriVertex)
        };
        pd.topology = rhi::PrimitiveTopology::TriangleList;
        pd.rasterizer.cull_mode = rhi::CullMode::None;   // ambas caras visibles en la demo NDC
        pd.render_target_fmts = { rhi::Format::BGRA8_Unorm };
        pd.descriptor_layout = { .bindings = { { 0, rhi::DescriptorType::UniformBuffer } } };
        pd.debug_name = "TrianglePipeline";

        m_pipeline = m_device->create_pipeline(pd);
        if (!m_pipeline) return false;

        // Vertex buffer
        rhi::BufferDesc vbDesc{ sizeof(k_triangle_verts), rhi::BufferUsage::Vertex, "TriangleVB" };
        m_vertex_buffer = m_device->create_buffer(vbDesc, k_triangle_verts, sizeof(k_triangle_verts));
        if (!m_vertex_buffer.is_valid()) return false;

        // Constant buffer: tint = blanco (deja pasar los colores del vértice sin modificarlos)
        const float tint[4] = { 1.f, 1.f, 1.f, 1.f };
        rhi::BufferDesc cbDesc{ 256, rhi::BufferUsage::Uniform, "TintCB" };
        m_constant_buffer = m_device->create_buffer(cbDesc, tint, sizeof(tint));
        if (!m_constant_buffer.is_valid()) return false;

        // Descriptor set
        rhi::DescriptorSetLayout dsLayout{ .bindings = { { 0, rhi::DescriptorType::UniformBuffer } } };
        m_descriptor_set = m_device->create_descriptor_set(dsLayout);
        m_descriptor_set->update({ { 0, rhi::DescriptorType::UniformBuffer, m_constant_buffer } });

        LOGF_INFO(k_category, "Pipeline de triángulo listo ({} bytes VS, {} bytes PS).", vsBc.size(), psBc.size());
        return true;
    }

    void RenderingModule::set_world(anxiety::ecs::World* world) { if (m_scene_renderer) m_scene_renderer->attach_world(world); }

    void RenderingModule::on_update(float /*dt*/) {
        std::lock_guard lock(m_swapchain_mutex);

        // Nada que renderizar en modo headless, o en modo embebido antes de attach_window().
        if (!m_swapchain) return;

        // Construye el render graph de este fotograma ---------------------------------------------
        m_graph.reset();

        m_swapchain->acquire_next_image();
        const rhi::TextureHandle bb = m_swapchain->current_backbuffer();

        // Importa el backbuffer del swapchain como recurso del grafo. initial_state=Present (estado al
        // inicio del fotograma), final_state=Present (necesario para presentar). El grafo emite
        // automáticamente el barrier Present→RenderTarget antes del ClearPass y el barrier final
        // RenderTarget→Present.
        const auto bb_graph = m_graph.import_texture("backbuffer", bb, rhi::ResourceState::Present, rhi::ResourceState::Present);

        // Calcula el aspect ratio a partir de la extensión actual del swapchain.
        const auto ext = m_swapchain->extent();
        const float aspect = (ext.height > 0) ? static_cast<float>(ext.width) / static_cast<float>(ext.height) : 16.f / 9.f;

        // Camino del SceneRenderer (cuando hay un world adjuntado) --------------------------------
        if (m_scene_renderer && m_scene_renderer->world()) {
            m_scene_renderer->build_passes(m_graph, bb_graph, bb, m_cfg.clear_color, aspect);
        }
        else {
            // Fallback — clear + triángulo RGB incorporados ---------------------------------------
            const rhi::ClearColor clearColor = m_cfg.clear_color;
            m_graph.add_pass({
                .name = "ClearPass",
                .writes = {{ bb_graph, rhi::ResourceState::RenderTarget }},
                .execute = [bb, clearColor](rhi::ICommandBuffer& cmd) { cmd.clear_render_target(bb, clearColor); }
                });

            if (m_pipeline) {
                const rhi::BufferHandle vb = m_vertex_buffer;
                rhi::IPipeline* pipe = m_pipeline.get();
                rhi::IDescriptorSet* ds = m_descriptor_set.get();

                m_graph.add_pass({
                    .name    = "TrianglePass",
                    .reads   = {{ bb_graph, rhi::ResourceState::RenderTarget }},
                    .writes  = {{ bb_graph, rhi::ResourceState::RenderTarget }},
                    .execute = [pipe, ds, vb](rhi::ICommandBuffer& cmd) {
                        cmd.bind_pipeline(*pipe);
                        cmd.bind_descriptor_set(0, *ds);
                        cmd.bind_vertex_buffer(0, vb, 0, sizeof(TriVertex));
                        cmd.draw(3, 1, 0, 0);
                    }
                    });
            }
        }

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
            // Libera los objetos que dependen del dispositivo ANTES de que este se destruya. Sus
            // miembros se declaran antes que m_device, así que C++ los destruiría después de él
            // (vkDestroyPipeline / vkFreeDescriptorSets sobre un VkDevice ya destruido en Vulkan).
            m_scene_renderer.reset();
            m_material_manager.reset();
            m_descriptor_set.reset();
            m_pipeline.reset();
            m_fragment_shader.reset();
            m_vertex_shader.reset();
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

        // Explicit cleanup in reverse dependency order.
        if (m_vertex_buffer.is_valid())   m_device->destroy_buffer(m_vertex_buffer);
        if (m_constant_buffer.is_valid()) m_device->destroy_buffer(m_constant_buffer);

        LOG_INFO(k_category, "RenderingModule desconectado.");
    }

    // Ventana embebida (modo 2) -----------------------------------------------------------------------
    bool RenderingModule::attach_window(void* native_window_handle, rhi::Extent2D extent) {
        if (!m_device) {
            LOG_WARNING(k_category, "attach_window: el dispositivo todavía no está inicializado.");
            return false;
        }
        if (!native_window_handle) {
            LOG_WARNING(k_category, "attach_window: native_window_handle es nulo.");
            return false;
        }

        std::lock_guard lock(m_swapchain_mutex);

        rhi::SwapchainDesc sc_desc;
        sc_desc.surface_type         = current_platform_surface_type();
        sc_desc.native_window_handle = native_window_handle;
        sc_desc.extent               = extent;
        sc_desc.image_count          = 2;
        sc_desc.vsync                = true;

        m_swapchain = m_device->create_swapchain(sc_desc);
        if (!m_swapchain) {
            LOG_ERROR(k_category, "attach_window: no se pudo crear el swapchain para la ventana externa.");
            return false;
        }

        LOGF_INFO(k_category, "Ventana externa adjuntada ({}×{}).", extent.width, extent.height);
        return true;
    }

    void RenderingModule::resize(rhi::Extent2D new_extent) {
        std::lock_guard lock(m_swapchain_mutex);
        if (m_swapchain) m_swapchain->resize(new_extent);
    }

    void RenderingModule::detach_window() {
        std::lock_guard lock(m_swapchain_mutex);
        if (!m_swapchain) return;

        if (m_device) m_device->wait_idle();
        m_swapchain.reset();

        LOG_INFO(k_category, "Ventana externa desadjuntada.");
    }
} // namespace anxiety::rendering
