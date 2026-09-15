#pragma once

#include "RHITypes.h"

namespace anxiety::rendering::rhi {
    // IBuffer ------------------------------------------------------------------------------------
    // Interfaz extendida para buffers visibles desde la CPU (subida / lectura). La mayoría de los
    // usuarios interactúan con los buffers a través de BufferHandle + IDevice. Esta interfaz se
    // devuelve cuando se necesita mapeo directo desde la CPU.
    // --------------------------------------------------------------------------------------------
    class IBuffer {
    public:
        virtual ~IBuffer() = default;

        [[nodiscard]] virtual BufferHandle handle()     const noexcept = 0;
        [[nodiscard]] virtual uint64_t     size_bytes() const noexcept = 0;

        // Mapea el buffer para lectura/escritura desde la CPU. Solo válido para tipos de heap de subida/lectura.
        virtual void* map() = 0;
        virtual void  unmap() = 0;

    protected:
        IBuffer() = default;
        IBuffer(const IBuffer&) = delete;
        IBuffer& operator=(const IBuffer&) = delete;
    };
} // namespace anxiety::rendering::rhi
