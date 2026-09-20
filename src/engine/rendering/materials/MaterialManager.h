#pragma once

#include "Material.h"
#include "MaterialInstance.h"
#include "rhi/IDevice.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// MaterialManager --------------------------------------------------------------------------------
// Factoría, propietario y caché de todos los objetos Material y MaterialInstance.
//
// En la construcción se carga el material incorporado "unlit" desde <assets_dir>/shaders/unlit.hlsl
// y queda disponible vía default_unlit(). Las siguientes llamadas a load_material() se cachean por
// nombre.
//
// Uso:
//   MaterialManager mgr(device);                               // carga el material unlit por defecto
//   MaterialHandle         mat  = mgr.default_unlit();
//   MaterialInstanceHandle red  = mgr.create_instance(mat);
//   mgr.set_base_color(red, 1.f, 0.f, 0.f, 1.f);
//   meshRenderer.material_instance = red;
// ------------------------------------------------------------------------------------------------

// Directorio de assets en tiempo de compilación (fijado vía CMake; puede sobrescribirse en runtime).
#ifndef ANXIETY_ASSETS_DIR
#  define ANXIETY_ASSETS_DIR ""
#endif

namespace anxiety::rendering::materials {
    class MaterialManager {
    public:
        // assets_dir toma por defecto el valor del define de compilación ANXIETY_ASSETS_DIR.
        explicit MaterialManager(rhi::IDevice& device, std::string_view assets_dir = ANXIETY_ASSETS_DIR);
        ~MaterialManager() = default;

        // Carga de materiales -----------------------------------------------------------------
        // Carga (o devuelve el cacheado) un material desde un único fichero HLSL que contiene ambos
        // entry points VS y PS. shader_path es relativo a assets_dir salvo que empiece por '/' o por
        // una letra de unidad de Windows.
        [[nodiscard]] MaterialHandle load_material(std::string_view name, std::string_view shader_path, std::string_view vs_entry = "VSMain", std::string_view ps_entry = "PSMain");

        // Gestión de instancias ---------------------------------------------------------------
        [[nodiscard]] MaterialInstanceHandle create_instance(MaterialHandle mat);

        // Accesores -------------------------------------------------------------------------------
        [[nodiscard]] Material*         get_material(MaterialHandle h)         const noexcept;
        [[nodiscard]] MaterialInstance* get_instance(MaterialInstanceHandle h) const noexcept;

        // Setter de conveniencia — equivalente a get_instance(h)->set_base_color(...).
        void set_base_color(MaterialInstanceHandle h, float r, float g, float b, float a) noexcept;

        // Handle del material unlit incorporado (cargado en la construcción). Inválido cuando el
        // directorio de assets está vacío o el fichero no existe.
        [[nodiscard]] MaterialHandle default_unlit() const noexcept { return m_unlit_handle; }

    private:
        [[nodiscard]] std::string resolve_path(std::string_view path) const;

        rhi::IDevice&   m_device;
        std::string     m_assets_dir;
        MaterialHandle  m_unlit_handle;

        std::vector<std::unique_ptr<Material>>          m_materials;  // indexado por id-1
        std::vector<std::unique_ptr<MaterialInstance>>  m_instances;  // indexado por id-1
        std::unordered_map<std::string, MaterialHandle> m_name_cache;
    };
} // namespace anxiety::rendering::materials
