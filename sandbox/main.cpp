#include "Engine.h"
#include "IModule.h"
#include "Logger.h"
#include "PlatformModule.h"
#include "RenderingModule.h"
#include "World.h"

#include "scene/SceneComponents.h"
#include "scene/SceneRenderer.h"
#include "textures/TextureManager.h"

using namespace anxiety;

static constexpr std::string_view k_category = "sandbox";

// Módulos de ejemplo -----------------------------------------------------------------------------

// AutoStopModule ---------------------------------------------------------------------------------
// Solicita el apagado del motor tras 'max_seconds' segundos. Demuestra el concepto de un módulo.
// ------------------------------------------------------------------------------------------------
class AutoStopModule final : public IModule {
public:
	explicit AutoStopModule(double max_seconds) : m_max_seconds(max_seconds) {}

	std::string_view name() const noexcept override { return "AutoStop"; }

	bool on_init(Engine& engine) override {
		LOG_INFO(k_category, "iniciandose...");
		LOGF_INFO(k_category, "Se solicitará la parada tras {} segundos.", m_max_seconds);

		m_engine = &engine;
		return true;
	}

	void on_update(float dt) override {
		m_elapsed += static_cast<double>(dt);

		if (m_elapsed >= m_max_seconds) {
			LOGF_INFO(k_category, "Se alcanzaron {} segundos - solicitando parada.", m_max_seconds);
			m_engine->request_stop();
		}
	}

	void on_shutdown() override { LOG_INFO(k_category, "apagado..."); }

private:
	static constexpr std::string_view k_category = "AutoStop";

	double  m_max_seconds{ 5.0 };
	Engine* m_engine{ nullptr };
	double  m_elapsed{ 0 };
};

// Demo de escena — con ventana, D3D12, cámara + una entidad malla con triángulo RGB --------------
// Se abre una ventana Win32. Una entidad cámara mira al origen desde Z=-3. Una entidad malla se
// sitúa en el origen con un triángulo coloreado. El SceneRenderer la dibuja vía ScenePass durante
// 60 fotogramas y luego cierra.
// ------------------------------------------------------------------------------------------------
[[maybe_unused]] static void run_scene_demo() {
	LOG_INFO("Scene", "--- Demo de SceneRenderer ---");

	anxiety::platform::PlatformModule::Config pal_cfg;
	pal_cfg.window.title     = "IAEngine — Scene Renderer";
	pal_cfg.window.width     = 1280;
	pal_cfg.window.height    = 720;
	pal_cfg.window.resizable = true;

	anxiety::EngineConfig cfg{
		.app_name   = "Scene Demo",
		.target_fps = 60,
		.headless   = false,
	};

	anxiety::Engine eng(cfg);
	auto& plat_mod   = eng.emplace_module<anxiety::platform::PlatformModule>(pal_cfg);
	rendering::RenderingModule::Config mod_cfg;
	// mod_cfg.preferred_backend = rendering::rhi::RHIBackend::OpenGL;
	auto& render_mod = eng.emplace_module<anxiety::rendering::RenderingModule>(plat_mod, mod_cfg);
	eng.emplace_module<AutoStopModule>(60u);

	if (!eng.init()) {
		LOG_FATAL("Scene", "La demo de escena no pudo inicializarse.");
		return;
	}

	// Construye el world de ECS: cámara + una entidad malla.
	anxiety::ecs::World world;

	namespace sc  = anxiety::rendering::scene;
	namespace mat = anxiety::rendering::materials;
	namespace tex = anxiety::rendering::textures;

	// Entidad cámara -------------------------------------------------------------------------------
	{
		auto camEnt = world.create_entity();
		sc::Transform camT{};
		camT.position[2] = -4.0f;   // ojo en (0, 0, -4), mirando hacia +Z
		world.add_component<sc::Transform>(camEnt, camT);
		world.add_component<sc::Camera>(camEnt, sc::Camera{ 1.0472f, 0.1f, 1000.0f, 16.0f / 9.0f });
	}

    // Geometría del quad (36 bytes/vértice: POSITION float3, COLOR float4, UV float2) ----------
    struct QuadV { float x, y, z, r, g, b, a, u, v; };

    // Quad unitario centrado en el origen, abarcando [-0.5, 0.5] en XY.
    static constexpr QuadV k_quad_verts[] = {
        { -0.5f,  0.5f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f },        // arriba-izquierda
        {  0.5f,  0.5f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f },        // arriba-derecha
        {  0.5f, -0.5f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f },        // abajo-derecha
        { -0.5f, -0.5f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f },        // abajo-izquierda
    };
    static constexpr uint32_t k_quad_idx[] = { 0,1,2,  0,2,3 };             // 6 índices

    auto* sr  = render_mod.scene_renderer();
    auto* mgr = render_mod.material_manager();
    auto* tmg = render_mod.texture_manager();

    // Quad izquierdo — unlit, tinte rojo (sin textura) --------------------------------------------
    if (sr && mgr) {
        mat::MaterialHandle unlit_mat = mgr->default_unlit();
        mat::MaterialInstanceHandle red_inst;
        if (unlit_mat.is_valid()) {
            red_inst = mgr->create_instance(unlit_mat);
            mgr->set_base_color(red_inst, 1.0f, 0.3f, 0.3f, 1.0f);
        }

        auto h   = sr->upload_mesh(k_quad_verts, sizeof(k_quad_verts), k_quad_idx, sizeof(k_quad_idx));
        auto ent = world.create_entity();
        sc::Transform t{};
        t.position[0] = -1.0f;
        world.add_component<sc::Transform>(ent, t);
        sc::MeshRenderer mr{};
        mr.vertex_buffer     = h.vertex_buffer;
        mr.index_buffer      = h.index_buffer;
        mr.index_count       = 6;
        mr.vertex_stride     = sizeof(QuadV);
        mr.material_instance = red_inst;
        world.add_component<sc::MeshRenderer>(ent, mr);
    }

    // Quad derecho — unlit_textured, textura cargada desde fichero -------------------------------
    if (sr && mgr && tmg) {
        mat::MaterialHandle tex_mat = mgr->default_unlit_textured();
        mat::MaterialInstanceHandle texInst;
        anxiety::rendering::rhi::TextureHandle checker_tex;

        if (tex_mat.is_valid()) {
            texInst = mgr->create_instance(tex_mat);

            // Damero RGBA8 2×2: cian arriba-izquierda/abajo-derecha, magenta en el resto.
            constexpr uint8_t C = 255, Z = 0;
            const uint8_t k_pixels[2 * 2 * 4] = {
                C, Z, C, C,   Z, C, C, C,                       // fila 0: cian, magenta
                Z, C, C, C,   C, Z, C, C,                       // fila 1: magenta, cian
            };
            //checker_tex = tmg->load_from_memory(k_pixels, 2, 2, "CheckerTex");
            checker_tex = tmg->load("textures/roca.jpg");
            if (checker_tex.is_valid()) mgr->set_albedo_texture(texInst, checker_tex);
        }

        auto h = sr->upload_mesh(k_quad_verts, sizeof(k_quad_verts), k_quad_idx, sizeof(k_quad_idx));
        auto ent = world.create_entity();
        sc::Transform t{};
        t.position[0] = 1.0f;
        world.add_component<sc::Transform>(ent, t);
        sc::MeshRenderer mr{};
        mr.vertex_buffer     = h.vertex_buffer;
        mr.index_buffer      = h.index_buffer;
        mr.index_count       = 6;
        mr.vertex_stride     = sizeof(QuadV);
        mr.material_instance = texInst;
        world.add_component<sc::MeshRenderer>(ent, mr);
    }

    render_mod.set_world(&world);
    eng.run();
    render_mod.set_world(nullptr);
}

// main ===========================================================================================
int main() {
	logs::Logger::get().set_level(logs::LogLevel::Trace);
	LOG_INFO(k_category, "=== Ejemplo de Anxiety -> ECS + Jobs + PAL ===");

	run_scene_demo();

	LOG_INFO(k_category, "=== Cerrando la aplicación de ejemplo ===");
	return 0;
}
