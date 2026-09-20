// Anxiety - Material Unlit ----------------------------------------------------------------------
// Color de vértice modulado por un tinte de color base por material.
//
// Distribución de los constant buffers:
//   b0  PerObject   - por entidad   : row_major float4x4 world_view_proj
//   b1  PerMaterial - por instancia : float4 base_color
//
// Vertex layout (28 bytes, coincide con el valor por defecto de SceneComponents):
//   float3 POSITION (offset  0)
//   float4 COLOR    (offset 12)
// ------------------------------------------------------------------------------------------------
cbuffer PerObject : register(b0) {
    row_major float4x4 world_view_proj;
}

cbuffer PerMaterial : register(b1) {
    float4 base_color;
}

struct VSIn {
    float3 pos : POSITION;
    float4 col : COLOR;
};

struct VSOut {
    float4 pos : SV_Position;
    float4 col : COLOR;
};

VSOut VSMain(VSIn i) {
    VSOut o;
    o.pos = mul(world_view_proj, float4(i.pos, 1.0f));
    o.col = i.col * base_color;
    return o;
}

float4 PSMain(VSOut i) : SV_Target {
    return i.col;
}
