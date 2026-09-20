#include "MaterialManager.h"
#include "ShaderLoader.h"
#include "Logger.h"

#include "rhi/PipelineDesc.h"
#include "rhi/VertexLayout.h"

namespace anxiety::rendering::materials {
    static constexpr char k_category[] = "MaterialManager";

    // Construcción ------------------------------------------------------------------------------
    MaterialManager::MaterialManager(rhi::IDevice& device, std::string_view assets_dir) : m_device(device), m_assets_dir(assets_dir) {
        // Carga el material unlit incorporado de inmediato para que siempre esté disponible.
        m_unlit_handle = load_material("unlit", "shaders/unlit.hlsl");
        if (!m_unlit_handle.is_valid()) {
            LOGF_WARNING(k_category, "Falló la carga del material unlit por defecto (assets_dir='{}').", m_assets_dir);
        } else {
            LOGF_INFO(k_category, "MaterialManager en línea. Material unlit id={}.", m_unlit_handle.id);
        }
    }

    // Resolución de rutas ----------------------------------------------------------------------
    std::string MaterialManager::resolve_path(std::string_view path) const {
        // Se trata como absoluta cuando empieza por '/' o por "X:" (letra de unidad de Windows).
        if (!path.empty() && (path[0] == '/' || (path.size() > 1 && path[1] == ':'))) return std::string(path);
        if (m_assets_dir.empty()) return std::string(path);

        return m_assets_dir + "/" + std::string(path);
    }

    // load_material ----------------------------------------------------------------------------
    MaterialHandle MaterialManager::load_material(std::string_view name, std::string_view shader_path, std::string_view vs_entry, std::string_view ps_entry) {
        // Devuelve el handle cacheado cuando el nombre ya se cargó antes.
        const std::string name_str(name);
        auto it = m_name_cache.find(name_str);
        if (it != m_name_cache.end()) return it->second;

        const std::string full_path = resolve_path(shader_path);

        // Compila los shaders desde el fichero HLSL compartido.
        const std::string vs_entry_str(vs_entry), ps_entry_str(ps_entry);
        auto vs_bc = compile_from_file(m_device, full_path, vs_entry_str, rhi::ShaderStage::Vertex);
        auto ps_bc = compile_from_file(m_device, full_path, ps_entry_str, rhi::ShaderStage::Fragment);
        if (vs_bc.empty() || ps_bc.empty()) {
            LOGF_ERROR(k_category, "Falló la compilación de shaders del material '{}' (path='{}').", name_str, full_path);
            return {};
        }

        auto mat = std::make_unique<Material>();
        mat->m_name = name_str;

        mat->m_vs = m_device.create_shader({ vs_bc.data(), vs_bc.size(), vs_entry_str.c_str() }, rhi::ShaderStage::Vertex);
        mat->m_ps = m_device.create_shader({ ps_bc.data(), ps_bc.size(), ps_entry_str.c_str() }, rhi::ShaderStage::Fragment);
        if (!mat->m_vs || !mat->m_ps) {
            LOGF_ERROR(k_category, "Falló la creación de los objetos shader del material '{}'.", name_str);
            return {};
        }

        // Pipeline: dos cbuffers — b0 = PerObject (WVP), b1 = PerMaterial (params).
        rhi::PipelineDesc pd;
        pd.vertex_shader   = mat->m_vs.get();
        pd.fragment_shader = mat->m_ps.get();
        pd.vertex_layout   = {
            .attributes = {
                { "POSITION", 0, rhi::VertexFormat::Float3, 0,  0 },
                { "COLOR",    0, rhi::VertexFormat::Float4, 0, 12 },
            },
            .stride_bytes = 28,
        };
        pd.topology                  = rhi::PrimitiveTopology::TriangleList;
        pd.rasterizer.cull_mode      = rhi::CullMode::Back;
        pd.rasterizer.front_face_CCW = false;           // D3D12 LH: winding CW = cara frontal
        pd.render_target_fmts        = { rhi::Format::BGRA8_Unorm };
        pd.descriptor_layout    = { .bindings = {
            { 0, rhi::DescriptorType::UniformBuffer },  // b0: PerObject   (WVP)
            { 1, rhi::DescriptorType::UniformBuffer },  // b1: PerMaterial (base_color)
        } };
        pd.debug_name = name_str.c_str();

        mat->m_pipeline = m_device.create_pipeline(pd);
        if (!mat->m_pipeline) {
            LOGF_ERROR(k_category, "Falló la creación del pipeline del material '{}'.", name_str);
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
} // namespace anxiety::rendering::materials
