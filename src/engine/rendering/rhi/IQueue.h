#pragma once

#include <cstdint>

namespace anxiety::rendering::rhi {
    class ICommandBuffer;

    // IQueue -------------------------------------------------------------------------------------
    // Representa una cola de comandos de GPU (gráficos / cómputo / copia). En la mayoría de los
    // backends es un detalle interno de IDevice. Se expone aquí para casos de uso avanzados (p. ej.
    // cómputo asíncrono, colas de copia).
    // --------------------------------------------------------------------------------------------
    class IQueue {
    public:
        virtual ~IQueue() = default;

        virtual void submit(ICommandBuffer& cmd) = 0;
        virtual void wait_idle() = 0;

        // Consulta la marca de tiempo monótona actual de la GPU (unidades definidas por la implementación).
        [[nodiscard]] virtual uint64_t timestamp() const noexcept = 0;

    protected:
        IQueue() = default;
        IQueue(const IQueue&) = delete;
        IQueue& operator=(const IQueue&) = delete;
    };
} // namespace anxiety::rendering::rhi