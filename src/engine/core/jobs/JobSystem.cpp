#include "JobSystem.h"
#include "Logger.h"

#include <algorithm>
#include <chrono>

namespace anxiety::jobs {
    // Índice, local al hilo, del worker en ejecución. -1 significa "no es un hilo worker" (es decir,
    // el hilo principal o cualquier otro hilo externo que llame a wait/waitIdle).
    thread_local int t_worker_index = -1;

    // Centinela: valor de t_worker_index convertido a uint32_t cuando vale -1.
    static constexpr uint32_t k_no_worker = ~0u;

    static constexpr std::string_view k_category = "JobSystem";

    // Constructor / Destructor -------------------------------------------------------------------
    JobSystem::JobSystem(uint32_t thread_count) {
        const uint32_t n = (thread_count == 0) ? std::max(1u, std::thread::hardware_concurrency() - 1u) : thread_count;

        // Construye los deques por worker antes de lanzarlos (los workers indexan sobre este vector).
        m_deques.reserve(n);
        for (uint32_t i = 0; i < n; ++i) {
            m_deques.emplace_back(std::make_unique<WorkStealingDeque<JobHandle>>());
        }
        m_workers.reserve(n);
        for (uint32_t i = 0; i < n; ++i) {
            m_workers.emplace_back([this, i]() { worker_loop(i); });
        }
    }
    JobSystem::~JobSystem() { stop(); }

    // IModule ------------------------------------------------------------------------------------
    bool JobSystem::on_init(anxiety::Engine& /*engine*/) {
        // Los workers ya están corriendo desde el constructor.
        LOGF_INFO(k_category, "Job system en línea ({} hilo(s) worker).", static_cast<uint32_t>(m_workers.size()));
        return true;
    }

    void JobSystem::on_shutdown() {
        stop();
        LOG_INFO(k_category, "Job system desconectado.");
    }

    // API pública ---------------------------------------------------------------------------------
    JobHandle JobSystem::submit(std::function<void()> fn, std::vector<JobHandle> deps) {
        auto desc = std::make_shared<JobDesc>();
        desc->fn = std::move(fn);

        // deps_remaining empieza en deps.size()+1. El +1 es una "guarda" que evita que el trabajo se
        // encole antes de terminar de registrar todas las continuaciones — incluso si cada
        // dependencia se completa entre una iteración y otra.
        desc->deps_remaining.store(static_cast<int>(deps.size()) + 1, std::memory_order_relaxed);

        m_pending_jobs.fetch_add(1, std::memory_order_relaxed);

        // Se registra como continuación de cada dependencia, o decrementa de inmediato si esa
        // dependencia ya ha terminado. Ambas ramas están protegidas por dep->mu, así que son
        // atómicas respecto a on_job_finished().
        for (const auto& dep : deps) {
            std::lock_guard lock(dep->mu);
            if (dep->done.load(std::memory_order_relaxed)) {
                // La dependencia ya está hecha — satisface nuestro contador ahora.
                desc->deps_remaining.fetch_sub(1, std::memory_order_acq_rel);
            } else {
                dep->continuations.push_back(desc);
            }
        }

        // Libera la guarda +1. Si el resultado es 0, todas las dependencias ya estaban hechas.
        if (desc->deps_remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            enqueue_ready(desc);
        }

        return desc;
    }

    void JobSystem::wait(const JobHandle& h) {
        while (!h->done.load(std::memory_order_acquire)) {
            if (!try_execute_one()) std::this_thread::yield();
        }
    }

    void JobSystem::wait_idle() {
        while (m_pending_jobs.load(std::memory_order_acquire) > 0) {
            if (!try_execute_one()) std::this_thread::yield();
        }
    }

    // Privado ------------------------------------------------------------------------------------
    void JobSystem::stop() {
        // exchange() devuelve el valor anterior; si ya era true, stop() ya se había llamado antes —
        // se omite para evitar un doble join.
        if (m_stopping.exchange(true)) return;

        m_cv.notify_all();     // despierta a los workers dormidos para que puedan salir

        for (auto& t : m_workers) {
            if (t.joinable()) t.join();
        }
        m_workers.clear();
        // NO se vacía m_deques aquí — try_execute_one() todavía puede llamarse desde el hilo
        // principal después de stop() (p. ej. desde una llamada a wait() pendiente).
    }

    void JobSystem::enqueue_ready(JobHandle h) {
        // Si estamos corriendo dentro de un worker, se empuja al deque de ese propio worker para una
        // ejecución local a la caché. Si no, se empuja a la cola de inyección global.
        const int idx = t_worker_index;
        if (idx >= 0 && static_cast<uint32_t>(idx) < m_deques.size()) {
            m_deques[static_cast<uint32_t>(idx)]->push_back(std::move(h));
        } else {
            std::lock_guard lock(m_global_mu);
            m_global_queue.push(std::move(h));
        }
        // Siempre notifica a un esperador — un worker dormido en m_cv se despertará, intentará
        // try_execute_one(), y o bien procesará el trabajo o lo robará de un deque.
        m_cv.notify_one();
    }

    bool JobSystem::try_execute_one() {
        const uint32_t myIdx = (t_worker_index >= 0) ? static_cast<uint32_t>(t_worker_index) : k_no_worker;

        JobHandle job;

        // 1. Deque propio — LIFO (lo más recientemente empujado, caliente en caché).
        if (myIdx < m_deques.size() && m_deques[myIdx]->pop_back(job)) {
            run_job(std::move(job));
            return true;
        }

        // 2. Cola de inyección global.
        {
            std::lock_guard lock(m_global_mu);
            if (!m_global_queue.empty()) {
                job = std::move(m_global_queue.front());
                m_global_queue.pop();
            }
        }

        if (job) {
            run_job(std::move(job));
            return true;
        }

        // 3. Robar de otros workers — FIFO (el trabajo más antiguo primero).
        const uint32_t n = static_cast<uint32_t>(m_deques.size());
        for (uint32_t i = 0; i < n; ++i) {
            if (i == myIdx) continue;
            if (m_deques[i]->steal(job)) {
                run_job(std::move(job));
                return true;
            }
        }

        return false;
    }

    void JobSystem::run_job(JobHandle h) {
        if (h->fn) h->fn();
        on_job_finished(h);
    }

    void JobSystem::on_job_finished(const JobHandle& h) {
        // Bajo h->mu: marca como hecho y recoge las continuaciones de forma atómica, para que un
        // submit() concurrente vea un estado consistente (o bien done==true, o nuestra continuación
        // ya registrada antes de que la saquemos de ahí).
        std::vector<JobHandle> conts;

        {
            std::lock_guard lock(h->mu);
            h->done.store(true, std::memory_order_release);
            conts = std::move(h->continuations);
        }

        // Decrementa el contador global de pendientes por este trabajo completado.
        m_pending_jobs.fetch_sub(1, std::memory_order_acq_rel);

        // Satisface cada continuación. Cuando su contador llega a 0, está lista.
        for (auto& cont : conts) {
            if (cont->deps_remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) enqueue_ready(std::move(cont));
        }
    }

    void JobSystem::worker_loop(uint32_t idx) {
        t_worker_index = static_cast<int>(idx);

        // Sigue corriendo mientras:
        //   • no se esté deteniendo todavía, O
        //   • se esté deteniendo pero aún queden trabajos pendientes por vaciar.
        while (!m_stopping.load(std::memory_order_relaxed) || m_pending_jobs.load(std::memory_order_relaxed) > 0) {
            if (try_execute_one()) continue;

            // No hay nada que hacer — espera a que llegue trabajo nuevo o una señal de parada.
            // Se usa wait_for para que los workers vuelvan a comprobar periódicamente sus propios
            // deques, que pueden haberse poblado por robo (ahí no hay notificación por cv).
            std::unique_lock lock(m_global_mu);
            m_cv.wait_for(lock, std::chrono::microseconds(500), [this] { return !m_global_queue.empty() || (m_stopping.load() && m_pending_jobs.load() == 0); });
        }
    }
} // namespace anxiety::jobs
