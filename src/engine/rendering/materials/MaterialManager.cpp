#include "MaterialManager.h"
#include "ShaderLoader.h"
#include "Logger.h"

#include "rhi/PipelineDesc.h"
#include "rhi/SamplerDesc.h"
#include "rhi/VertexLayout.h"

namespace anxiety::rendering::materials {
    static constexpr char k_category[] = "MaterialManager";

    // Construcción ------------------------------------------------------------------------------
    MaterialManager::MaterialManager(rhi::IDevice& device, std::string_view assets_dir) : m_device(device), m_assets_dir(assets_dir) {
        // Carga el material unlit incorporado (vértice POSITION+COLOR, sin textura).
        m_unlit_handle = load_material("unlit", "shaders/unlit.hlsl");
        if (!m_unlit_handle.is_valid()) {
            LOGF_WARNING(k_category, "Falló la carga del material unlit por defecto (assets_dir='{}').", m_assets_dir);
        } else {
            LOGF_INFO(k_category, "MaterialManager en línea. Material unlit id={}.", m_unlit_handle.id);
        }

        // Material con textura incorporado (POSITION+COLOR+UV, textura t0, sampler s0).
        m_unlit_textured_handle = load_textured_material("unlit_textured", "shaders/unlit_textured.hlsl");
        if (!m_unlit_textured_handle.is_valid()) {
            LOGF_WARNING(k_category, "Falló la carga del material unlit_textured por defecto.");
        } else {
            LOGF_INFO(k_category, "Material unlit_textured cargado (id={}).", m_unlit_textured_handle.id);
        }

        // Material PBR incorporado (POSITION+NORMAL+UV+TANGENT, DS de 6 bindings, sampler s0).
        m_pbr_handle = load_PBR_material("pbr", "shaders/pbr.hlsl");
        if (!m_pbr_handle.is_valid()) {
            LOGF_WARNING(k_category, "Falló la carga del material PBR por defecto.");
        } else {
            LOGF_INFO(k_category, "Material PBR cargado (id={}).", m_pbr_handle.id);
        }
    }

    // Resolución de rutas ----------------------------------------------------------------------
    std::string MaterialManager::resolve_path(std::string_view path) const {
        // Se trata como absoluta cuando empieza por '/' o por "X:" (letra de unidad de Windows).
        if (!path.empty() && (path[0] == '/' || (path.size() > 1 && path[1] == ':'))) return std::string(path);
        if (m_assets_dir.empty()) return std::string(path);

        return m_assets_dir + "/" + std::string(path);
    }

    // load_material_internal - lógica compartida de creación del PSO ---------------------------
    MaterialHandle MaterialManager::load_material_internal(std::string_view name, std::string_view shader_path, std::string_view vs_entry, std::string_view ps_entry, const rhi::DescriptorSetLayout& ds_layout, const rhi::VertexLayout& vertex_layout, const std::vector<rhi::SamplerDesc>& samplers) {
        // Caché por nombre — devuelve el handle existente si ya se cargó.
        const std::string name_str(name);
        auto it = m_name_cache.find(name_str);
        if (it != m_name_cache.end()) return it->second;

        const std::string full_path = resolve_path(shader_path);
        const std::string vs_entry_S(vs_entry), ps_entry_S(ps_entry);

        // Compila los shaders desde el fichero HLSL compartido.
        auto vs_bc = compile_from_file(m_device, full_path, vs_entry_S, rhi::ShaderStage::Vertex);
        auto ps_bc = compile_from_file(m_device, full_path, ps_entry_S, rhi::ShaderStage::Fragment);
        if (vs_bc.empty() || ps_bc.empty()) {
            LOGF_ERROR(k_category, "Falló la compilación de shaders del material '{}' (path='{}').", name_str, full_path);
            return {};
        }

        auto mat = std::make_unique<Material>();
        mat->m_name = name_str;

        mat->m_vs = m_device.create_shader({ vs_bc.data(), vs_bc.size(), vs_entry_S.c_str() }, rhi::ShaderStage::Vertex);
        mat->m_ps = m_device.create_shader({ ps_bc.data(), ps_bc.size(), ps_entry_S.c_str() }, rhi::ShaderStage::Fragment);
        if (!mat->m_vs || !mat->m_ps) {
            LOGF_ERROR(k_category, "Falló la creación de los objetos shader de '{}'.", name_str);
            return {};
        }

        rhi::PipelineDesc pd;
        pd.vertex_shader             = mat->m_vs.get();
        pd.fragment_shader           = mat->m_ps.get();
        pd.vertex_layout             = vertex_layout;
        pd.topology                  = rhi::PrimitiveTopology::TriangleList;
        pd.rasterizer.cull_mode      = rhi::CullMode::Back;
        pd.rasterizer.front_face_CCW = false;           // D3D12 LH: winding CW = cara frontal
        pd.render_target_fmts        = { rhi::Format::BGRA8_Unorm };
        pd.descriptor_layout         = ds_layout;
        pd.static_samplers           = samplers;
        pd.debug_name                = name_str.c_str();

        mat->m_ds_layout = ds_layout;
        mat->m_pipeline = m_device.create_pipeline(pd);
        if (!mat->m_pipeline) {
            LOGF_ERROR(k_category, "Falló la creación del pipeline de '{}'.", name_str);
            return {};
        }

        const uint32_t id = static_cast<uint32_t>(m_materials.size()) + 1;
        mat->m_handle = { id };
        const MaterialHandle h{ id };
        m_name_cache.emplace(name_str, h);
        m_materials.push_back(std::move(mat));

        LOGF_INFO(k_category, "Material '{}' cargado (id={}, {} bytes VS, {} bytes PS).", name_str, id, vs_bc.size(), ps_bc.size());
        return h;
    }

    // load_material — unlit de 2 bindings (sin textura) ---------------------------------------
    MaterialHandle MaterialManager::load_material(std::string_view name, std::string_view shader_path, std::string_view vs_entry, std::string_view ps_entry) {
        rhi::DescriptorSetLayout ds_layout{ .bindings = {
            { 0, rhi::DescriptorType::UniformBuffer },              // b0 : PerObject   (WVP)
            { 1, rhi::DescriptorType::UniformBuffer }               // b1 : PerMaterial (base_color)
        }};
        rhi::VertexLayout vl;
        vl.attributes = {
            { "POSITION", 0, rhi::VertexFormat::Float3, 0,  0 },
            { "COLOR",    0, rhi::VertexFormat::Float4, 0, 12 },
        };
        vl.stride_bytes = 28u;
        return load_material_internal(name, shader_path, vs_entry, ps_entry, ds_layout, vl, {});
    }
    // load_textured_material — 3 bindings (b0 + b1 + t0) con sampler estático s0 ---------------
    MaterialHandle MaterialManager::load_textured_material(std::string_view name, std::string_view shader_path, std::string_view vs_entry, std::string_view ps_entry) {
        rhi::DescriptorSetLayout ds_layout{ .bindings = {
            { 0, rhi::DescriptorType::UniformBuffer },              // b0: PerObject   (WVP)
            { 1, rhi::DescriptorType::UniformBuffer },              // b1: PerMaterial (base_color + use_texture)
            { 0, rhi::DescriptorType::Texture       },              // t0: textura de albedo
        } };
        rhi::VertexLayout vl;
        vl.attributes = {
            { "POSITION", 0, rhi::VertexFormat::Float3, 0,  0 },
            { "COLOR",    0, rhi::VertexFormat::Float4, 0, 12 },
            { "TEXCOORD", 0, rhi::VertexFormat::Float2, 0, 28 },
        };
        vl.stride_bytes = 36u;
        const std::vector<rhi::SamplerDesc> samplers = { rhi::sampler_linear_wrap(0) };
        return load_material_internal(name, shader_path, vs_entry, ps_entry, ds_layout, vl, samplers);
    }

    // load_PBR_material — 6-binding PBR (b0+b1+b2+t0+t1+t2), stride 48 ---------------------------
    MaterialHandle MaterialManager::load_PBR_material(std::string_view name, std::string_view shader_path, std::string_view vs_entry, std::string_view ps_entry) {
        rhi::DescriptorSetLayout dsLayout{ .bindings = {
            { 0, rhi::DescriptorType::UniformBuffer },              // b0: PerObject  (WVP + worldMat)
            { 1, rhi::DescriptorType::UniformBuffer },              // b1: PerMaterial (PBR params)
            { 2, rhi::DescriptorType::UniformBuffer },              // b2: LightsCB
            { 0, rhi::DescriptorType::Texture       },              // t0: albedo
            { 1, rhi::DescriptorType::Texture       },              // t1: normal map
            { 2, rhi::DescriptorType::Texture       },              // t2: ORM
        } };
        rhi::VertexLayout vl;
        vl.attributes = {
            { "POSITION", 0, rhi::VertexFormat::Float3, 0,  0 },
            { "NORMAL",   0, rhi::VertexFormat::Float3, 0, 12 },
            { "TEXCOORD", 0, rhi::VertexFormat::Float2, 0, 24 },
            { "TANGENT",  0, rhi::VertexFormat::Float4, 0, 32 },
        };
        vl.stride_bytes = 48u;
        const std::vector<rhi::SamplerDesc> samplers = { rhi::sampler_linear_wrap(0) };
        return load_material_internal(name, shader_path, vs_entry, ps_entry, dsLayout, vl, samplers);
    }

    // Gestión de instancias ------------------------------------------------------------------------
    MaterialInstanceHandle MaterialManager::create_instance(MaterialHandle mat) {
        if (!get_material(mat)) {
            LOG_WARNING(k_category, "create_instance: handle de material inválido o desconocido.");
            return {};
        }
        auto inst = std::make_unique<MaterialInstance>();
        inst->m_material = mat;
        const uint32_t id = static_cast<uint32_t>(m_instances.size()) + 1;
        m_instances.push_back(std::move(inst));
        return { id };
    }

    // Accesores ----------------------------------------------------------------------------------
    Material* MaterialManager::get_material(MaterialHandle h) const noexcept {
        if (!h.is_valid() || h.id > static_cast<uint32_t>(m_materials.size())) return nullptr;
        return m_materials[h.id - 1].get();
    }

    MaterialInstance* MaterialManager::get_instance(MaterialInstanceHandle h) const noexcept {
        if (!h.is_valid() || h.id > static_cast<uint32_t>(m_instances.size())) return nullptr;
        return m_instances[h.id - 1].get();
    }

    void MaterialManager::set_base_color(MaterialInstanceHandle h, float r, float g, float b, float a) noexcept {
        if (auto* inst = get_instance(h)) inst->set_base_color(r, g, b, a);
    }

    void MaterialManager::set_albedo_texture(MaterialInstanceHandle h, rhi::TextureHandle texture) noexcept {
        if (auto* inst = get_instance(h)) inst->set_albedo_texture(texture);
    }

    void MaterialManager::set_metallic(MaterialInstanceHandle h, float v) noexcept {
        if (auto* inst = get_instance(h)) inst->set_metallic(v);
    }

    void MaterialManager::set_roughness(MaterialInstanceHandle h, float v) noexcept {
        if (auto* inst = get_instance(h)) inst->set_roughness(v);
    }

    void MaterialManager::set_emissive(MaterialInstanceHandle h, float r, float g, float b) noexcept {
        if (auto* inst = get_instance(h)) inst->set_emissive(r, g, b);
    }

    void MaterialManager::set_normal_texture(MaterialInstanceHandle h, rhi::TextureHandle texture) noexcept {
        if (auto* inst = get_instance(h)) inst->set_normal_texture(texture);
    }

    void MaterialManager::set_orm_texture(MaterialInstanceHandle h, rhi::TextureHandle texture) noexcept {
        if (auto* inst = get_instance(h)) inst->set_orm_texture(texture);
    }
} // namespace anxiety::rendering::materials
