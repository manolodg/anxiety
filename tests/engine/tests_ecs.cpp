#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "World.h"
#include "EcsModule.h"
#include "JobSystem.h"

// Tipos de componentes usados en los tests ---------------------------------------------------------
struct Position { float x, y, z; };
struct Health   { int hp; };
struct Velocity { float vx, vy; };
struct Tag { int id; };

// Parte 1 -----------------------------------------------------------------------------------------
// API legacy (debe seguir pasando con el backend basado en archetypes)
// ------------------------------------------------------------------------------------------------
TEST_CASE("ecs_create_entity", "[ecs]") {
    anxiety::ecs::World world;
    const auto id = world.create_entity();

    REQUIRE(id.is_valid());
    REQUIRE(world.entity_count() == 1u);
}

TEST_CASE("ecs_entity_alive", "[ecs]") {
    anxiety::ecs::World world;
    const auto id = world.create_entity();

    REQUIRE(world.is_alive(id));
    REQUIRE_FALSE(world.is_alive(anxiety::ecs::EntityId::null()));
}

TEST_CASE("ecs_add_and_get_component", "[ecs]") {
    anxiety::ecs::World world;
    const auto id = world.create_entity();
    world.add_component<Position>(id, { 1.0f, 2.0f, 3.0f });

    const Position* pos = world.get_component<Position>(id);

    REQUIRE(pos != nullptr);

    REQUIRE(pos->x == Catch::Approx(1.0f));
    REQUIRE(pos->y == Catch::Approx(2.0f));
    REQUIRE(pos->z == Catch::Approx(3.0f));
}

TEST_CASE("ecs_has_component", "[ecs]") {
    anxiety::ecs::World world;

    const auto id = world.create_entity();

    REQUIRE_FALSE(world.has_component<Position>(id));

    world.add_component<Position>(id, {});

    REQUIRE(world.has_component<Position>(id));
}

TEST_CASE("ecs_remove_component", "[ecs]") {
    anxiety::ecs::World world;

    const auto id = world.create_entity();

    world.add_component<Health>(id, { 100 });
    world.remove_component<Health>(id);

    REQUIRE_FALSE(world.has_component<Health>(id));
}

TEST_CASE("ecs_destroy_entity", "[ecs]") {
    anxiety::ecs::World world;

    const auto id = world.create_entity();

    world.add_component<Position>(id, { 0.f, 0.f, 0.f });

    world.destroy_entity(id);

    REQUIRE_FALSE(world.is_alive(id));
    REQUIRE(world.entity_count() == 0u);
}

TEST_CASE("ecs_stale_handle_after_destroy", "[ecs]") {
    anxiety::ecs::World world;

    const auto id = world.create_entity();

    world.destroy_entity(id);

    // Se recrea en el mismo índice → nueva generación
    const auto id2 = world.create_entity();

    REQUIRE(id2.is_valid());
    REQUIRE_FALSE(world.is_alive(id));
    REQUIRE(world.is_alive(id2));
}

TEST_CASE("ecs_multiple_components", "[ecs]") {
    anxiety::ecs::World world;

    const auto id = world.create_entity();

    world.add_component<Position>(id, { 10.f, 0.f, 0.f });
    world.add_component<Health>(id, { 50 });

    REQUIRE(world.has_component<Position>(id));
    REQUIRE(world.has_component<Health>(id));

    REQUIRE(world.get_component<Health>(id)->hp == 50);
}

// Parte 2 -----------------------------------------------------------------------------------------
// Comportamiento de los archetypes
// ------------------------------------------------------------------------------------------------

TEST_CASE("ecs_archetype_migration_on_add", "[ecs]") {
    // Añadir un segundo componente migra la entidad a un archetype distinto.
    // Todos los valores establecidos antes deben sobrevivir a la migración.
    anxiety::ecs::World world;
    const auto e = world.create_entity();
    world.add_component<Position>(e, { 1.f, 2.f, 3.f });
    world.add_component<Health>(e, { 42 });

    REQUIRE(world.has_component<Position>(e));
    REQUIRE(world.has_component<Health>(e));
    REQUIRE(world.get_component<Position>(e)->x == 1.0f);
    REQUIRE(world.get_component<Health>(e)->hp == 42);
}

TEST_CASE("ecs_archetype_migration_on_remove", "[ecs]") {
    // Quitar un componente migra la entidad de vuelta.
    anxiety::ecs::World world;
    const auto e = world.create_entity();
    world.add_component<Position>(e, { 5.f, 0.f, 0.f });
    world.add_component<Velocity>(e, { 1.f, 0.f });
    world.remove_component<Velocity>(e);

    REQUIRE(world.has_component<Position>(e));
    REQUIRE(!world.has_component<Velocity>(e));
    REQUIRE(world.get_component<Position>(e)->x == 5.0f);
}

TEST_CASE("ecs_swap_remove_correctness", "[ecs]") {
    // Quitar una entidad que no es la última no debe corromper las demás.
    anxiety::ecs::World world;

    auto e1 = world.create_entity();
    auto e2 = world.create_entity();
    auto e3 = world.create_entity();

    world.add_component<Health>(e1, { 10 });
    world.add_component<Health>(e2, { 20 });
    world.add_component<Health>(e3, { 30 });

    // Destruye la entidad del medio (dispara un swap-remove).
    world.destroy_entity(e2);

    REQUIRE(world.is_alive(e1));
    REQUIRE(!world.is_alive(e2));
    REQUIRE(world.is_alive(e3));
    REQUIRE(world.get_component<Health>(e1)->hp == 10);
    REQUIRE(world.get_component<Health>(e3)->hp == 30);
}

TEST_CASE("ecs_entities_in_correct_archetypes", "[ecs]") {
    anxiety::ecs::World world;

    auto e1 = world.create_entity();   // tendrá solo Position
    auto e2 = world.create_entity();   // tendrá solo Health
    auto e3 = world.create_entity();   // tendrá ambos

    world.add_component<Position>(e1, {});
    world.add_component<Health>(e2, {});
    world.add_component<Position>(e3, {});
    world.add_component<Health>(e3, {});

    REQUIRE(world.has_component<Position>(e1));
    REQUIRE(!world.has_component<Health>(e1));
    REQUIRE(!world.has_component<Position>(e2));
    REQUIRE(world.has_component<Health>(e2));
    REQUIRE(world.has_component<Position>(e3));
    REQUIRE(world.has_component<Health>(e3));
}

// Parte 3 -----------------------------------------------------------------------------------------
// Sistema de consultas (queries)
// ------------------------------------------------------------------------------------------------

TEST_CASE("ecs_query_empty", "[ecs]") {
    anxiety::ecs::World world;
    // Sin entidades — la consulta no debe devolver nada.
    int count = 0;
    world.query<Position>().for_each([&](Position&) { ++count; });
    REQUIRE(count == 0);
}

TEST_CASE("ecs_query_basic_foreach", "[ecs]") {
    anxiety::ecs::World world;
    const auto e = world.create_entity();
    world.add_component<Position>(e, { 7.f, 0.f, 0.f });

    int count = 0;
    float sumX = 0.f;
    world.query<Position>().for_each([&](Position& p) {
        sumX += p.x;
        ++count;
        });

    REQUIRE(count == 1);
    REQUIRE(sumX == 7.0f);
}

TEST_CASE("ecs_query_filters_by_component_set", "[ecs]") {
    // query<Position, Health> NO debe visitar a e2, que carece de Health.
    anxiety::ecs::World world;

    auto e1 = world.create_entity();
    auto e2 = world.create_entity();

    world.add_component<Position>(e1, { 1.f, 0.f, 0.f });
    world.add_component<Health>(e1, { 100 });
    world.add_component<Position>(e2, { 2.f, 0.f, 0.f });

    int count = 0;
    world.query<Position, Health>().for_each([&](Position&, Health&) { ++count; });
    REQUIRE(count == 1);
}

TEST_CASE("ecs_query_entity_count", "[ecs]") {
    anxiety::ecs::World world;
    constexpr int N = 50;

    for (int i = 0; i < N; ++i) {
        auto e = world.create_entity();
        world.add_component<Position>(e, { static_cast<float>(i), 0.f, 0.f });
    }

    REQUIRE(world.query<Position>().entity_count() == size_t(N));
}

TEST_CASE("ecs_query_foreach_with_entity", "[ecs]") {
    anxiety::ecs::World world;
    const auto e = world.create_entity();
    world.add_component<Tag>(e, { 99 });

    bool found = false;
    world.query<Tag>().for_each_with_entity([&](anxiety::ecs::EntityId id, Tag& t) { if (id == e && t.id == 99) found = true; });
    REQUIRE(found);
}

TEST_CASE("ecs_query_multi_archetype", "[ecs]") {
    // Entidades de archetypes distintos que contienen ambas Position deben ser
    // visitadas todas por query<Position>.
    anxiety::ecs::World world;

    auto e1 = world.create_entity();
    auto e2 = world.create_entity();

    world.add_component<Position>(e1, { 1.f, 0.f, 0.f });                   // arch: {Position}
    world.add_component<Position>(e2, { 2.f, 0.f, 0.f });
    world.add_component<Health>(e2, { 10 });                               // arch: {Position, Health}

    float sumX = 0.f;
    world.query<Position>().for_each([&](Position& p) { sumX += p.x; });
    REQUIRE(sumX == 3.0f);
}

// Parte 4 -----------------------------------------------------------------------------------------
// Sistema paralelo vía JobSystem
// ------------------------------------------------------------------------------------------------

TEST_CASE("ecs_parallel_foreach", "[ecs]") {
    anxiety::ecs::World      world;
    anxiety::jobs::JobSystem js(4);

    constexpr int N = 1000;
    for (int i = 0; i < N; ++i) {
        auto e = world.create_entity();
        world.add_component<Position>(e, { static_cast<float>(i), 0.f, 0.f });
        world.add_component<Velocity>(e, { 1.f, 0.f });
    }

    // Actualización de posición en paralelo: p.x += v.vx
    world.query<Position, Velocity>().for_each_parallel(js, [](Position& p, Velocity& v) { p.x += v.vx; });

    // Las 1000 entidades deben haberse actualizado.
    // Cada entidad i tiene x = float(i) + 1.0f.
    int verified = 0;
    world.query<Position, Velocity>().for_each([&](Position& p, Velocity&) {
        REQUIRE((p.x >= 1.f && p.x <= float(N)));
        ++verified;
        });
    REQUIRE(verified == N);
}

TEST_CASE("ecs_parallel_foreach_sum", "[ecs]") {
    // Verifica que la suma en paralelo coincide con la suma secuencial.
    anxiety::ecs::World      world;
    anxiety::jobs::JobSystem js(4);

    for (int i = 1; i <= 100; ++i) {
        auto e = world.create_entity();
        world.add_component<Health>(e, { i });
    }

    std::atomic<long long> parallelSum{ 0 };
    world.query<Health>().for_each_parallel(js, [&parallelSum](Health& h) { parallelSum.fetch_add(h.hp, std::memory_order_relaxed); });

    long long seqSum = 0;
    world.query<Health>().for_each([&seqSum](Health& h) { seqSum += h.hp; });

    REQUIRE(parallelSum.load() == seqSum);
}

// Parte 5 -----------------------------------------------------------------------------------------
// EcsModule
// ------------------------------------------------------------------------------------------------

TEST_CASE("ecs_module_name", "[ecs]") {
    anxiety::ecs::EcsModule mod;
    REQUIRE(mod.name() == std::string_view{ "ECS" });
}

