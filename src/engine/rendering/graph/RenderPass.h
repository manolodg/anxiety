#pragma once

#include "ResourceHandle.h"

#include <functional>
#include <string>
#include <vector>

namespace anxiety::rendering::rhi { class ICommandBuffer; }

namespace anxiety::rendering::graph {
    // RenderPassDesc -----------------------------------------------------------------------------
    // Describe un pase de renderizado dentro del grafo.
    //
    //   name          — identificador de depuración único, mostrado en logs / capturas de GPU.
    //   reads         — texturas del grafo que este pase muestrea / lee.
    //   writes        — texturas del grafo sobre las que este pase renderiza / escribe.
    //   read_buffers  — buffers del grafo que este pase lee.
    //   write_buffers — buffers del grafo que este pase escribe.
    //   execute       — callback invocado con un ICommandBuffer abierto durante RenderGraph::execute().
    //                   La referencia a ICommandBuffer solo es válida durante la llamada.
    //
    // Regla de dependencia:
    //   Un pase que escribe la textura T crea una arista de orden hacia cada pase posterior que lea T.
    //   Los pases sin dependencia de recursos se ejecutan en el orden en que se registraron.
    // --------------------------------------------------------------------------------------------
    struct RenderPassDesc {
        std::string                                                    name;
        std::vector<RGTextureHandle>                                   reads;
        std::vector<RGTextureHandle>                                   writes;
        std::vector<RGBufferHandle>                                    read_buffers;
        std::vector<RGBufferHandle>                                    write_buffers;
        std::function<void(anxiety::rendering::rhi::ICommandBuffer&)>  execute;
    };
} // namespace anxiety::rendering::graph