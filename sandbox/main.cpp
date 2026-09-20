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


    // Cámara — en (0,1,-5) mirando hacia el origen ------------------------------------------------
    {
        auto cam_ent = world.create_entity();
        sc::Transform cam_T{};
        cam_T.position[0] =  0.0f;
        cam_T.position[1] =  1.0f;
        cam_T.position[2] = -5.f;
        // Mira a lo largo de +Z: el cuaternión "adelante" por defecto es la identidad {0,0,0,1}
        world.add_component<sc::Transform>(cam_ent, cam_T);
        world.add_component<sc::Camera>(cam_ent, sc::Camera{ 1.0472f, 0.1f, 500.f, 16.f / 9.f });
    }

    // Luz direccional — sol cálido, arriba a la derecha -------------------------------------------
    {
        auto dl_ent = world.create_entity();
        world.add_component<sc::Transform>(dl_ent, sc::Transform{});

        sc::DirectionalLight dl{};
        // Dirección en la que viaja la luz (normalizada, hacia abajo-izquierda-adelante)
        const float s = 1.0f / std::sqrt(3.0f);
        dl.direction[0] = -s;                           // izquierda
        dl.direction[1] = -s;                           // abajo
        dl.direction[2] =  s;                           // adelante
        dl.intensity    = 2.5f;
        dl.color[0]     = 1.00f;
        dl.color[1]     = 0.95f;
        dl.color[2]     = 0.80f;                        // blanco cálido
        world.add_component<sc::DirectionalLight>(dl_ent, dl);
    }

    // Luz puntual 1 — azul frío, lado izquierdo ---------------------------------------------------
    {
        auto pl_ent = world.create_entity();
        sc::Transform t{};
        t.position[0] = -3.0f;
        t.position[1] =  2.0f;
        t.position[2] =  0.0f;
        world.add_component<sc::Transform>(pl_ent, t);

        sc::PointLight pl{};
        pl.color[0]  = 0.3f;
        pl.color[1]  = 0.6f;
        pl.color[2]  = 1.0f;                            // azul frío
        pl.intensity = 6.0f;
        pl.range     = 10.f;
        world.add_component<sc::PointLight>(pl_ent, pl);
    }

    // Luz puntual 2 — naranja cálido, lado derecho ------------------------------------------------
    {
        auto pl_ent = world.create_entity();
        sc::Transform t{};
        t.position[0] = 3.0f;
        t.position[1] = 2.0f;
        t.position[2] = 0.0f;
        world.add_component<sc::Transform>(pl_ent, t);

        sc::PointLight pl{};
        pl.color[0]  = 1.0f;
        pl.color[1]  = 0.4f;
        pl.color[2]  = 0.1f;                            // naranja cálido
        pl.intensity = 6.0f;
        pl.range     = 10.f;
        world.add_component<sc::PointLight>(pl_ent, pl);
    }

    // Geometría del quad PBR -----------------------------------------------------------------------
    // Quad plano en el plano XY en z=0. La normal mira hacia -Z (hacia la cámara). Tangente = +X,
    // signo de la bitangente = +1.
    struct PbrV {
        float px, py, pz;                               // POSITION
        float nx, ny, nz;                               // NORMAL
        float u, v;                                     // TEXCOORD
        float tx, ty, tz, tw;                           // TANGENT (w = bitangent sign)
    };
    static_assert(sizeof(PbrV) == 48);

    // Quad unitario 1×1 centrado en el origen, mirando hacia -Z.
    const PbrV k_quad_v[] = {
        { -0.5f,  0.5f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f },
        {  0.5f,  0.5f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f },
        {  0.5f, -0.5f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f },
        { -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f },
    };
    const uint32_t k_quad_idx[] = { 0,1,2, 0,2,3 };

    auto* sr  = render_mod.scene_renderer();
    auto* mgr = render_mod.material_manager();
    auto* tmg = render_mod.texture_manager();

    if (!sr || !mgr || !tmg || !mgr->default_PBR().is_valid()) {
        LOG_WARNING("PBR", "Material PBR no disponible — demo omitida.");
        render_mod.set_world(&world);
        eng.run();
        render_mod.set_world(nullptr);
        return;
    }

    // 3 columnas × 2 filas — varía metallic (columna) y roughness (fila).
    const float metallic_values[3]  = { 0.0f, 0.5f, 1.0f };
    const float roughness_values[2] = { 0.1f, 0.8f };

    const float spacing_X = 1.4f;
    const float spacing_Y = 1.4f;
    const float start_X   = -(spacing_X * (3 - 1)) * 0.5f;          // centra la rejilla
    const float start_Y   =  (spacing_Y * (2 - 1)) * 0.5f;
    
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 3; ++col) {
            auto h   = sr->upload_mesh(k_quad_v, sizeof(k_quad_v), k_quad_idx, sizeof(k_quad_idx));
            auto ent = world.create_entity();

            sc::Transform t{};
            t.position[0] = start_X + static_cast<float>(col) * spacing_X;
            t.position[1] = start_Y - static_cast<float>(row) * spacing_Y;
            t.position[2] = 0.0f;
            world.add_component<sc::Transform>(ent, t);

            auto tex = tmg->load("textures/roca.jpg");

            mat::MaterialInstanceHandle mi = mgr->create_instance(mgr->default_PBR());
            mgr->set_base_color(mi, 0.8f, 0.8f, 0.8f, 1.0f);        // albedo gris claro
            mgr->set_metallic(mi, metallic_values[col]);
            mgr->set_roughness(mi, roughness_values[row]);
            mgr->set_albedo_texture(mi, tex);

            sc::MeshRenderer mr{};
            mr.vertex_buffer     = h.vertex_buffer;
            mr.index_buffer      = h.index_buffer;
            mr.index_count       = 6;
            mr.vertex_stride     = sizeof(PbrV);
            mr.material_instance = mi;
            world.add_component<sc::MeshRenderer>(ent, mr);
        }
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
