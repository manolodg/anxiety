#pragma once

namespace anxiety::ecs::components {
    // Transform ----------------------------------------------------------------------------------
    // El componente fundamental de posicionamiento en escena — registrado como "core.transform"
    // (ver CoreModule::on_init()). Toda entidad de escena lo tiene; Player/Camera/Light/etc. no son
    // tipos especiales de entidad, siguen siendo composiciones de componentes con este entre ellos.
    //
    // Deliberadamente sin ninguna dependencia de una librería de matemáticas: el motor todavía no
    // tiene una (ni Vector3 ni Quaternion existen como tipos compartidos), y crear una ahora sería
    // adelantarse a lo que este paso necesita — solo garantizar que Core.Transform existe y puede
    // descubrirse. Vec3 es deliberadamente un struct interno mínimo (solo datos, sin operadores) que
    // desaparece en cuanto el motor tenga su propio tipo de vector.
    // --------------------------------------------------------------------------------------------
    struct Transform {
        struct Vec3 { float x = 0.f, y = 0.f, z = 0.f; };

        Vec3 position;
        Vec3 rotation;                    // grados de Euler — sin quaternion todavía
        Vec3 scale{ 1.f, 1.f, 1.f };
    };
} // namespace anxiety::ecs::components
