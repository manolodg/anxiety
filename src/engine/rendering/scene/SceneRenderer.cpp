#include "SceneRenderer.h"
#include "textures/TextureManager.h"
#include "Logger.h"

namespace anxiety::rendering::scene {
    static constexpr char k_category[] = "SceneRenderer";

    // Comando de dibujado interno, usado por la lambda capturada del ScenePass ---------------------
    struct DrawCmd {
        rhi::BufferHandle    vertex_buffer;
        rhi::BufferHandle    index_buffer;
        uint32_t             index_count;
        uint32_t             vertex_stride;
        rhi::IPipeline*      pipeline;              // sin propiedad, procede de Material::pipeline()
        rhi::IDescriptorSet* descriptor_set;        // sin propiedad, su vida va ligada a m_entity_data
    };

    // Construcción / destrucción ----------------------------------------------------------------
    SceneRenderer::SceneRenderer(rhi::IDevice& device, materials::MaterialManager* materials, textures::TextureManager* textures) : m_device(device), m_materials(materials), m_textures(textures) {}

    SceneRenderer::~SceneRenderer() {
        // Destruye todos los constant buffers por entidad.
        for (auto& [idx, data] : m_entity_data) {
            if (data.wvp_buffer.is_valid())        m_device.destroy_buffer(data.wvp_buffer);
            if (data.mat_params_buffer.is_valid()) m_device.destroy_buffer(data.mat_params_buffer);
        }
    }

    // Subida / destrucción de mallas ----------------------------------------------------------
    MeshHandles SceneRenderer::upload_mesh(const void* vertices, size_t vb_bytes, const void* indices, size_t ib_bytes) {
        rhi::BufferDesc vbDesc{ vb_bytes, rhi::BufferUsage::Vertex, "SceneVB" };
        rhi::BufferDesc ibDesc{ ib_bytes, rhi::BufferUsage::Index,  "SceneIB" };

        return { m_device.create_buffer(vbDesc, vertices, vb_bytes), m_device.create_buffer(ibDesc, indices,  ib_bytes), };
    }

    void SceneRenderer::destroy_mesh(MeshHandles h) {
        if (h.vertex_buffer.is_valid()) m_device.destroy_buffer(h.vertex_buffer);
        if (h.index_buffer.is_valid())  m_device.destroy_buffer(h.index_buffer);
    }

    // Datos de GPU por entidad ------------------------------------------------------------------
    SceneRenderer::PerEntityData& SceneRenderer::ensure_entity_data(uint32_t entity_index) {
        auto& data = m_entity_data[entity_index];

        if (data.wvp_buffer.is_valid()) return data;

        // Reserva dos constant buffers de 256 bytes (requisito de alineación de root CBV en DX12).
        data.wvp_buffer        = m_device.create_buffer({ 256, rhi::BufferUsage::Uniform, "SceneWVP" });
        data.mat_params_buffer = m_device.create_buffer({ 256, rhi::BufferUsage::Uniform, "SceneMat" });

        // Un único descriptor set con ambos bindings:
        //  binding 0 - b0 (PerObject  : WVP)
        //  binding 1 - b1 (PerMaterial: base_color)
        rhi::DescriptorSetLayout layout{ .bindings = {
            { 0, rhi::DescriptorType::UniformBuffer },
            { 1, rhi::DescriptorType::UniformBuffer }
        }};
        data.descriptor_set = m_device.create_descriptor_set(layout);
        data.descriptor_set->update({
            { 0, rhi::DescriptorType::UniformBuffer, data.wvp_buffer },
            { 1, rhi::DescriptorType::UniformBuffer, data.mat_params_buffer }
        });

        return data;
    }

    void SceneRenderer::refresh_entity_DS(PerEntityData& data, materials::Material* mat, rhi::TextureHandle albedo_tex) {
        if (data.last_material == mat->handle() && data.descriptor_set) return;

        // Recrea el descriptor set con el layout del material.
        data.descriptor_set = m_device.create_descriptor_set(mat->descriptor_layout());

        std::vector<rhi::DescriptorWrite> writes = {
            { 0, rhi::DescriptorType::UniformBuffer, data.wvp_buffer },
            { 1, rhi::DescriptorType::UniformBuffer, data.mat_params_buffer }
        };

        // Vincula la textura a cualquier binding de tipo Texture del layout.
        for (const auto& b : mat->descriptor_layout().bindings) {
            if (b.type == rhi::DescriptorType::Texture) {
                rhi::TextureHandle tex_to_use = albedo_tex.is_valid() ? albedo_tex : (m_textures ? m_textures->null_texture() : rhi::TextureHandle{});
                if (tex_to_use.is_valid()) writes.push_back({ b.binding, rhi::DescriptorType::Texture, rhi::BufferHandle{}, tex_to_use });
            }
        }

        data.descriptor_set->update(writes);
        data.last_material = mat->handle();
    }

    // Construcción de pases por fotograma --------------------------------------------------------
    void SceneRenderer::build_passes(graph::RenderGraph& graph, graph::RGTextureHandle bb_handle, rhi::TextureHandle bb_physical, rhi::ClearColor clear_color, float aspect_ratio) {
        if (!m_world) return;

        // Crea de forma perezosa la instancia de material por defecto (unlit, blanco).
        if (m_materials && !m_default_instance.is_valid() && m_materials->default_unlit().is_valid()) m_default_instance = m_materials->create_instance(m_materials->default_unlit());

        // 1. Vista-proyección a partir de la primera entidad Camera+Transform --------------------
        Mat4 view_mat = mat4_identity();
        Mat4 proj_mat = mat4_identity();

        m_world->query<Transform, Camera>().for_each([&, found = false](Transform& t, Camera& cam) mutable {
            if (found) return;
            found = true;

            Vec3 eye     = { t.position[0], t.position[1], t.position[2] };
            Vec3 forward = quat_rotate(t.rotation[0], t.rotation[1], t.rotation[2], t.rotation[3], { 0.0f, 0.0f, 1.0f });
            Vec3 center  = vec3_add(eye, forward);

            view_mat = mat4_look_at_LH(eye, center, { 0.f, 1.f, 0.f });

            const float ar = (aspect_ratio > 0.0f) ? aspect_ratio : cam.aspect_ratio;
            proj_mat = mat4_perspective_LH(cam.fov_Y, ar, cam.near_Z, cam.far_Z);
        });

        // 2. Construye la lista de dibujado de las entidades con Transform + MeshRenderer --------
        std::vector<DrawCmd> drawList;

        m_world->query<Transform, MeshRenderer>().for_each_with_entity([&](anxiety::ecs::EntityId id, Transform& t, MeshRenderer& mr) {
            if (!mr.vertex_buffer.is_valid() || !mr.index_buffer.is_valid() || mr.index_count == 0) return;

            // Resuelve la instancia de material (prioriza la de la entidad; si no, usa la de por defecto).
            materials::MaterialInstanceHandle mi_handle = mr.material_instance;
            materials::MaterialInstance* mat_inst = (m_materials && mi_handle.is_valid()) ? m_materials->get_instance(mi_handle) : nullptr;

            if (!mat_inst && m_materials && m_default_instance.is_valid()) mat_inst = m_materials->get_instance(m_default_instance);

            // Resuelve el pipeline a partir del material.
            materials::Material* mat = (m_materials && mat_inst) ? m_materials->get_material(mat_inst->material()) : nullptr;

            // Sin material disponible: se omite esta entidad.
            if (!mat || !mat->pipeline()) return;

            // Matriz de mundo: T * R * S (convención de vector columna).
            const Mat4 world_mat = mat4_TRS({ t.position[0], t.position[1], t.position[2] }, t.rotation[0], t.rotation[1], t.rotation[2], t.rotation[3], { t.scale[0],    t.scale[1],    t.scale[2] });

            // WVP = P * V * W
            const Mat4 wvp = mat4_mul(proj_mat, mat4_mul(view_mat, world_mat));

            // Garantiza que existen los constant buffers de GPU.
            PerEntityData& data = ensure_entity_data(id.index);
            m_device.write_buffer(data.wvp_buffer, wvp.data(), 0, sizeof(Mat4));

            const auto& p = mat_inst->params();
            m_device.write_buffer(data.mat_params_buffer, &p, 0, sizeof(materials::MaterialParams));

            // Reconstruye el descriptor set cuando cambia el material (y por tanto su layout de DS).
            refresh_entity_DS(data, mat, mat_inst->albedo_texture());

            drawList.push_back({
                .vertex_buffer  = mr.vertex_buffer,
                .index_buffer   = mr.index_buffer,
                .index_count    = mr.index_count,
                .vertex_stride  = mr.vertex_stride,
                .pipeline       = mat->pipeline(),
                .descriptor_set = data.descriptor_set.get(),
            });
        });

        // 3. ClearPass — siempre se emite cuando hay un world adjuntado --------------------------
        graph.add_pass({
            .name    = "ClearPass",
            .writes  = {{ bb_handle, rhi::ResourceState::RenderTarget }},
            .execute = [bb_physical, clear_color](rhi::ICommandBuffer& cmd) { cmd.clear_render_target(bb_physical, clear_color); }
        });

        // 4. ScenePass — solo cuando hay entidades que dibujar ----------------------------------
        if (!drawList.empty()) {
            graph.add_pass({
                .name    = "ScenePass",
                // El read crea la arista de orden desde ClearPass; el write marca la modificación del RT.
                .reads   = {{ bb_handle, rhi::ResourceState::RenderTarget }},
                .writes  = {{ bb_handle, rhi::ResourceState::RenderTarget }},
                .execute = [drawList = std::move(drawList)](rhi::ICommandBuffer& cmd) {
                    rhi::IPipeline* last_pipeline = nullptr;
                    for (const DrawCmd& d : drawList) {
                        if (d.pipeline != last_pipeline) {
                            cmd.bind_pipeline(*d.pipeline);
                            last_pipeline = d.pipeline;
                        }
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
