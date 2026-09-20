#include <catch2/catch_test_macros.hpp>

#include "JobSystem.h"

#include <atomic>
#include <chrono>
#include <set>
#include <thread>
#include <vector>

using anxiety::jobs::JobHandle;
using anxiety::jobs::JobSystem;

// API básica --------------------------------------------------------------------------------------
TEST_CASE("jobs_module_name", "[jobs]") {
    JobSystem js(2);
    REQUIRE(js.name() == std::string_view{ "JobSystem" });
}

TEST_CASE("jobs_thread_count", "[jobs]") {
    anxiety::jobs::JobSystem js(4);
    REQUIRE(js.thread_count() == 4u);
}

TEST_CASE("jobs_thread_count_default", "[jobs]") {
    JobSystem js;
    REQUIRE(js.thread_count() >= 1u);
}

// Submit + wait ----------------------------------------------------------------------------------
TEST_CASE("jobs_submit_executes", "[jobs]") {
    JobSystem js(2);
    std::atomic<bool> ran{ false };

    auto h = js.submit([&ran]() { ran.store(true, std::memory_order_release); });
    js.wait(h);

    REQUIRE(ran.load(std::memory_order_acquire));
}

TEST_CASE("jobs_submit_multiple", "[jobs]") {
    JobSystem js(4);
    std::atomic<int> counter{ 0 };

    std::vector<JobHandle> handles;
    handles.reserve(100);
    for (int i = 0; i < 100; ++i) {
        handles.push_back(js.submit([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); }));
    }
    for (const auto& h : handles) js.wait(h);

    REQUIRE(counter.load() == 100);
}

TEST_CASE("jobs_wait_idle", "[jobs]") {
    JobSystem js(2);
    std::atomic<int> done{ 0 };

    // Mantiene los handles vivos para que pendingJobs siga sin ser cero hasta que todos los trabajos
    // corran; waitIdle() debe bloquear hasta que cada trabajo termine.
    std::vector<JobHandle> handles;
    handles.reserve(10);
    for (int i = 0; i < 10; ++i) {
        handles.push_back(js.submit([&done]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            done.fetch_add(1, std::memory_order_relaxed);
            }));
    }
    js.wait_idle();

    REQUIRE(done.load() == 10);
}

// Sistema de dependencias ------------------------------------------------------------------------

TEST_CASE("jobs_dependency_ordering", "[jobs]") {
    // B depende de A — B no debe empezar hasta que A haya terminado.
    JobSystem js(4);

    std::atomic<int> seq{ 0 };
    int aSeq = -1, bSeq = -1;

    auto a = js.submit([&seq, &aSeq]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        aSeq = seq.fetch_add(1, std::memory_order_acq_rel);
        });

    auto b = js.submit([&seq, &bSeq]() { bSeq = seq.fetch_add(1, std::memory_order_acq_rel); }, { a });
    js.wait(b);

    REQUIRE(aSeq == 0);
    REQUIRE(bSeq == 1);
}

TEST_CASE("jobs_dependency_already_done", "[jobs]") {
    // Enviar un trabajo cuya dependencia ya está completa debe seguir ejecutándolo.
    JobSystem js(2);

    auto a = js.submit([]() {});
    js.wait(a);   // a está terminado con toda seguridad

    std::atomic<bool> ran{ false };
    auto b = js.submit([&ran]() { ran = true; }, { a });
    js.wait(b);

    REQUIRE(ran.load());
}

TEST_CASE("jobs_diamond_dependency", "[jobs]") {
    //   A
    //  / \
    // B   C
    //  \ /
    //   D   (D depende de B y C; B y C dependen ambos de A)
    JobSystem js(4);

    std::atomic<int> seq{ 0 };
    int aOrder = -1, bOrder = -1, cOrder = -1, dOrder = -1;

    auto a = js.submit([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        aOrder = seq.fetch_add(1, std::memory_order_acq_rel);
        });

    auto b = js.submit([&] { bOrder = seq.fetch_add(1, std::memory_order_acq_rel); }, { a });
    auto c = js.submit([&] { cOrder = seq.fetch_add(1, std::memory_order_acq_rel); }, { a });
    auto d = js.submit([&] { dOrder = seq.fetch_add(1, std::memory_order_acq_rel); }, { b, c });

    js.wait(d);

    // A debe ser el primero; D debe ser el último.
    REQUIRE(aOrder == 0);
    REQUIRE(dOrder == 3);
    // B y C ocupan cada uno el hueco 1 o 2 (el orden entre ellos no está especificado).
    REQUIRE((bOrder == 1 || bOrder == 2));
    REQUIRE((cOrder == 1 || cOrder == 2));
}

// parallel_for -----------------------------------------------------------------------------------

TEST_CASE("jobs_parallel_for_all_indices", "[jobs]") {
    JobSystem js(4);

    constexpr int N = 1000;
    std::vector<std::atomic<int>> visited(N);
    for (auto& v : visited) v.store(0);

    js.parallel_for(0, N, [&visited](int i) {
        visited[i].fetch_add(1, std::memory_order_relaxed);
        });

    for (int i = 0; i < N; ++i) {
        REQUIRE(visited[i].load() == 1);
    }
}

TEST_CASE("jobs_parallel_for_empty_range", "[jobs]") {
    // Debe ser un no-op — sin caída, sin bloqueo.
    JobSystem js(2);
    js.parallel_for(5, 5, [](int /*i*/) {});
    js.parallel_for(10, 5, [](int /*i*/) {});
}

TEST_CASE("jobs_parallel_for_sum", "[jobs]") {
    JobSystem js(4);

    std::atomic<long long> sum{ 0 };
    js.parallel_for(0, 100, [&sum](int i) { sum.fetch_add(i, std::memory_order_relaxed); });

    // Suma 0..99 = 4950
    REQUIRE(sum.load() == 4950LL);
}

// Verificación de paralelismo -----------------------------------------------------------------------

TEST_CASE("jobs_parallelism", "[jobs]") {
    // Envía N trabajos que duermen brevemente — todos deben correr concurrentemente en workers
    // distintos, produciendo N ids de hilo diferentes.
    constexpr int N = 4;
    JobSystem js(N);

    std::vector<std::thread::id> ids(N);
    std::vector<JobHandle> handles;
    handles.reserve(N);

    for (int i = 0; i < N; ++i) {
        handles.push_back(js.submit([i, &ids]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            ids[i] = std::this_thread::get_id();
            }));
    }
    for (const auto& h : handles) js.wait(h);

    std::set<std::thread::id> unique(ids.begin(), ids.end());
    REQUIRE(unique.size() > 1);
}

// Participación del hilo principal durante wait() --------------------------------------------------

TEST_CASE("jobs_main_thread_participation", "[jobs]") {
    // Usa un único worker. Lo bloquea. Después envía un segundo trabajo y llama a wait() en el hilo
    // principal. Con un solo worker ocupado, el hilo principal debe robar y ejecutar el segundo
    // trabajo para que wait() pueda retornar.
    JobSystem js(1);

    std::atomic<bool> workerBlocked{ false };
    std::atomic<bool> releaseWorker{ false };

    // Este trabajo ocupa el único worker.
    auto blocker = js.submit([&] {
        workerBlocked.store(true, std::memory_order_release);
        while (!releaseWorker.load(std::memory_order_acquire))
            std::this_thread::yield();
        });

    // Espera hasta que el worker esté dando vueltas dentro del trabajo bloqueador.
    while (!workerBlocked.load(std::memory_order_acquire))
        std::this_thread::yield();

    // Ahora envía un segundo trabajo. El worker está ocupado, así que wait() debe ejecutarlo en el
    // hilo que llama (el principal).
    std::thread::id executorId;
    auto second = js.submit([&executorId]() {
        executorId = std::this_thread::get_id();
        });

    js.wait(second);   // el hilo principal debe ejecutar 'second' aquí

    REQUIRE(executorId == std::this_thread::get_id());

    releaseWorker.store(true, std::memory_order_release);
    js.wait(blocker);
}
