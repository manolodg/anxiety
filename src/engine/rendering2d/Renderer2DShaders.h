#pragma once

namespace anxiety::rendering2d {

    // Sprite batch vertex shader (GLSL 300 es) ---------------------------------------------------
    inline constexpr const char* k_sprite_batch_vert_GLSL = R"GLSL(
#version 300 es
precision highp float;

layout(location=0) in vec2  a_pos;
layout(location=1) in vec2  a_uv;
layout(location=2) in vec4  a_color;

uniform mat4 u_viewProj;
uniform float u_zLayer;

out vec2 v_uv;
out vec4 v_color;

void main() {
    v_uv    = a_uv;
    v_color = a_color;
    gl_Position = u_viewProj * vec4(a_pos, u_zLayer, 1.0);
}
)GLSL";

    inline constexpr const char* k_sprite_batch_frag_GLSL = R"GLSL(
#version 300 es
precision mediump float;

in vec2 v_uv;
in vec4 v_color;

uniform sampler2D u_tex;

out vec4 fragColor;

void main() {
    vec4 t = texture(u_tex, v_uv);
    fragColor = t * v_color;
    if (fragColor.a < 0.004) discard;
}
)GLSL";

    // GPU-instanced sprite vertex shader ---------------------------------------------------------
    inline constexpr const char* k_sprite_instanced_vert_GLSL = R"GLSL(
#version 300 es
precision highp float;

// Per-vertex quad (unit quad centred at 0)
layout(location=0) in vec2 a_vert;    // [-0.5..+0.5] x2

// Per-instance
layout(location=1) in vec2  i_pos;
layout(location=2) in vec2  i_scale;
layout(location=3) in float i_rot;
layout(location=4) in vec2  i_pivot;
layout(location=5) in vec4  i_color;
layout(location=6) in vec4  i_uvRect; // u0,v0,u1,v1
layout(location=7) in float i_zOrder;

uniform mat4 u_viewProj;

out vec2 v_uv;
out vec4 v_color;

void main() {
    // Apply pivot
    vec2 offset = a_vert - (i_pivot - vec2(0.5));
    // Scale
    offset *= i_scale;
    // Rotate
    float s = sin(i_rot), c = cos(i_rot);
    vec2 rotated = vec2(c*offset.x - s*offset.y,
                        s*offset.x + c*offset.y);
    vec2 world = rotated + i_pos;

    // UV mapping
    vec2 quadUV = a_vert + vec2(0.5);
    v_uv    = i_uvRect.xy + quadUV * (i_uvRect.zw - i_uvRect.xy);
    v_color = i_color;
    gl_Position = u_viewProj * vec4(world, i_zOrder * 0.001, 1.0);
}
)GLSL";

    inline constexpr const char* k_sprite_instanced_frag_GLSL = R"GLSL(
#version 300 es
precision mediump float;

in vec2 v_uv;
in vec4 v_color;

uniform sampler2D u_tex;

out vec4 fragColor;

void main() {
    vec4 t = texture(u_tex, v_uv);
    fragColor = t * v_color;
    if (fragColor.a < 0.004) discard;
}
)GLSL";

    // Normal-mapped sprite shader ----------------------------------------------------------------
    inline constexpr const char* k_sprite_normal_vert_GLSL = R"GLSL(
#version 300 es
precision highp float;

layout(location=0) in vec2  a_pos;
layout(location=1) in vec2  a_uv;
layout(location=2) in vec4  a_color;

uniform mat4 u_viewProj;

out vec2 v_uv;
out vec4 v_color;
out vec2 v_worldPos;

void main() {
    v_uv       = a_uv;
    v_color    = a_color;
    v_worldPos = a_pos;
    gl_Position = u_viewProj * vec4(a_pos, 0.0, 1.0);
}
)GLSL";

    inline constexpr const char* k_sprite_normal_frag_GLSL = R"GLSL(
#version 300 es
precision mediump float;

in vec2 v_uv;
in vec4 v_color;
in vec2 v_worldPos;

uniform sampler2D u_diffuse;
uniform sampler2D u_normalMap;

// Up to 8 point lights
uniform vec3  u_lightPos[8];    // xy=world pos, z=radius
uniform vec4  u_lightColor[8];  // rgb=color, a=intensity
uniform int   u_lightCount;
uniform vec4  u_ambientColor;

out vec4 fragColor;

void main() {
    vec4 diff   = texture(u_diffuse, v_uv) * v_color;
    vec3 normal = texture(u_normalMap, v_uv).rgb * 2.0 - 1.0;
    normal = normalize(normal);

    vec3 lit = u_ambientColor.rgb * u_ambientColor.a;
    for (int i = 0; i < u_lightCount; i++) {
        vec2 toLight = u_lightPos[i].xy - v_worldPos;
        float dist   = length(toLight);
        float atten  = max(0.0, 1.0 - dist / u_lightPos[i].z);
        vec3  lightDir3 = normalize(vec3(toLight, 0.5));
        float ndotl  = max(0.0, dot(normal, lightDir3));
        lit += u_lightColor[i].rgb * (u_lightColor[i].a * ndotl * atten * atten);
    }

    fragColor = vec4(diff.rgb * lit, diff.a);
    if (fragColor.a < 0.004) discard;
}
)GLSL";

    // Tilemap vertex/fragment shader -------------------------------------------------------------
    inline constexpr const char* k_tilemap_vert_GLSL = R"GLSL(
#version 300 es
precision highp float;

layout(location=0) in vec2  a_pos;
layout(location=1) in vec2  a_uv;
layout(location=2) in float a_layer;   // sorting layer depth

uniform mat4 u_viewProj;

out vec2 v_uv;

void main() {
    v_uv = a_uv;
    gl_Position = u_viewProj * vec4(a_pos, a_layer * 0.001, 1.0);
}
)GLSL";

    inline constexpr const char* k_tilemap_frag_GLSL = R"GLSL(
#version 300 es
precision mediump float;

in vec2 v_uv;
uniform sampler2D u_tileset;
out vec4 fragColor;

void main() {
    fragColor = texture(u_tileset, v_uv);
    if (fragColor.a < 0.004) discard;
}
)GLSL";

    // 2D light (radial) composite shader ---------------------------------------------------------
    inline constexpr const char* k_light_2D_frag_GLSL = R"GLSL(
#version 300 es
precision mediump float;

uniform sampler2D u_scene;
uniform sampler2D u_lightmap;
uniform vec2      u_resolution;

out vec4 fragColor;

void main() {
    vec2 uv    = gl_FragCoord.xy / u_resolution;
    vec4 scene = texture(u_scene, uv);
    vec4 light = texture(u_lightmap, uv);
    fragColor  = scene * light;
}
)GLSL";

    // SDF text shader ----------------------------------------------------------------------------
    inline constexpr const char* k_sdf_text_vert_GLSL = R"GLSL(
#version 300 es
precision highp float;

layout(location=0) in vec2 a_pos;
layout(location=1) in vec2 a_uv;
layout(location=2) in vec4 a_color;

uniform mat4 u_viewProj;

out vec2 v_uv;
out vec4 v_color;

void main() {
    v_uv = a_uv;
    v_color = a_color;
    gl_Position = u_viewProj * vec4(a_pos, 0.5, 1.0); // UI on top
}
)GLSL";

    inline constexpr const char* k_sdf_text_frag_GLSL = R"GLSL(
#version 300 es
precision mediump float;

in vec2 v_uv;
in vec4 v_color;

uniform sampler2D u_sdfAtlas;
uniform float     u_softness;   // world-space softness (PX / screenPX)

out vec4 fragColor;

void main() {
    float dist = texture(u_sdfAtlas, v_uv).r;
    float edge = 0.5;
    float alpha = smoothstep(edge - u_softness, edge + u_softness, dist);
    fragColor = vec4(v_color.rgb, v_color.a * alpha);
    if (fragColor.a < 0.004) discard;
}
)GLSL";

    // Particle 2D shader -------------------------------------------------------------------------
    inline constexpr const char* k_particle_2D_vert_GLSL = R"GLSL(
#version 300 es
precision highp float;

// Per-particle instance
layout(location=0) in vec2  i_pos;
layout(location=1) in float i_rot;
layout(location=2) in vec2  i_size;
layout(location=3) in vec4  i_color;
layout(location=4) in vec4  i_uvRect;
layout(location=5) in float i_life;  // 0=dead, 1=just born

uniform mat4 u_viewProj;

out vec2 v_uv;
out vec4 v_color;

// Quad corners 0..3
const vec2 kCorners[4] = vec2[4](
    vec2(-0.5,-0.5), vec2(0.5,-0.5), vec2(0.5,0.5), vec2(-0.5,0.5));

void main() {
    if (i_life <= 0.0) { gl_Position = vec4(0,0,0,0); return; }
    int idx = gl_VertexID % 4;
    vec2 corner = kCorners[idx];
    float s = sin(i_rot), c = cos(i_rot);
    vec2 rot = vec2(c*corner.x - s*corner.y, s*corner.x + c*corner.y);
    vec2 world = i_pos + rot * i_size;
    v_uv    = i_uvRect.xy + (corner + 0.5) * (i_uvRect.zw - i_uvRect.xy);
    v_color = i_color;
    gl_Position = u_viewProj * vec4(world, 0.0, 1.0);
}
)GLSL";

} // namespace anxiety::rendering2d
