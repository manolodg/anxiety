#pragma once

#include "RHITypes.h"
#include "SamplerDesc.h"
#include "ShaderTypes.h"
#include "VertexLayout.h"
#include "IDescriptorSet.h"

#include <vector>

namespace anxiety::rendering::rhi {
    // Topology -----------------------------------------------------------------------------------
    enum class PrimitiveTopology : uint32_t {
        TriangleList,
        TriangleStrip,
        LineList,
        LineStrip,
        PointList,
    };

    // Rasterizer ---------------------------------------------------------------------------------
    enum class FillMode : uint32_t { Solid, Wireframe };
    enum class CullMode : uint32_t { None, Front, Back };

    struct RasterizerDesc {
        FillMode fill_mode      = FillMode::Solid;
        CullMode cull_mode      = CullMode::Back;
        bool     front_face_CCW = true;                     // antihorario = cara frontal (convención de OpenGL)
    };

    // Depth / stencil ----------------------------------------------------------------------------
    enum class CompareOp : uint32_t { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };

    struct DepthStencilDesc {
        bool      depth_test_enable  = false;
        bool      depth_write_enable = false;
        CompareOp depth_compare_op   = CompareOp::Less;
        Format    depth_format       = Format::Unknown;     // Unknown = sin adjunto de profundidad
    };

    // Blend --------------------------------------------------------------------------------------
    enum class BlendFactor : uint32_t {
        Zero,     One,
        SrcAlpha, OneMinusSrcAlpha,
        DstAlpha, OneMinusDstAlpha,
        SrcColor, OneMinusSrcColor,
    };
    enum class BlendOp : uint32_t { Add, Subtract, ReverseSubtract, Min, Max };

    struct BlendAttachmentDesc {
        bool        blend_enable     = false;
        BlendFactor src_color_factor = BlendFactor::One;
        BlendFactor dst_color_factor = BlendFactor::Zero;
        BlendOp     color_blend_op   = BlendOp::Add;
        BlendFactor src_alpha_factor = BlendFactor::One;
        BlendFactor dst_alpha_factor = BlendFactor::Zero;
        BlendOp     alpha_blend_op   = BlendOp::Add;
    };

    // PipelineDesc -------------------------------------------------------------------------------
    // Descripción completa de un objeto de estado de pipeline gráfico (PSO) inmutable.
    //
    //   vertex_shader / fragment_shader — objetos shader compilados; deben seguir vivos mientras dure create_pipeline().
    //   vertex_layout                   — layout de atributos por vértice para el input assembler.
    //   topology                        — tipo de primitiva.
    //   rasterizer                      — fill mode, cull mode, winding.
    //   depth_stencil                   — configuración de test/escritura de profundidad.
    //   blend                           — configuración de blend por adjunto.
    //   render_target_fmts              — formatos de los adjuntos de render target, en orden.
    //   descriptor_layout               — describe qué recursos esperan los shaders (se corresponde directamente con el root signature de D3D12 / pipeline layout de Vulkan).
    //   debug_name                      — etiqueta opcional mostrada en capturas de GPU.
    struct PipelineDesc {
        const IShader*           vertex_shader      = nullptr;
        const IShader*           fragment_shader    = nullptr;
        VertexLayout             vertex_layout;
        PrimitiveTopology        topology           = PrimitiveTopology::TriangleList;
        RasterizerDesc           rasterizer;
        DepthStencilDesc         depth_stencil;
        BlendAttachmentDesc      blend;
        std::vector<Format>      render_target_fmts;
        DescriptorSetLayout      descriptor_layout;
        // Samplers estáticos incrustados en el root signature. Cada entrada se corresponde con un
        // registro sN. Usa las funciones factoría de SamplerDesc (sampler_linear_wrap, etc.) para las
        // configuraciones habituales.
        std::vector<SamplerDesc> static_samplers;
        const char*              debug_name         = nullptr;
    };
} // namespace anxiety::rendering::rhi