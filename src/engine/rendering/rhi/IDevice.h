#pragma once

#include "RHITypes.h"
#include "ShaderTypes.h"
#include "PipelineDesc.h"
#include "IPipeline.h"
#include "IDescriptorSet.h"

#include <memory>
#include <string_view>
#include <vector>

namespace anxiety::rendering::rhi {
    class ICommandBuffer;
    class ISwapchain;

    // IDevice ------------------------------------------------------------------------------------
    // Handle central del dispositivo de GPU. Es propietario del ciclo de vida de todos los recursos
    // de GPU. Un IDevice por cada adaptador de GPU físico (o virtual).
    //
    // Esta interfaz no expone ningún tipo específico de una API concreta.
    // --------------------------------------------------------------------------------------------
    class IDevice {
    public:
        virtual ~IDevice() = default;

        [[nodiscard]] virtual std::string_view backend_name() const noexcept = 0;

        // Creación de buffers / texturas -----------------------------------------------------------
        // create_buffer: reserva un buffer de GPU. Si initial_data no es nulo, los bytes dados se
        // suben antes de devolver el handle.
        [[nodiscard]] virtual BufferHandle  create_buffer(const BufferDesc& desc, const void* initial_data = nullptr, size_t initial_data_sz = 0) = 0;
        [[nodiscard]] virtual TextureHandle create_texture(const TextureDesc&)                                                                    = 0;
        virtual void destroy_buffer(BufferHandle)   = 0;
        virtual void destroy_texture(TextureHandle) = 0;

        // Escribe bytes en un buffer existente (CPU → GPU vía heap UPLOAD). Se usa para actualizar
        // constant buffers por fotograma.
        virtual void write_buffer(BufferHandle handle, const void* data, size_t offset, size_t size) = 0;

        // Sube datos de píxeles RGBA8 empaquetados a una textura 2D existente. La textura debe
        // haberse creado con Format::RGBA8_Unorm. Síncrono — solo retorna cuando la copia en GPU ha
        // terminado.
        virtual void upload_texture_data(TextureHandle handle, const uint8_t* rgba8, uint32_t width, uint32_t height) = 0;

        // Compilación de shaders -------------------------------------------------------------------
        // Compila un shader a partir de su código fuente y devuelve el bytecode nativo del backend
        // (DXIL en D3D12, SPIR-V en Vulkan). Devuelve un vector vacío si falla.
        [[nodiscard]] virtual std::vector<uint8_t> compile_shader_from_source(const char* source, const char* entry_point, ShaderStage stage) = 0;
        // Crea un objeto shader inmutable a partir de bytecode ya compilado.
        [[nodiscard]] virtual std::unique_ptr<IShader> create_shader(const ShaderDesc& desc, ShaderStage stage) = 0;

        // Estado de pipeline -------------------------------------------------------------------------
        // Crea un objeto de estado de pipeline gráfico (PSO) inmutable.
        [[nodiscard]] virtual std::unique_ptr<IPipeline> create_pipeline(const PipelineDesc& desc) = 0;

        // Descriptor sets ------------------------------------------------------------------------
        [[nodiscard]] virtual std::unique_ptr<IDescriptorSet> create_descriptor_set(const DescriptorSetLayout& layout) = 0;

        // Grabación de comandos -------------------------------------------------------------------
        [[nodiscard]] virtual std::unique_ptr<ICommandBuffer> create_command_buffer() = 0;

        // Swapchain ------------------------------------------------------------------------------
        // Devuelve nullptr si el backend no soporta presentación (modo headless).
        [[nodiscard]] virtual std::unique_ptr<ISwapchain> create_swapchain(const SwapchainDesc&) = 0;

        // Envío y sincronización -------------------------------------------------------------------
        virtual void submit(ICommandBuffer&) = 0;

        // Bloquea el hilo que llama hasta que todo el trabajo de GPU en curso haya terminado.
        virtual void wait_idle() = 0;

    protected:
        IDevice() = default;
        IDevice(const IDevice&) = delete;
        IDevice& operator=(const IDevice&) = delete;
    };
} // namespace anxiety::rendering::rhi