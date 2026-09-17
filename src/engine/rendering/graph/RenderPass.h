#pragma once

#include "ResourceHandle.h"
#include "../rhi/RHITypes.h"

#include <functional>
#include <string>
#include <vector>

namespace anxiety::rendering::rhi { class ICommandBuffer; }

namespace anxiety::rendering::graph {
    // RGTextureAccess ----------------------------------------------------------------------------
    // Empareja un handle de textura del grafo con el ResourceState en el que este pase usará la
    // textura. El RenderGraph usa estos estados declarados para insertar automáticamente las
    // llamadas correctas a resource_barrier() antes de invocar el pase.
    //
    //   entrada de reads:  state = el estado en el que el pase necesita leer (p. ej. ShaderResource)
    //   entrada de writes: state = el estado en el que el pase necesita escribir (p. ej. RenderTarget)
    // --------------------------------------------------------------------------------------------
    struct RGTextureAccess {
        RGTextureHandle    handle;
        rhi::ResourceState state = rhi::ResourceState::Undefined;
    };

    // RenderPassDesc -----------------------------------------------------------------------------
    // Describe un pase de renderizado dentro del grafo.
    //
    //   name          — identificador de depuración único, mostrado en logs / capturas de GPU.
    //   reads         — texturas del grafo que este pase muestrea/lee, con su estado requerido.
    //   writes        — texturas del grafo que este pase renderiza/escribe, con su estado requerido.
    //   read_buffers  — buffers del grafo que este pase lee.
    //   write_buffers — buffers del grafo que este pase escribe.
    //   execute       — callback invocado con un ICommandBuffer abierto durante RenderGraph::execute().
    //                   Los barriers ya han sido emitidos por el grafo antes de que este callback se
    //                   ejecute — NO los emitas aquí.
    //
    // Regla de dependencia:
    //   Un pase que escribe la textura T crea una arista de orden hacia cada pase posterior que lee T.
    //   Los pases sin dependencia de recursos se ejecutan en el orden de registro.
    // --------------------------------------------------------------------------------------------
    struct RenderPassDesc {
        std::string                                                    name;
        std::vector<RGTextureAccess>                                   reads         = {};
        std::vector<RGTextureAccess>                                   writes        = {};
        std::vector<RGBufferHandle>                                    read_buffers  = {};
        std::vector<RGBufferHandle>                                    write_buffers = {};
        std::function<void(anxiety::rendering::rhi::ICommandBuffer&)>  execute;
    };
} // namespace anxiety::rendering::graph