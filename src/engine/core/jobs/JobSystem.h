#pragma once

#include "Job.h"
#include "WorkStealingDeque.h"
#include "IModule.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace anxiety::jobs {
    using Job = std::function<void()>;

    // JobSystem ----------------------------------------------------------------------------------
    // Job system work-stealing de alto rendimiento, implementado como un IModule.
    //
    // Arquitectura
    // -------------
    //   • N-1 hilos worker persistentes (por defecto: hardware_concurrency()-1).
    //   • Un WorkStealingDeque por worker: extracción local LIFO, robo FIFO.
    //   • Cola de inyección global para envíos desde hilos que no son workers.
    //   • Robo de trabajo: los workers inactivos extraen de los deques de sus compañeros antes de
    //     dormirse.
    //   • Sistema de dependencias: contador atómico por trabajo; el trabajo pasa a estar listo
    //     cuando todas las dependencias listadas se han completado.
    //   • Participación del hilo principal: wait() y waitIdle() ejecutan trabajos pendientes en el
    //     hilo que llama en vez de quedarse inactivos dando vueltas.
    //
    // Uso como IModule
    // -----------------
    //   engine.emplace_module<JobSystem>();			// on_init arranca los workers
    //												// on_shutdown los une
    //
    // Uso independiente (p. ej. tests)
    // ------------------------------
    //   JobSystem js(4);							// 4 workers, arrancan de inmediato
    //   auto h = js.submit([]{});
    //   js.wait(h);								// el hilo principal ayuda; bloquea hasta terminar
    // --------------------------------------------------------------------------------------------
    class JobSystem final : public IModule {
    public:
        // thread_count == 0  →  std::thread::hardware_concurrency() - 1 (mínimo 1). Los hilos worker
        // arrancan de inmediato en el constructor.
        explicit JobSystem(uint32_t thread_count = 0);
        ~JobSystem();

        // IModule --------------------------------------------------------------------------------
        [[nodiscard]] std::string_view name()                           const noexcept override { return "JobSystem"; }
        [[nodiscard]] bool             on_init(anxiety::Engine& engine)                override;
        void                           on_update(float /*dt*/)                         override {}
        void                           on_shutdown()                                   override;

        // API de trabajos --------------------------------------------------------------------------

        // Envía fn para que se ejecute en cuanto cada dependencia en 'deps' se haya completado.
        // Devuelve un handle utilizable para más dependencias o para esperar.
        [[nodiscard]] JobHandle submit(std::function<void()> fn, std::vector<JobHandle> deps = {});
        // Bloquea el hilo que llama hasta que h termine. Ese hilo ejecuta trabajos pendientes
        // mientras espera (sin desperdiciar el tiempo dando vueltas).
        void                    wait(const JobHandle& h);
        // Bloquea hasta que cada trabajo enviado previamente (incluidos los que esperan
        // dependencias) se haya completado. El hilo que llama participa.
        void                    wait_idle();

        // Ejecuta fn(i) para i en [begin, end) repartido entre el pool de hilos. Bloquea hasta que
        // todas las iteraciones terminen. El hilo que llama participa.
        template<typename Fn>
        void parallel_for(int begin, int end, Fn&& fn) {
            if (begin >= end) return;

            int total = end - begin;
            // +1 para que el hilo que llama también procese un fragmento
            int workers = static_cast<int>(m_workers.size()) + 1;
            int chunk = std::max(1, (total + workers - 1) / workers);

            std::vector<JobHandle> handles;
            handles.reserve(static_cast<size_t>((total + chunk - 1) / chunk));

            for (int i = begin; i < end; i += chunk) {
                const int lo = i;
                const int hi = std::min(i + chunk, end);
                handles.push_back(submit([lo, hi, fn]() mutable { for (int j = lo; j < hi; ++j) fn(j); }));
            }

            for (const auto& h : handles) wait(h);
        }

        [[nodiscard]] uint32_t thread_count() const noexcept { return static_cast<uint32_t>(m_workers.size()); }

    private:
        void worker_loop(uint32_t idx);
        // Intenta ejecutar un trabajo (deque propio → cola global → robo). Devuelve true si se
        // ejecutó un trabajo.
        bool try_execute_one();
        // Ejecuta h->fn() y dispara las notificaciones de dependencia.
        void run_job(JobHandle h);
        // Se llama tras terminar run_job: marca como hecho, satisface las continuaciones.
        void on_job_finished(const JobHandle& h);
        // Empuja un trabajo listo a la cola correspondiente y notifica a un worker.
        void enqueue_ready(JobHandle h);
        // Vacía los trabajos restantes y une todos los hilos worker (idempotente).
        void stop();

        // Deques por worker — extracción local LIFO, robo FIFO.
        // Se guardan como unique_ptr porque std::mutex no es movible/copiable, lo que impide
        // almacenar WorkStealingDeque directamente en un std::vector.
        std::vector<std::unique_ptr<WorkStealingDeque<JobHandle>>> m_deques;

        // Cola de inyección global — usada por hilos que no son workers.
        std::queue<JobHandle>                                      m_global_queue;
        std::mutex                                                 m_global_mu;
        std::condition_variable                                    m_cv;

        std::vector<std::thread>                                   m_workers;
        std::atomic<bool>                                          m_stopping{ false };

        // Recuento de trabajos enviados pero no terminados todavía (incluidos los bloqueados por dependencias).
        std::atomic<int>                                           m_pending_jobs{ 0 };
    };
} // namespace anxiety::jobs
