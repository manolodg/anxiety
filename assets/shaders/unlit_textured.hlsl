// unlit_textured.hlsl — superficie con color de vértice + textura opcional -----------------------
// Registros
//   b0 : PerObject   — world_view_proj (64 bytes, float4x4 row-major)
//   b1 : PerMaterial — base_color (float4) + use_texture (int) + _pad0/1/2 (float)
//   t0 : textura de albedo (Texture2D)
//   s0 : sampler linear wrap (estático, incrustado en el root signature)
//
// Vertex layout (debe coincidir con PipelineDesc):
//   POSITION  float3  offset  0  (12 bytes)
//   COLOR     float4  offset 12  (16 bytes)
//   TEXCOORD  float2  offset 28  ( 8 bytes)
//   stride = 36 bytes
// ------------------------------------------------------------------------------------------------
cbuffer PerObject : register(b0) {
    row_major float4x4 world_view_proj;
};
cbuffer PerMaterial : register(b1) {
    float4 base_color;
    int    use_texture;
    // Tres escalares en vez de float3: un float3 en el offset 20 no es representable en std140
    // (OpenGL/SPIRV-Cross exige alinear un vec3 a 16). El tamaño y los offsets son los mismos.
    float  _pad0;
    float  _pad1;
    float  _pad2;
};

Texture2D    tex_albedo : register(t0);
SamplerState sam_linear : register(s0);

struct VSIn {
    float3 pos : POSITION;
    float4 col : COLOR;
    float2 uv  : TEXCOORD;
};

struct VSOut {
    float4 pos : SV_Position;
    float4 col : COLOR;
    float2 uv  : TEXCOORD;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.pos = mul(world_view_proj, float4(i.pos, 1.0f));
    o.col = i.col * base_color;
    o.uv  = i.uv;
    return o;
}

float4 PSMain(VSOut i) : SV_Target {
    float4 tex_sample = (use_texture != 0) ? tex_albedo.Sample(sam_linear, i.uv) : float4(1.0f, 1.0f, 1.0f, 1.0f);
    return i.col * tex_sample;
}