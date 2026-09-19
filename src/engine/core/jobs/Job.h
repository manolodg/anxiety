#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace anxiety::jobs {
	struct JobDesc;

	// Un JobHandle es un ticket contador de referencia para un trabajo envíado. Usalo para expresar dependencias
	// (envía con deps=[...]) o para esperar que acaben (JobSystem::wait(h)).
	using JobHandle = std::shared_ptr<JobDesc>;

	// JobDesc ------------------------------------------------------------------------------------
	// Estado interno para una simple trabajo..
	// Los usuarios nunca tocan esto directamente; interactuan mediante JobHandle.
	// --------------------------------------------------------------------------------------------
	struct JobDesc {
		std::function<void()>  fn;

		// Comienza en deps.size() + 1 (+1 evita el encolado prematuro). Decrementado a 0 por cada
		// dependencia completada y el decremento final "liberado" en submit(). Cuando llega a 0 el trabajo está listo.
		std::atomic<int>       deps_remaining{ 1 };
		// A true por el ejecutor cuando fn() vuelve.
		std::atomic<bool>      done          { false };
		// Guarda 'done' y 'continuations' juntos de forma que submit() y on_job_finished() pueden ejecutarse con seguridad.
		std::mutex             mu;
		// Trabajos que están esperando para que este se complete.
		std::vector<JobHandle> continuations;
	};
} // namespace anxiety::jobs