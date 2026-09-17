#pragma once

#include <string_view>

namespace anxiety::rendering::rhi {
    // IPipeline ----------------------------------------------------------------------------------
    // Objeto de estado de pipeline gráfico (PSO), inmutable. Creado por IDevice::create_pipeline(PipelineDesc).
    // Se vincula con ICommandBuffer::bind_pipeline() antes de emitir draw calls.
    //
    // Los pipelines son inmutables tras su creación. Para cambiar el estado del pipeline, crea uno
    // nuevo y vincúlalo.
    class IPipeline {
    public:
        virtual ~IPipeline() = default;

        // Etiqueta de depuración opcional (de PipelineDesc::debug_name).
        [[nodiscard]] virtual std::string_view debug_name() const noexcept = 0;

    protected:
        IPipeline() = default;
        IPipeline(const IPipeline&) = delete;
        IPipeline& operator=(const IPipeline&) = delete;
    };
} // namespace anxiety::rendering::rhi