#include "SceneRenderer.h"
#include "textures/TextureManager.h"
#include "Logger.h"

#include <algorithm>
#include <cstring>

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
            if (data.per_object_buffer.is_valid()) m_device.destroy_buffer(data.per_object_buffer);
            if (data.mat_params_buffer.is_valid()) m_device.destroy_buffer(data.mat_params_buffer);
        }

        if (m_lights_buffer.is_valid()) m_device.destroy_buffer(m_lights_buffer);
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
        if (data.per_object_buffer.is_valid()) return data;

        // CB de 256 bytes para b0: GpuPerObject (128 bytes usados)
        data.per_object_buffer = m_device.create_buffer({ 256, rhi::BufferUsage::Uniform, "ScenePerObj" });
        // CB de 256 bytes para b1: MaterialParams (64 bytes usados)
        data.mat_params_buffer = m_device.create_buffer({ 256, rhi::BufferUsage::Uniform, "SceneMat" });

        return data;
    }

    void SceneRenderer::refresh_entity_DS(PerEntityData& data, materials::Material* mat, const materials::MaterialInstance* inst) {
        if (data.last_material == mat->handle() && data.descriptor_set) return;

        // Recrea el descriptor set con el layout del material.
        data.descriptor_set = m_device.create_descriptor_set(mat->descriptor_layout());

        std::vector<rhi::DescriptorWrite> writes = {
            { 0, rhi::DescriptorType::UniformBuffer, data.per_object_buffer },
            { 1, rhi::DescriptorType::UniformBuffer, data.mat_params_buffer }
        };

        // Vincula el buffer de luces en b2 si el layout tiene un tercer binding de UB.
        bool has_lights_binding = false;
        int  tex_binding_count  = 0;

        for (const auto& b : mat->descriptor_layout().bindings) {
            if (b.type == rhi::DescriptorType::UniformBuffer && b.binding == 2) {
                has_lights_binding = true;
                if (m_lights_buffer.is_valid()) writes.push_back({ 2, rhi::DescriptorType::UniformBuffer, m_lights_buffer });
            }
            if (b.type == rhi::DescriptorType::Texture) ++tex_binding_count;
        }

        (void)has_lights_binding;

        // Vincula las texturas en orden de slot (t0=albedo, t1=normal, t2=ORM).
        if (tex_binding_count > 0) {
            rhi::TextureHandle nullTex = m_textures ? m_textures->null_texture() : rhi::TextureHandle{};

            auto tex_or_null = [&](rhi::TextureHandle h) -> rhi::TextureHandle { return h.is_valid() ? h : nullTex; };

            rhi::TextureHandle albedo = inst ? tex_or_null(inst->albedo_texture()) : nullTex;
            rhi::TextureHandle normal = inst ? tex_or_null(inst->normal_texture()) : nullTex;
            rhi::TextureHandle orm    = inst ? tex_or_null(inst->orm_texture())    : nullTex;

            int texSlot = 0;
            for (const auto& b : mat->descriptor_layout().bindings) {
                if (b.type != rhi::DescriptorType::Texture) continue;

                rhi::TextureHandle tex = nullTex;
                if (texSlot == 0) {
                    tex = albedo;
                } else if (texSlot == 1) {
                    tex = normal;
                } else if (texSlot == 2) {
                    tex = orm;
                }

                if (tex.is_valid()) writes.push_back({ b.binding, rhi::DescriptorType::Texture, rhi::BufferHandle{}, tex });

                ++texSlot;
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
        // Crea de forma perezosa el constant buffer de luces compartido.
        if (!m_lights_buffer.is_valid()) m_lights_buffer = m_device.create_buffer({ 1024, rhi::BufferUsage::Uniform, "LightsCB" });

        // 1. Vista-proyección a partir de la primera entidad Camera+Transform --------------------
        Mat4 view_mat   = mat4_identity();
        Mat4 proj_mat   = mat4_identity();
        Vec3 camera_pos = { 0.0f, 0.0f, 0.0f };

        m_world->query<Transform, Camera>().for_each([&, found = false](Transform& t, Camera& cam) mutable {
            if (found) return;
            found = true;

            camera_pos   = { t.position[0], t.position[1], t.position[2] };
            Vec3 forward = quat_rotate(t.rotation[0], t.rotation[1], t.rotation[2], t.rotation[3], { 0.0f, 0.0f, 1.0f });
            Vec3 center  = vec3_add(camera_pos, forward);

            view_mat = mat4_look_at_LH(camera_pos, center, { 0.f, 1.f, 0.f });

            const float ar = (aspect_ratio > 0.0f) ? aspect_ratio : cam.aspect_ratio;
            proj_mat = mat4_perspective_LH(cam.fov_Y, ar, cam.near_Z, cam.far_Z);
        });

        // 2. Recolecta las luces + escribe LightsCB ------------------------------------------------
        GpuLightsCB lightsCB{};

        // Luz direccional por defecto — apunta recto hacia abajo, muy tenue, para que las escenas
        // sin ninguna entidad de luz no queden completamente negras.
        lightsCB.dir_direction[0] =  0.0f;
        lightsCB.dir_direction[1] = -1.0f;
        lightsCB.dir_direction[2] =  0.0f;
        lightsCB.dir_intensity    =  0.05f;
        lightsCB.dir_color[0]     =  1.0f;
        lightsCB.dir_color[1]     =  1.0f;
        lightsCB.dir_color[2]     =  1.0f;

        // Sobrescribe con la primera entidad DirectionalLight que se encuentre.
        m_world->query<DirectionalLight>().for_each( [&, found = false](DirectionalLight& dl) mutable {
            if (found) return;

            found = true;
            lightsCB.dir_direction[0] = dl.direction[0];
            lightsCB.dir_direction[1] = dl.direction[1];
            lightsCB.dir_direction[2] = dl.direction[2];
            lightsCB.dir_intensity    = dl.intensity;
            lightsCB.dir_color[0]     = dl.color[0];
            lightsCB.dir_color[1]     = dl.color[1];
            lightsCB.dir_color[2]     = dl.color[2];
        });

        // Recolecta las luces puntuales (hasta k_max_point_lights).
        m_world->query<Transform, PointLight>().for_each( [&](Transform& t, PointLight& pl) {
            if (lightsCB.num_point_lights >= k_max_point_lights) return;

            GpuPointLight& gpl = lightsCB.point_lights[lightsCB.num_point_lights++];
            gpl.position[0] = t.position[0];
            gpl.position[1] = t.position[1];
            gpl.position[2] = t.position[2];
            gpl.intensity   = pl.intensity;
            gpl.color[0]    = pl.color[0];
            gpl.color[1]    = pl.color[1];
            gpl.color[2]    = pl.color[2];
            gpl.range       = pl.range;
        });

        lightsCB.camera_pos[0] = camera_pos.x;
        lightsCB.camera_pos[1] = camera_pos.y;
        lightsCB.camera_pos[2] = camera_pos.z;

        m_device.write_buffer(m_lights_buffer, &lightsCB, 0, sizeof(GpuLightsCB));

        // 3. Construye la lista de dibujado a partir de las entidades con Transform + MeshRenderer -
        std::vector<DrawCmd> drawList;

        m_world->query<Transform, MeshRenderer>().for_each_with_entity([&](ecs::EntityId id, Transform& t, MeshRenderer& mr) {
            if (!mr.vertex_buffer.is_valid() || !mr.index_buffer.is_valid() || mr.index_count == 0) return;

            // Resuelve la instancia de material.
            materials::MaterialInstanceHandle mi_handle = mr.material_instance;
            materials::MaterialInstance* mat_inst = (m_materials && mi_handle.is_valid()) ? m_materials->get_instance(mi_handle) : nullptr;

            if (!mat_inst && m_materials && m_default_instance.is_valid()) mat_inst = m_materials->get_instance(m_default_instance);

            // Resuelve el pipeline a partir del material.
            materials::Material* mat = (m_materials && mat_inst) ? m_materials->get_material(mat_inst->material()) : nullptr;

            if (!mat || !mat->pipeline()) return;

            // World matrix: T * R * S
            const Mat4 world_mat = mat4_TRS({ t.position[0], t.position[1], t.position[2] }, t.rotation[0], t.rotation[1], t.rotation[2], t.rotation[3], { t.scale[0],    t.scale[1],    t.scale[2] });

            // WVP = P * V * W
            const Mat4 wvp = mat4_mul(proj_mat, mat4_mul(view_mat, world_mat));

            // Escribe GpuPerObject (WVP + world_matrix) en b0.
            GpuPerObject per_obj{};
            std::memcpy(per_obj.world_view_proj, wvp.data(), sizeof(float) * 16);
            std::memcpy(per_obj.world_matrix, world_mat.data(), sizeof(float) * 16);

            PerEntityData& data = ensure_entity_data(id.index);
            m_device.write_buffer(data.per_object_buffer, &per_obj, 0, sizeof(GpuPerObject));

            const auto& p = mat_inst->params();
            m_device.write_buffer(data.mat_params_buffer, &p, 0, sizeof(materials::MaterialParams));

            // Reconstruye el descriptor set cuando el material (y por tanto su layout) cambia.
            refresh_entity_DS(data, mat, mat_inst);

            drawList.push_back({
                .vertex_buffer  = mr.vertex_buffer,
                .index_buffer   = mr.index_buffer,
                .index_count    = mr.index_count,
                .vertex_stride  = mr.vertex_stride,
                .pipeline       = mat->pipeline(),
                .descriptor_set = data.descriptor_set.get(),
            });
        });

        // 4. ClearPass ---------------------------------------------------------------------------
        graph.add_pass({
            .name    = "ClearPass",
            .writes  = {{ bb_handle, rhi::ResourceState::RenderTarget }},
            .execute = [bb_physical, clear_color](rhi::ICommandBuffer& cmd) { cmd.clear_render_target(bb_physical, clear_color); }
        });

        // 5. ScenePass ---------------------------------------------------------------------------
        if (!drawList.empty()) {
            graph.add_pass({
                .name    = "ScenePass",
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
