#pragma once

#include <cstdint>

namespace anxiety::rendering::rhi {
    // Handles opacos de recursos de GPU ------------------------------------------------------------
    // Los handles son índices dentro de los pools de recursos del backend. id == 0 siempre es inválido.
    struct BufferHandle { uint64_t id = 0; [[nodiscard]] bool is_valid() const noexcept { return id != 0; } };
    struct TextureHandle { uint64_t id = 0; [[nodiscard]] bool is_valid() const noexcept { return id != 0; } };

    // Formatos de píxel ---------------------------------------------------------------------------
    enum class Format : uint32_t {
        Unknown,
        BGRA8_Unorm,
        RGBA8_Unorm,
        RGBA16_Float,
        D32_Float,
        D24_Unorm_S8_Uint
    };

    // Estados del recurso (usados para las barreras del pipeline) ---------------------------------
    enum class ResourceState : uint32_t {
        Undefined,
        Common,
        RenderTarget,
        Present,
        ShaderResource,
        UnorderedAccess,
        CopyDest,
        CopySrc,
        DepthWrite,
        DepthRead,
    };

    // Flags de uso del buffer (combinables) ---------------------------------------------------------
    enum class BufferUsage : uint32_t {
        None = 0,
        Vertex = 1 << 0,
        Index = 1 << 1,
        Uniform = 1 << 2,
        Storage = 1 << 3,
    };

    inline BufferUsage operator|(BufferUsage a, BufferUsage b)      noexcept { return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); }
    inline bool        has_flag(BufferUsage mask, BufferUsage flag) noexcept { return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(flag)) != 0; }

    // Descriptores --------------------------------------------------------------------------------
    struct Extent2D { uint32_t width = 0; uint32_t height = 0; };
    struct ClearColor { float r = 0.f; float g = 0.f; float b = 0.f; float a = 1.f; };

    struct BufferDesc {
        uint64_t    size_bytes = 0;
        BufferUsage usage = BufferUsage::None;
        const char* debug_name = nullptr;
    };

    struct TextureDesc {
        Extent2D    extent;
        Format      format = Format::Unknown;
        uint32_t    mip_levels = 1;
        uint32_t    array_size = 1;
        bool        is_render_target = false;
        bool        is_depth_target = false;
        const char* debug_name = nullptr;
    };

    struct SwapchainDesc {
        void* native_window_handle = nullptr;        // HWND en Win32, Window en X11
        Extent2D extent;
        uint32_t image_count = 2;
        Format   format = Format::BGRA8_Unorm;
        bool     vsync = true;
    };
} // namespace anxiety::rendering::rhi