#pragma once

#include <cstdint>
#include <vector>

namespace anxiety::rendering::rhi {
    // VertexFormat -------------------------------------------------------------------------------
    enum class VertexFormat : uint32_t {
        Float, Float2, Float3, Float4,
        UInt,  UInt2,  UInt3,  UInt4,
        UByte4, UByte4Norm,
    };

    // VertexAttribute ----------------------------------------------------------------------------
    // Describe un canal por vértice: dónde vive en el stream y cómo se corresponde con un registro
    // de entrada del shader.
    //
    //   semantic_name   — nombre de semántica HLSL (p. ej. "POSITION", "COLOR", "TEXCOORD")
    //   semantic_index  — índice de semántica HLSL (p. ej. TEXCOORD0 vs TEXCOORD1)
    //   format          — tipo de dato y número de canales en memoria
    //   input_slot      — de qué slot de vertex buffer lee este canal
    //   byte_offset     — desplazamiento en bytes dentro de un registro de vértice
    struct VertexAttribute {
        const char*  semantic_name  = "TEXCOORD";
        uint32_t     semantic_index = 0;
        VertexFormat format         = VertexFormat::Float4;
        uint32_t     input_slot     = 0;
        uint32_t     byte_offset    = 0;
    };

    // VertexLayout -------------------------------------------------------------------------------
    // Describe el registro de vértice completo para la etapa input-assembler de un pipeline.
    //   attributes   — lista de canales por vértice
    //   stride_bytes — tamaño en bytes de un registro de vértice completo (para IASetVertexBuffers)
    struct VertexLayout {
        std::vector<VertexAttribute> attributes;
        uint32_t                     stride_bytes = 0;
    };
} // namespace anxiety::rendering::rhi
