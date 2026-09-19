#pragma once

#include <cmath>

// SceneMath — matemática mínima de matrices/vectores en row-major (solo cabecera) -----------------
// Convención: vector columna — las transformaciones se aplican como v' = M * v
// Esto coincide con `mul(M, v)` de HLSL con `row_major float4x4 M` en los cbuffers.
//
// Concatenación de matrices: world = T * R * S   (primero escala, luego rota, traslada)
// WVP                       : wvp   = P * V * W   (world primero, proj en el exterior)
// ------------------------------------------------------------------------------------------------
namespace anxiety::rendering::scene {
    // Vec3 / Vec4 --------------------------------------------------------------------------------

    struct Vec3 { float x, y, z; };
    struct Vec4 { float x, y, z, w; };

    [[nodiscard]] inline Vec3  vec3_add  (Vec3 a, Vec3 b)  { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
    [[nodiscard]] inline Vec3  vec3_sub  (Vec3 a, Vec3 b)  { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    [[nodiscard]] inline Vec3  vec3_scale(Vec3 v, float s) { return { v.x * s, v.y * s, v.z * s }; }
    [[nodiscard]] inline float vec3_dot  (Vec3 a, Vec3 b)  { return a.x * b.x + a.y * b.y + a.z * b.z; }
    [[nodiscard]] inline Vec3  vec3_cross(Vec3 a, Vec3 b)  { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
    
    [[nodiscard]] inline Vec3  vec3_normalize(Vec3 v) {
        const float len = std::sqrt(vec3_dot(v, v));
        if (len < 1e-7f) return { 0.f, 0.f, 1.f };
        return vec3_scale(v, 1.f / len);
    }

    // Mat4 ---------------------------------------------------------------------------------------
    // Matriz 4×4 row-major almacenada como m[fila][col]. Layout en memoria: m[0][0..3] = fila 0,
    // m[1][0..3] = fila 1, …
    // Coincide con el empaquetado de cbuffer `row_major float4x4` de HLSL.
    // --------------------------------------------------------------------------------------------
    struct Mat4 {
        float m[4][4] = {};

        [[nodiscard]] float*       data()       { return &m[0][0]; }
        [[nodiscard]] const float* data() const { return &m[0][0]; }
    };

    [[nodiscard]] inline Mat4 mat4_identity() {
        Mat4 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.f;
        return r;
    }

    // Traslación: la columna 3 de las filas 0-2 guarda la traslación (convención de vector columna).
    [[nodiscard]] inline Mat4 mat4_translate(Vec3 t) {
        Mat4 r = mat4_identity();
        r.m[0][3] = t.x;
        r.m[1][3] = t.y;
        r.m[2][3] = t.z;
        return r;
    }

    // Escala uniforme / no uniforme.
    [[nodiscard]] inline Mat4 mat4_scale(Vec3 s) {
        Mat4 r = mat4_identity();
        r.m[0][0] = s.x;
        r.m[1][1] = s.y;
        r.m[2][2] = s.z;
        return r;
    }

    // Rotación a partir de un cuaternión unitario (xyzw).
    [[nodiscard]] inline Mat4 mat4_from_quat(float qx, float qy, float qz, float qw) {
        const float xx = qx * qx, yy = qy * qy, zz = qz * qz;
        const float xy = qx * qy, xz = qx * qz, yz = qy * qz;
        const float wx = qw * qx, wy = qw * qy, wz = qw * qz;
        Mat4 r = mat4_identity();
        r.m[0][0] = 1.f - 2.f * (yy + zz);  r.m[0][1] = 2.f * (xy + wz);       r.m[0][2] = 2.f * (xz - wy);
        r.m[1][0] = 2.f * (xy - wz);         r.m[1][1] = 1.f - 2.f * (xx + zz); r.m[1][2] = 2.f * (yz + wx);
        r.m[2][0] = 2.f * (xz + wy);         r.m[2][1] = 2.f * (yz - wx);       r.m[2][2] = 1.f - 2.f * (xx + yy);
        return r;
    }

    // Rota el vector v por el cuaternión unitario (xyzw).
    [[nodiscard]] inline Vec3 quat_rotate(float qx, float qy, float qz, float qw, Vec3 v) {
        Vec3 qv = { qx, qy, qz };
        Vec3 t = vec3_scale(vec3_cross(qv, v), 2.f);
        return vec3_add(v, vec3_add(vec3_scale(t, qw), vec3_cross(qv, t)));
    }

    // Multiplicación de matrices: C = A * B  (row-major)
    [[nodiscard]] inline Mat4 mat4_mul(const Mat4& A, const Mat4& B) {
        Mat4 C;
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                for (int k = 0; k < 4; ++k) {
                    C.m[r][c] += A.m[r][k] * B.m[k][c];
                }
            }
        }
        return C;
    }

    // Matriz de mundo TRS: T * R * S  (escala → rota → traslada, convención de vector columna).
    [[nodiscard]] inline Mat4 mat4_TRS(Vec3 t, float qx, float qy, float qz, float qw, Vec3 s) { return mat4_mul(mat4_translate(t), mat4_mul(mat4_from_quat(qx, qy, qz, qw), mat4_scale(s))); }

    // Proyección en perspectiva left-handed, z ∈ [0,1]  (convención de DirectX / DX12).
    [[nodiscard]] inline Mat4 mat4_perspective_LH(float fovY, float aspect, float nearZ, float farZ) {
        const float f = 1.f / std::tan(fovY * 0.5f);
        const float Q = farZ / (farZ - nearZ);
        Mat4 r;
        r.m[0][0] = f / aspect;
        r.m[1][1] = f;
        r.m[2][2] = Q;
        r.m[2][3] = -nearZ * Q;                     // desplazamiento z_clip: z_ndc = 0 en nearZ, 1 en farZ
        r.m[3][2] = 1.f;                            // w_clip = z_view  (división de perspectiva)
        return r;
    }

    // Matriz de vista look-at left-handed (convención de vector columna).
    // eye: posición de la cámara en el mundo.  center: objetivo hacia el que mira.  up: pista de "arriba" del mundo.
    [[nodiscard]] inline Mat4 mat4_look_at_LH(Vec3 eye, Vec3 center, Vec3 up) {
        Vec3 f = vec3_normalize(vec3_sub(center, eye));     // adelante (+Z en LH)
        Vec3 r = vec3_normalize(vec3_cross(up, f));         // derecha (up × adelante para LH)
        Vec3 u = vec3_cross(f, r);                          // "arriba" de cámara corregido

        Mat4 m = mat4_identity();
        m.m[0][0] = r.x; m.m[0][1] = r.y; m.m[0][2] = r.z; m.m[0][3] = -vec3_dot(r, eye);
        m.m[1][0] = u.x; m.m[1][1] = u.y; m.m[1][2] = u.z; m.m[1][3] = -vec3_dot(u, eye);
        m.m[2][0] = f.x; m.m[2][1] = f.y; m.m[2][2] = f.z; m.m[2][3] = -vec3_dot(f, eye);

        return m;
    }
} // namespace anxiety::rendering::scene
