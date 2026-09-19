#include "Engine.h"
#include "IModule.h"
#include "Logger.h"
#include "PlatformModule.h"
#include "RenderingModule.h"
#include "World.h"

#include "scene/SceneComponents.h"
#include "scene/SceneRenderer.h"

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

	anxiety::platform::PlatformModule::Config palCfg;
	palCfg.window.title = "IAEngine — Scene Renderer";
	palCfg.window.width = 1280;
	palCfg.window.height = 720;
	palCfg.window.resizable = true;

	anxiety::EngineConfig cfg{
		.app_name = "Scene Demo",
		.target_fps = 60,
		.headless = false,
	};

	anxiety::Engine eng(cfg);
	auto& platMod   = eng.emplace_module<anxiety::platform::PlatformModule>(palCfg);
	rendering::RenderingModule::Config mod_cfg;
	//mod_cfg.preferred_backend = rendering::rhi::RHIBackend::Vulkan;
	auto& renderMod = eng.emplace_module<anxiety::rendering::RenderingModule>(platMod, mod_cfg);
	eng.emplace_module<AutoStopModule>(60u);

	if (!eng.init()) {
		LOG_FATAL("Scene", "La demo de escena no pudo inicializarse.");
		return;
	}

	// Construye el world de ECS: cámara + una entidad malla.
	anxiety::ecs::World world;

	namespace sc = anxiety::rendering::scene;

	// Entidad cámara -------------------------------------------------------------------------------
	{
		auto camEnt = world.create_entity();
		sc::Transform camT{};
		camT.position[2] = -3.f;   // ojo en (0, 0, -3), mirando hacia +Z
		world.add_component<sc::Transform>(camEnt, camT);
		world.add_component<sc::Camera>(camEnt, sc::Camera{ 1.0472f, 0.1f, 1000.f, 16.f / 9.f });
	}

	// Entidad malla --------------------------------------------------------------------------------
	{
		struct SceneV { float x, y, z, r, g, b, a; };
		static constexpr SceneV kVerts[] = {
			{  0.f,  0.5f, 0.f, 1.f, 0.f, 0.f, 1.f },           // arriba   — rojo
			{  0.5f,-0.5f, 0.f, 0.f, 1.f, 0.f, 1.f },           // derecha  — verde
			{ -0.5f,-0.5f, 0.f, 0.f, 0.f, 1.f, 1.f },           // izquierda— azul
		};
		static constexpr uint32_t kIndices[] = { 0, 1, 2 };

		auto meshEnt = world.create_entity();
		sc::Transform meshT{};                                  // en el origen
		world.add_component<sc::Transform>(meshEnt, meshT);

		auto* sr = renderMod.scene_renderer();
		if (sr) {
			auto h = sr->upload_mesh(kVerts, sizeof(kVerts), kIndices, sizeof(kIndices));
			sc::MeshRenderer mr{};
			mr.vertex_buffer = h.vertex_buffer;
			mr.index_buffer = h.index_buffer;
			mr.index_count = 3;
			mr.vertex_stride = sizeof(SceneV);
			world.add_component<sc::MeshRenderer>(meshEnt, mr);
		}
	}

	renderMod.set_world(&world);

	eng.run();                                                  // bloquea; la escena se renderiza cada fotograma

	renderMod.set_world(nullptr);                               // desadjuntar antes de que world salga de ámbito
}

// main ===========================================================================================
int main() {
	logs::Logger::get().set_level(logs::LogLevel::Trace);
	LOG_INFO(k_category, "=== Ejemplo de Anxiety -> ECS + Jobs + PAL ===");

	run_scene_demo();

	LOG_INFO(k_category, "=== Cerrando la aplicación de ejemplo ===");
	return 0;
}
