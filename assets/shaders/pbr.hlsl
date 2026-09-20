// pbr.hlsl  —  Renderizado basado en físicas (BRDF de Cook-Torrance) -----------------------------
// Vertex layout (stride 48 bytes):
//   POSITION  float3  offset  0
//   NORMAL    float3  offset 12
//   TEXCOORD  float2  offset 24
//   TANGENT   float4  offset 32   (xyz = tangente, w = signo de la bitangente)
//
// Layout de los constant buffers:
//   b0  PerObject   128 bytes  worldViewProj + worldMatrix
//   b1  PerMaterial  64 bytes  baseColor, useAlbedoTex, metallic, roughness, emissive, useNormalTex, useOrmTex
//   b2  LightsCB    576 bytes  luz direccional + hasta 16 luces puntuales
//
// Texturas:
//   t0  textura de albedo (RGBA)
//   t1  mapa de normales  (XYZ en espacio tangente, en RGB, lineal)
//   t2  textura ORM       (R=occlusion, G=roughness, B=metallic)
//
// Sampler estático:
//   s0  linear wrap
// ------------------------------------------------------------------------------------------------

// Constant buffers -------------------------------------------------------------------------------
cbuffer PerObject : register(b0) {
    row_major float4x4 world_view_proj;                     // offset   0
    row_major float4x4 world_matrix;                        // offset  64
}

cbuffer PerMaterial : register(b1) {
    float4 base_color;                                      // offset  0  (RGBA albedo tint)
    int    use_albedo_tex;                                  // offset 16
    float  metallic;                                        // offset 20
    float  roughness;                                       // offset 24
    float  _pad0;                                           // offset 28
    float3 emissive;                                        // offset 32
    int    use_normal_tex;                                  // offset 44
    int    use_orm_tex;                                     // offset 48
    float  _pad1a;                                          // offset 52  (escalares, no float3: std140 exige alinear un vec3 a 16)
    float  _pad1b;                                          // offset 56
    float  _pad1c;                                          // offset 60
}

struct GpuPointLight {
    float3 position;
    float  intensity;
    float3 color;
    float  range;
};

cbuffer LightsCB : register(b2) {
    float3        dir_direction;                            // offset   0
    float         dir_intensity;                            // offset  12
    float3        dir_color;                                // offset  16
    float         dir_pad;                                  // offset  28
    int           num_point_lights;                         // offset  32
    float         _light_pad0;                              // offset  36 (escalares, no float3: ver PerMaterial)
    float         _light_pad1;                              // offset  40
    float         _light_pad2;                              // offset  44
    GpuPointLight point_lights[16];                         // offset  48  (32 bytes × 16 = 512)
    float3        camera_pos;                               // offset 560
    float         _pad2;                                    // offset 572
}

// Texturas + sampler -----------------------------------------------------------------------------

Texture2D    tex_albedo : register(t0);
Texture2D    tex_normal : register(t1);
Texture2D    tex_ORM    : register(t2);
SamplerState sam_linear : register(s0);

// Structs de vértice / píxel ----------------------------------------------------------------------

struct VSIn {
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD;
    float4 tangent  : TANGENT;                              // xyz = tangente, w = signo de la bitangente
};

struct VSOut {
    float4 pos_CS       : SV_Position;
    float3 pos_WS       : TEXCOORD0;                        // posición en espacio de mundo
    float3 normal_WS    : TEXCOORD1;                        // normal en espacio de mundo (pre-normalizada)
    float2 uv           : TEXCOORD2;
    float3 tangent_WS   : TEXCOORD3;
    float3 bitangent_WS : TEXCOORD4;
};

// Vertex shader ------------------------------------------------------------------------------------

VSOut VSMain(VSIn input) {
    VSOut o;

    // Posición en clip-space
    o.pos_CS = mul(world_view_proj, float4(input.position, 1.0f));

    // Posición en espacio de mundo
    o.pos_WS = mul(world_matrix, float4(input.position, 1.0f)).xyz;

    // Normal en espacio de mundo (asume escala uniforme; si no lo es, usar la inversa-transpuesta)
    float3x3 wm3 = (float3x3) world_matrix;
    o.normal_WS  = normalize(mul(wm3, input.normal));
    o.tangent_WS = normalize(mul(wm3, input.tangent.xyz));
    // Re-ortogonaliza la tangente (Gram-Schmidt) y aplica el signo de orientación
    o.tangent_WS   = normalize(o.tangent_WS - dot(o.tangent_WS, o.normal_WS) * o.normal_WS);
    o.bitangent_WS = cross(o.normal_WS, o.tangent_WS) * input.tangent.w;

    o.uv = input.uv;
    return o;
}

// Funciones auxiliares de PBR ----------------------------------------------------------------------

static const float PI = 3.14159265358979323846f;

// Función de distribución normal GGX / Trowbridge-Reitz
float D_GGX(float N_dot_H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float d = (N_dot_H * N_dot_H) * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * d * d);
}

// Función de geometría Schlick-GGX (un solo lado)
float G_SchlickGGX(float N_dot_V, float roughness) {
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return N_dot_V / (N_dot_V * (1.0f - k) + k);
}

// Término de geometría de Smith (lado de vista y de luz)
float G_Smith(float N_dot_V, float N_dot_L, float roughness) {
    return G_SchlickGGX(N_dot_V, roughness) * G_SchlickGGX(N_dot_L, roughness);
}

// Aproximación de Fresnel de Schlick
float3 F_Schlick(float H_dot_V, float3 F0) {
    return F0 + (1.0f - F0) * pow(saturate(1.0f - H_dot_V), 5.0f);
}

// BRDF de Cook-Torrance: devuelve la contribución combinada de luz directa de una luz.
// L = dirección normalizada de la superficie HACIA la luz.
// V = dirección normalizada de la superficie HACIA la cámara.
// N = normal de la superficie (posiblemente con normal map aplicado).
float3 CookTorranceBRDF(float3 albedo, float metallic, float rough, float3 N, float3 V, float3 L, float3 light_color) {
    float3 H = normalize(V + L);

    float N_dot_V = saturate(dot(N, V));
    float N_dot_L = saturate(dot(N, L));
    float N_dot_H = saturate(dot(N, H));
    float H_dot_V = saturate(dot(H, V));

    if (N_dot_L <= 0.0f) return float3(0, 0, 0);

    // Reflectancia base en incidencia normal
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    float D  = D_GGX(N_dot_H, rough);
    float G  = G_Smith(N_dot_V, N_dot_L, rough);
    float3 F = F_Schlick(H_dot_V, F0);

    // Término especular
    float3 specular = (D * G * F) / max(4.0f * N_dot_V * N_dot_L, 0.0001f);

    // Difuso: los metales no tienen componente difusa
    float3 kD = (float3(1, 1, 1) - F) * (1.0f - metallic);
    float3 diffuse = kD * albedo / PI;

    return (diffuse + specular) * light_color * N_dot_L;
}

// Atenuación con caída suave para luces puntuales
float pointLightAttenuation(float dist, float range) {
    float t = saturate(dist / range);
    float f = saturate(1.0f - t * t);
    return (f * f) / (dist * dist + 1.0f);
}

// Pixel shader -------------------------------------------------------------------------------------

float4 PSMain(VSOut input) : SV_Target {
    // Muestrea las texturas -------------------------------------------------------------------------
    float4 albedo_sample = (use_albedo_tex != 0) ? tex_albedo.Sample(sam_linear, input.uv) : float4(1, 1, 1, 1);
    float4 albedo4       = base_color * albedo_sample;
    float3 albedo        = albedo4.rgb;
    float  alpha         = albedo4.a;

    float mat_metallic = metallic;
    float mat_roughness = max(roughness, 0.05f);            // evita una superficie perfectamente lisa
    float ao = 1.0f;

    if (use_orm_tex != 0) {
        float3 orm = tex_ORM.Sample(sam_linear, input.uv).rgb;
        ao = orm.r;
        mat_roughness = max(orm.g, 0.05f);
        mat_metallic = orm.b;
    }

    // Normal map ------------------------------------------------------------------------------------
    float3 N = normalize(input.normal_WS);
    if (use_normal_tex != 0) {
        float3 tnorm = tex_normal.Sample(sam_linear, input.uv).xyz * 2.0f - 1.0f;
        float3 T     = normalize(input.tangent_WS);
        float3 B     = normalize(input.bitangent_WS);
        N = normalize(T * tnorm.x + B * tnorm.y + N * tnorm.z);
    }

    float3 V = normalize(camera_pos - input.pos_WS);

    // Luz direccional ---------------------------------------------------------------------------
    float3 Lo = float3(0, 0, 0);
    {
        float3 L = normalize(-dir_direction);                           // dirección de la superficie hacia la luz
        Lo += CookTorranceBRDF(albedo, mat_metallic, mat_roughness, N, V, L, dir_color * dir_intensity);
    }

    // Luces puntuales -----------------------------------------------------------------------------
    for (int i = 0; i < num_point_lights; ++i) {
        float3 to_light = point_lights[i].position - input.pos_WS;
        float  dist     = length(to_light);
        float3 L        = to_light / max(dist, 0.0001f);

        float atten = pointLightAttenuation(dist, point_lights[i].range);
        float3 lightRadiance = point_lights[i].color * point_lights[i].intensity * atten;

        Lo += CookTorranceBRDF(albedo, mat_metallic, mat_roughness, N, V, L, lightRadiance);
    }

    // Ambiental (constante, sin IBL) ---------------------------------------------------------------
    float3 ambient = 0.03f * albedo * ao;

    // Emisiva -----------------------------------------------------------------------------------
    float3 color = ambient + Lo + emissive;

    // Tonemapping de Reinhard + corrección de gamma (sRGB aproximado)
    color = color / (color + float3(1, 1, 1));
    color = pow(saturate(color), 1.0f / 2.2f);

    return float4(color, alpha);
}