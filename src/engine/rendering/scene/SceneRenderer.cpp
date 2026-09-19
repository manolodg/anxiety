#include "SceneRenderer.h"
#include "Logger.h"

namespace anxiety::rendering::scene {
    static constexpr char k_category[] = "SceneRenderer";

    // HLSL inline --------------------------------------------------------------------------------
    // El cbuffer usa `row_major` para que nuestra Mat4 row-major de C++ se suba directamente sin
    // transponer. `mul(M, v)` trata v como un vector columna.

    static constexpr const char* k_scene_VS = R"hlsl(
cbuffer PerObject : register(b0) {
    row_major float4x4 worldViewProj;
};
struct VSIn  { float3 pos : POSITION; float4 col : COLOR; };
struct VSOut { float4 pos : SV_Position; float4 col : COLOR; };
VSOut VSMain(VSIn i) {
    VSOut o;
    o.pos = mul(worldViewProj, float4(i.pos, 1.0f));
    o.col = i.col;
    return o;
}
)hlsl";

    static constexpr const char* k_scene_PS = R"hlsl(
struct VSOut { float4 pos : SV_Position; float4 col : COLOR; };
float4 PSMain(VSOut i) : SV_Target { return i.col; }
)hlsl";

    // Comando de dibujado interno, usado por la lambda capturada del ScenePass ---------------------
    struct DrawCmd {
        rhi::BufferHandle    vertex_buffer;
        rhi::BufferHandle    index_buffer;
        uint32_t             index_count;
        uint32_t             vertex_stride;
        rhi::IDescriptorSet* descriptor_set;        // non-owning, lifetime tied to m_entityData
    };

    // Construction / destruction -----------------------------------------------------------------
    SceneRenderer::SceneRenderer(rhi::IDevice& device) : m_device(device) {}

    SceneRenderer::~SceneRenderer() {
        // Destruye todos los constant buffers por entidad.
        for (auto& [idx, data] : m_entity_data) {
            if (data.constant_buffer.is_valid()) m_device.destroy_buffer(data.constant_buffer);
        }
    }

    // Mesh upload / destroy ----------------------------------------------------------------------
    MeshHandles SceneRenderer::upload_mesh(const void* vertices, size_t vb_bytes, const void* indices, size_t ib_bytes) {
        rhi::BufferDesc vbDesc{ vb_bytes, rhi::BufferUsage::Vertex, "SceneVB" };
        rhi::BufferDesc ibDesc{ ib_bytes, rhi::BufferUsage::Index,  "SceneIB" };

        return { m_device.create_buffer(vbDesc, vertices, vb_bytes), m_device.create_buffer(ibDesc, indices,  ib_bytes), };
    }

    void SceneRenderer::destroy_mesh(MeshHandles h) {
        if (h.vertex_buffer.is_valid()) m_device.destroy_buffer(h.vertex_buffer);
        if (h.index_buffer.is_valid())  m_device.destroy_buffer(h.index_buffer);
    }

    // Inicialización del pipeline ------------------------------------------------------------------
    bool SceneRenderer::init_pipeline() {
        auto vsBc = m_device.compile_shader_from_source(k_scene_VS, "VSMain", rhi::ShaderStage::Vertex);
        auto psBc = m_device.compile_shader_from_source(k_scene_PS, "PSMain", rhi::ShaderStage::Fragment);
        if (vsBc.empty() || psBc.empty()) {
            LOG_ERROR(k_category, "Falló la compilación de los shaders de escena.");
            return false;
        }

        m_vs = m_device.create_shader({ vsBc.data(), vsBc.size(), "VSMain" }, rhi::ShaderStage::Vertex);
        m_ps = m_device.create_shader({ psBc.data(), psBc.size(), "PSMain" }, rhi::ShaderStage::Fragment);
        if (!m_vs || !m_ps) return false;

        rhi::PipelineDesc pd;
        pd.vertex_shader   = m_vs.get();
        pd.fragment_shader = m_ps.get();
        pd.vertex_layout   = {
            .attributes = {
                { "POSITION", 0, rhi::VertexFormat::Float3, 0,  0 },
                { "COLOR",    0, rhi::VertexFormat::Float4, 0, 12 },
            },
            .stride_bytes = 28,                     // float3 pos + float4 col
        };
        pd.topology                  = rhi::PrimitiveTopology::TriangleList;
        pd.rasterizer.cull_mode      = rhi::CullMode::Back;
        pd.rasterizer.front_face_CCW = false;
        pd.render_target_fmts        = { rhi::Format::BGRA8_Unorm };
        pd.descriptor_layout         = { .bindings = {{ 0, rhi::DescriptorType::UniformBuffer }} };
        pd.debug_name                = "ScenePipeline";

        m_pipeline = m_device.create_pipeline(pd);
        if (!m_pipeline) {
            LOG_ERROR(k_category, "Falló la creación del pipeline de escena.");
            return false;
        }

        LOGF_INFO(k_category, "Pipeline de escena listo ({} bytes VS, {} bytes PS).", vsBc.size(), psBc.size());
        return true;
    }

    // Datos de GPU por entidad ------------------------------------------------------------------
    SceneRenderer::PerEntityData& SceneRenderer::ensure_entity_data(uint32_t entity_index) {
        auto& data = m_entity_data[entity_index];
        if (data.constant_buffer.is_valid()) return data;

        // Reserva un constant buffer de 256 bytes (requisito de alineación de root CBV en DX12).
        rhi::BufferDesc cbDesc{ 256, rhi::BufferUsage::Uniform, "SceneEntityCB" };
        data.constant_buffer = m_device.create_buffer(cbDesc);

        // Crea el descriptor set y vincula el CB una única vez (la GPU VA es estable en el heap UPLOAD).
        rhi::DescriptorSetLayout layout{ .bindings = {{ 0, rhi::DescriptorType::UniformBuffer }} };
        data.descriptor_set = m_device.create_descriptor_set(layout);
        data.descriptor_set->update({ { 0, rhi::DescriptorType::UniformBuffer, data.constant_buffer } });

        return data;
    }

    // Construcción de pases por fotograma --------------------------------------------------------
    void SceneRenderer::build_passes(graph::RenderGraph& graph, graph::RGTextureHandle bb_handle, rhi::TextureHandle bb_physical, rhi::ClearColor clear_color, float aspect_ratio) {
        // Nada que renderizar sin un world adjuntado.
        if (!m_world) return;

        if (!m_pipeline && !init_pipeline()) {
            // Falló la inicialización del pipeline; añade un clear pass simple como fallback.
            graph.add_pass({
                .name    = "ClearPass",
                .writes  = {{ bb_handle, rhi::ResourceState::RenderTarget }},
                .execute = [bb_physical, clear_color](rhi::ICommandBuffer& cmd) { cmd.clear_render_target(bb_physical, clear_color); }
            });
            return;
        }

        // 1. Calcula la vista-proyección a partir de la entidad Camera activa --------------------
        Mat4 viewMat = mat4_identity();
        Mat4 projMat = mat4_identity();

        if (m_world) {
            bool foundCamera = false;
            m_world->query<Transform, Camera>().for_each(
                [&](Transform& t, Camera& cam) {
                    if (foundCamera) return;
                    foundCamera = true;

                    Vec3 eye = { t.position[0], t.position[1], t.position[2] };
                    Vec3 forward = quat_rotate(t.rotation[0], t.rotation[1], t.rotation[2],
                        t.rotation[3], { 0.f, 0.f, 1.f });
                    Vec3 center = vec3_add(eye, forward);

                    viewMat = mat4_look_at_LH(eye, center, { 0.f, 1.f, 0.f });

                    const float ar = (aspect_ratio > 0.f) ? aspect_ratio : cam.aspect_ratio;
                    projMat = mat4_perspective_LH(cam.fov_Y, ar, cam.near_Z, cam.far_Z);
                });
        }

        // 2. Extrae la lista de dibujado de las entidades con Transform + MeshRenderer -----------
        std::vector<DrawCmd> drawList;

        if (m_world) {
            m_world->query<Transform, MeshRenderer>().for_each_with_entity(
                [&](anxiety::ecs::EntityId id, Transform& t, MeshRenderer& mr) {
                    if (!mr.vertex_buffer.is_valid() || !mr.index_buffer.is_valid() || mr.index_count == 0) return;

                    // Calcula WorldViewProjection: P * V * W (convención de vector columna).
                    Mat4 world = mat4_TRS( { t.position[0], t.position[1], t.position[2] }, t.rotation[0], t.rotation[1], t.rotation[2], t.rotation[3], { t.scale[0],    t.scale[1],    t.scale[2] });
                    Mat4 wvp   = mat4_mul(projMat, mat4_mul(viewMat, world));

                    // Crea de forma perezosa el CB y el descriptor set, y escribe la nueva WVP.
                    PerEntityData& data = ensure_entity_data(id.index);
                    m_device.write_buffer(data.constant_buffer, wvp.data(), 0, sizeof(Mat4));

                    drawList.push_back({
                        .vertex_buffer  = mr.vertex_buffer,
                        .index_buffer   = mr.index_buffer,
                        .index_count    = mr.index_count,
                        .vertex_stride  = mr.vertex_stride,
                        .descriptor_set = data.descriptor_set.get(),
                        });
                });
        }

        // 3. ClearPass ---------------------------------------------------------------------------
        graph.add_pass({
            .name    = "ClearPass",
            .writes  = {{ bb_handle, rhi::ResourceState::RenderTarget }},
            .execute = [bb_physical, clear_color](rhi::ICommandBuffer& cmd) { cmd.clear_render_target(bb_physical, clear_color); }
        });

        // 4. ScenePass (solo cuando hay elementos que dibujar) -----------------------------------
        if (!drawList.empty()) {
            rhi::IPipeline* pipe = m_pipeline.get();

            graph.add_pass({
                .name    = "ScenePass",
                // El read establece la arista de orden desde ClearPass; el write marca la modificación del RT.
                .reads   = {{ bb_handle, rhi::ResourceState::RenderTarget }},
                .writes  = {{ bb_handle, rhi::ResourceState::RenderTarget }},
                .execute = [pipe, drawList = std::move(drawList)](rhi::ICommandBuffer& cmd) {
                    for (const DrawCmd& d : drawList) {
                        cmd.bind_pipeline(*pipe);
                        cmd.bind_descriptor_set(0, *d.descriptor_set);
                        cmd.bind_vertex_buffer(0, d.vertex_buffer, 0, d.vertex_stride);
                        cmd.bind_index_buffer(d.index_buffer, 0, /*use32bit=*/true);
                        cmd.draw_indexed(d.index_count);
                    }
                }
            });
        }
    }
} // namespace anxiety::rendering::scene