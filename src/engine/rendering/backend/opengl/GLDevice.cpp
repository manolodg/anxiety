#ifdef ANXIETY_BACKEND_OPENGL

#include "GLDevice.h"
#include "GLCommandBuffer.h"
#include "GLSwapchain.h"
#include "GLShader.h"
#include "GLPipeline.h"
#include "GLDescriptorSet.h"
#include "Logger.h"
#include "../../shader/HlslCompiler.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <string>

#ifdef ANXIETY_HAVE_SPIRV_CROSS
#include <spirv_glsl.hpp>
#endif

#if defined(__APPLE__)
#import <AppKit/AppKit.h>
#endif

// GLX solo se conecta para OpenGL de escritorio en Linux (no para GLES / Raspberry Pi).
#if defined(__linux__) && !defined(__ANDROID__) && !defined(ANXIETY_PLATFORM_RPI)
#define ANXIETY_USE_GLX 1
#endif

#if ANXIETY_USE_GLX
#include <X11/Xlib.h>

using GLXFBConfig = struct __GLXFBConfigRec*;
using GLXContext  = struct __GLXcontextRec*;

extern "C" {
    GLXFBConfig* glXChooseFBConfig(Display* dpy, int screen, const int* attrib_list, int* nelements);
    int          glXGetFBConfigAttrib(Display* dpy, GLXFBConfig config, int attribute, int* value);
    GLXContext   glXCreateNewContext(Display* dpy, GLXFBConfig config, int render_type, GLXContext share_list, int direct);
    int          glXMakeCurrent(Display* dpy, unsigned long drawable, GLXContext ctx);
    void         glXDestroyContext(Display* dpy, GLXContext ctx);
    void*        glXGetProcAddressARB(const unsigned char* proc_name);
}

static constexpr int GLX_RENDER_TYPE  = 0x8011;
static constexpr int GLX_RGBA_BIT     = 0x00000001;
static constexpr int GLX_RGBA_TYPE    = 0x8014;
static constexpr int GLX_VISUAL_ID    = 0x800B;
static constexpr int GLX_DOUBLEBUFFER = 5;
#endif

namespace anxiety::rendering::backend::opengl {

    static constexpr char k_category[] = "GLDevice";

    // Callback de depuración -------------------------------------------------------------------------
    /*static*/ void GLAPIENTRY GLDevice::gl_debug_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei /*length*/, const GLchar* message, const void* /*user_param*/) {
        if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

        const char* src_str  = "unknown";
        const char* type_str = "unknown";

        switch (source) {
        case GL_DEBUG_SOURCE_API:             src_str = "API";             break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   src_str = "WindowSystem";    break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: src_str = "ShaderCompiler";  break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     src_str = "ThirdParty";      break;
        case GL_DEBUG_SOURCE_APPLICATION:     src_str = "Application";     break;
        default: break;
        }

        switch (type) {
        case GL_DEBUG_TYPE_ERROR:               type_str = "ERROR";               break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type_str = "DEPRECATED_BEHAVIOR"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  type_str = "UNDEFINED_BEHAVIOR";  break;
        case GL_DEBUG_TYPE_PORTABILITY:         type_str = "PORTABILITY";         break;
        case GL_DEBUG_TYPE_PERFORMANCE:         type_str = "PERFORMANCE";         break;
        default: break;
        }

        if (severity == GL_DEBUG_SEVERITY_HIGH || type == GL_DEBUG_TYPE_ERROR) {
            LOGF_ERROR("GLDebug", "[{}][{}] id={} : {}", src_str, type_str, id, message);
        } else if (severity == GL_DEBUG_SEVERITY_MEDIUM) {
            LOGF_WARNING("GLDebug", "[{}][{}] id={} : {}", src_str, type_str, id, message);
        } else {
            LOGF_INFO("GLDebug", "[{}][{}] id={} : {}", src_str, type_str, id, message);
        }
    }

    // Constructor / destructor -------------------------------------------------------------------
    GLDevice::GLDevice(bool enable_validation, [[maybe_unused]] void* native_window) {
    #if defined(_WIN32)
        if (native_window) {
            HWND  hwnd = static_cast<HWND>(native_window);
            HDC   hdc  = ::GetDC(hwnd);

            PIXELFORMATDESCRIPTOR pfd{};
            pfd.nSize        = sizeof(pfd);
            pfd.nVersion     = 1;
            pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
            pfd.iPixelType   = PFD_TYPE_RGBA;
            pfd.cColorBits   = 32;
            pfd.cDepthBits   = 24;
            pfd.cStencilBits = 8;
            pfd.iLayerType   = PFD_MAIN_PLANE;

            const int pf = ::ChoosePixelFormat(hdc, &pfd);
            if (!pf || !::SetPixelFormat(hdc, pf, &pfd)) {
                LOGF_ERROR(k_category, "SetPixelFormat falló (err=0x{:08X}).", static_cast<unsigned>(::GetLastError()));
                ::ReleaseDC(hwnd, hdc);
                return;
            }

            HGLRC legacy = ::wglCreateContext(hdc);
            if (!legacy) {
                LOGF_ERROR(k_category, "wglCreateContext falló (err=0x{:08X}).", static_cast<unsigned>(::GetLastError()));
                ::ReleaseDC(hwnd, hdc);
                return;
            }
            ::wglMakeCurrent(hdc, legacy);

            using PFN_wglCreateContextAttribsARB = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
            auto wglCreateContextAttribsARB = reinterpret_cast<PFN_wglCreateContextAttribsARB>(
                ::wglGetProcAddress("wglCreateContextAttribsARB"));

            HGLRC core_ctx = nullptr;
            if (wglCreateContextAttribsARB) {
                const int attribs[] = { 0x2091, 4, 0x2092, 5, 0x9126, 0x1, 0 };
                core_ctx = wglCreateContextAttribsARB(hdc, nullptr, attribs);
            }

            if (core_ctx) {
                ::wglMakeCurrent(hdc, core_ctx);
                ::wglDeleteContext(legacy);
                m_hglrc = core_ctx;
            } else {
                LOG_WARNING(k_category, "wglCreateContextAttribsARB no disponible; se usa el contexto de GL heredado.");
                m_hglrc = legacy;
            }
            m_hdc  = hdc;
            m_hwnd = hwnd;
        }
    #elif defined(__APPLE__)
        if (native_window) {
            @autoreleasepool {
                NSWindow* win  = (__bridge NSWindow*)native_window;
                NSView*   view = [win contentView];

                NSOpenGLPixelFormatAttribute attrs[] = {
                    NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
                    NSOpenGLPFAColorSize,     24,
                    NSOpenGLPFAAlphaSize,     8,
                    NSOpenGLPFADepthSize,     24,
                    NSOpenGLPFAStencilSize,   8,
                    NSOpenGLPFADoubleBuffer,
                    NSOpenGLPFAAccelerated,
                    0
                };

                NSOpenGLPixelFormat* pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
                if (!pf) { LOG_ERROR(k_category, "Falló la creación de NSOpenGLPixelFormat."); return; }

                NSOpenGLContext* ctx = [[NSOpenGLContext alloc] initWithFormat:pf shareContext:nil];
                if (!ctx) { LOG_ERROR(k_category, "Falló la creación de NSOpenGLContext."); return; }

                [view setWantsBestResolutionOpenGLSurface:YES];
                [ctx setView:view];
                [ctx makeCurrentContext];

                m_nsgl_ctx = (__bridge_retained void*)ctx;
            }
        }
    #elif defined(ANXIETY_USE_GLX)
        if (native_window) {
            auto* display = XOpenDisplay(nullptr);
            if (!display) { LOG_ERROR(k_category, "XOpenDisplay() falló."); return; }

            const auto window = static_cast<Window>(reinterpret_cast<uintptr_t>(native_window));
            const int  screen  = DefaultScreen(display);

            XWindowAttributes wattr{};
            XGetWindowAttributes(display, window, &wattr);
            const VisualID desired_visual_id = XVisualIDFromVisual(wattr.visual);

            static const int fb_attribs[] = { GLX_RENDER_TYPE, GLX_RGBA_BIT, GLX_DOUBLEBUFFER, 1, None };

            int fbcount = 0;
            GLXFBConfig* configs = glXChooseFBConfig(display, screen, fb_attribs, &fbcount);
            if (!configs || fbcount == 0) {
                LOG_ERROR(k_category, "glXChooseFBConfig() no devolvió ninguna configuración.");
                XCloseDisplay(display);
                return;
            }

            GLXFBConfig chosen = configs[0];
            for (int i = 0; i < fbcount; ++i) {
                int visual_id = 0;
                glXGetFBConfigAttrib(display, configs[i], GLX_VISUAL_ID, &visual_id);
                if (static_cast<VisualID>(visual_id) == desired_visual_id) { chosen = configs[i]; break; }
            }
            XFree(configs);

            using PFNGLXCREATECONTEXTATTRIBSARBPROC = GLXContext(*)(Display*, GLXFBConfig, GLXContext, int, const int*);
            auto glXCreateContextAttribsARB = reinterpret_cast<PFNGLXCREATECONTEXTATTRIBSARBPROC>(glXGetProcAddressARB(reinterpret_cast<const unsigned char*>("glXCreateContextAttribsARB")));

            GLXContext ctx = nullptr;
            if (glXCreateContextAttribsARB) {
                const int ctx_attribs[] = { 0x2091, 4, 0x2092, 5, 0x9126, 0x1, None };
                ctx = glXCreateContextAttribsARB(display, chosen, nullptr, 1, ctx_attribs);
            }
            if (!ctx) {
                LOG_WARNING(k_category, "glXCreateContextAttribsARB falló; se recurre a glXCreateNewContext.");
                ctx = glXCreateNewContext(display, chosen, GLX_RGBA_TYPE, nullptr, 1);
            }
            if (!ctx) { LOG_ERROR(k_category, "No se pudo crear el contexto GLX."); XCloseDisplay(display); return; }

            if (!glXMakeCurrent(display, window, ctx)) {
                LOG_ERROR(k_category, "glXMakeCurrent() falló.");
                glXDestroyContext(display, ctx);
                XCloseDisplay(display);
                return;
            }

            m_display     = display;
            m_glx_ctx     = ctx;
            m_own_display = true;
        }
    #endif

        if (!gladLoadGL()) {
            LOG_ERROR(k_category, "gladLoadGL() falló — no hay contexto de OpenGL actual.");
            return;
        }

        init_glad(enable_validation);
    }

    void GLDevice::init_glad(bool enable_validation) {
        const GLubyte* version_str = glGetString(GL_VERSION);
        if (!version_str) {
            LOG_ERROR(k_category, "No hay contexto de OpenGL activo — glGetString(GL_VERSION) devolvió nulo.");
            return;
        }

        LOGF_INFO("GLDevice", "Versión de OpenGL: {}", reinterpret_cast<const char*>(version_str));

        const GLubyte* renderer_str = glGetString(GL_RENDERER);
        if (renderer_str) LOGF_INFO("GLDevice", "Renderer: {}", reinterpret_cast<const char*>(renderer_str));

        if (enable_validation) {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(GLDevice::gl_debug_callback, this);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
            LOG_INFO("GLDevice", "Salida de depuración de GL habilitada.");
        }

        m_textures.reserve(64);
        m_buffers.reserve(128);
        m_valid = true;
        LOG_INFO("GLDevice", "GLDevice inicializado.");
    }

    GLDevice::~GLDevice() {
        for (auto& slot : m_textures) {
            if (slot.alive && slot.name != 0) glDeleteTextures(1, &slot.name);
        }
        for (auto& slot : m_buffers) {
            if (slot.alive && slot.name != 0) glDeleteBuffers(1, &slot.name);
        }

    #if defined(_WIN32)
        if (m_hglrc) {
            ::wglMakeCurrent(nullptr, nullptr);
            ::wglDeleteContext(static_cast<HGLRC>(m_hglrc));
        }
        if (m_hdc && m_hwnd)
            ::ReleaseDC(static_cast<HWND>(m_hwnd), static_cast<HDC>(m_hdc));
    #elif defined(__APPLE__)
        if (m_nsgl_ctx) {
            @autoreleasepool {
                [NSOpenGLContext clearCurrentContext];
                (void)(__bridge_transfer NSOpenGLContext*)m_nsgl_ctx;
                m_nsgl_ctx = nullptr;
            }
        }
    #elif defined(ANXIETY_USE_GLX)
        if (m_display) {
            auto* display = static_cast<Display*>(m_display);
            glXMakeCurrent(display, 0, nullptr);
            if (m_glx_ctx) glXDestroyContext(display, static_cast<GLXContext>(m_glx_ctx));
            if (m_own_display) XCloseDisplay(display);
        }
    #endif
        LOG_INFO(k_category, "GLDevice destruido.");
    }

    // Helpers del asignador de slots -----------------------------------------------------------------
    uint32_t GLDevice::alloc_tex_slot() {
        if (!m_texture_free.empty()) {
            uint32_t idx = m_texture_free.back(); m_texture_free.pop_back(); return idx;
        }
        m_textures.push_back({});
        return static_cast<uint32_t>(m_textures.size() - 1);
    }
    void GLDevice::free_tex_slot(uint32_t idx) { m_textures[idx] = {}; m_texture_free.push_back(idx); }

    uint32_t GLDevice::alloc_buf_slot() {
        if (!m_buffer_free.empty()) {
            uint32_t idx = m_buffer_free.back(); m_buffer_free.pop_back(); return idx;
        }
        m_buffers.push_back({});
        return static_cast<uint32_t>(m_buffers.size() - 1);
    }
    void GLDevice::free_buf_slot(uint32_t idx) { m_buffers[idx] = {}; m_buffer_free.push_back(idx); }

    // Accesores de slots — el id codifica (poolIndex + 2), de modo que id==0 siempre es inválido y id==1 es el centinela del swapchain.
    static constexpr uint64_t k_tex_id_base = 2ULL;
    static constexpr uint64_t k_buf_id_base = 2ULL;

    GLTextureSlot& GLDevice::tex_slot(rhi::TextureHandle h) {
        assert(h.is_valid() && h.id >= k_tex_id_base);
        return m_textures[static_cast<uint32_t>(h.id - k_tex_id_base)];
    }
    const GLTextureSlot& GLDevice::tex_slot(rhi::TextureHandle h) const {
        assert(h.is_valid() && h.id >= k_tex_id_base);
        return m_textures[static_cast<uint32_t>(h.id - k_tex_id_base)];
    }
    GLBufferSlot& GLDevice::buf_slot(rhi::BufferHandle h) {
        assert(h.is_valid() && h.id >= k_buf_id_base);
        return m_buffers[static_cast<uint32_t>(h.id - k_buf_id_base)];
    }
    const GLBufferSlot& GLDevice::buf_slot(rhi::BufferHandle h) const {
        assert(h.is_valid() && h.id >= k_buf_id_base);
        return m_buffers[static_cast<uint32_t>(h.id - k_buf_id_base)];
    }

    // create_buffer ------------------------------------------------------------------------------
    // Usa llamadas de GL basadas en bind (glGenBuffers/glBufferData) en vez de DSA de GL 4.5, para
    // que esta ruta funcione dentro del tope de GL 4.1 de macOS y pueda ser heredada por GLESDevice
    // en GLES 3.0.
    rhi::BufferHandle GLDevice::create_buffer(const rhi::BufferDesc& desc, const void* initial_data, size_t initial_data_sz) {
        const uint32_t idx = alloc_buf_slot();
        GLBufferSlot& slot = m_buffers[idx];

        GLenum target = GL_ARRAY_BUFFER;
        GLenum usage  = GL_STATIC_DRAW;

        using BU = anxiety::rendering::rhi::BufferUsage;
        if (has_flag(desc.usage, BU::Uniform)) {
            target = GL_UNIFORM_BUFFER;
            usage  = GL_DYNAMIC_DRAW;
        } else if (has_flag(desc.usage, BU::Index)) {
            target = GL_ELEMENT_ARRAY_BUFFER;
            usage  = GL_STATIC_DRAW;
        } else if (has_flag(desc.usage, BU::Storage)) {
            target = GL_SHADER_STORAGE_BUFFER;
            usage  = GL_DYNAMIC_DRAW;
        } else if (has_flag(desc.usage, BU::Indirect)) {
            target = GL_DRAW_INDIRECT_BUFFER;
            usage  = GL_DYNAMIC_DRAW;
        } else if (has_flag(desc.usage, BU::Vertex)) {
            target = GL_ARRAY_BUFFER;
            usage  = GL_STATIC_DRAW;
        }

        GLuint buf = 0;
        glGenBuffers(1, &buf);
        if (buf == 0) {
            LOGF_ERROR(k_category, "glGenBuffers falló ({})", desc.debug_name ? desc.debug_name : "?");
            free_buf_slot(idx);
            return {};
        }

        glBindBuffer(target, buf);
        glBufferData(target, static_cast<GLsizeiptr>(desc.size_bytes), (initial_data_sz > 0) ? initial_data : nullptr, usage);
        glBindBuffer(target, 0);

        // glObjectLabel requiere GL 4.3+ / la extensión KHR_debug — no está presente en macOS 4.1.
        if (glObjectLabel && desc.debug_name) glObjectLabel(GL_BUFFER, buf, -1, desc.debug_name);

        slot.name   = buf;
        slot.target = target;
        slot.size   = desc.size_bytes;
        slot.alive  = true;

        return rhi::BufferHandle{ idx + k_buf_id_base };
    }

    // destroy_buffer -----------------------------------------------------------------------------
    void GLDevice::destroy_buffer(rhi::BufferHandle handle) {
        if (!handle.is_valid()) return;
        GLBufferSlot& slot = buf_slot(handle);
        if (!slot.alive) return;

        glDeleteBuffers(1, &slot.name);
        const uint32_t idx = static_cast<uint32_t>(handle.id - k_buf_id_base);
        free_buf_slot(idx);
    }

    // write_buffer -------------------------------------------------------------------------------
    void GLDevice::write_buffer(rhi::BufferHandle handle, const void* data, size_t offset, size_t size) {
        if (!handle.is_valid()) return;
        GLBufferSlot& slot = buf_slot(handle);
        if (!slot.alive) return;

        glBindBuffer(slot.target, slot.name);
        glBufferSubData(slot.target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
        glBindBuffer(slot.target, 0);
    }

    // create_texture -----------------------------------------------------------------------------
    // Usa los puntos de entrada clásicos basados en bind en vez de DSA de GL 4.5
    // (glCreateTextures/glTextureStorage2D/glTextureParameteri): la implementación de OpenGL de
    // macOS tiene un tope de 4.1 core y GLES no tiene DSA en absoluto (GLESDevice hereda este
    // método), por lo que ahí las llamadas de DSA resolverían a punteros de función nulos.
    rhi::TextureHandle GLDevice::create_texture(const rhi::TextureDesc& desc) {
        const uint32_t idx = alloc_tex_slot();
        GLTextureSlot& slot = m_textures[idx];

        GLuint tex = 0;
        glGenTextures(1, &tex);
        if (tex == 0) {
            LOGF_ERROR(k_category, "glGenTextures falló ({})", desc.debug_name ? desc.debug_name : "?");
            free_tex_slot(idx);
            return {};
        }

        const GLenum  internal_fmt = to_GL_format(desc.format);
        const GLenum  base_fmt     = to_GL_base_format(desc.format);
        const GLenum  pixel_type   = to_GL_type(desc.format);
        const GLsizei mips         = static_cast<GLsizei>(desc.mip_levels > 0 ? desc.mip_levels : 1);

        glBindTexture(GL_TEXTURE_2D, tex);

        for (GLsizei level = 0; level < mips; ++level) {
            const GLsizei mip_w = std::max<GLsizei>(1, static_cast<GLsizei>(desc.extent.width)  >> level);
            const GLsizei mip_h = std::max<GLsizei>(1, static_cast<GLsizei>(desc.extent.height) >> level);
            glTexImage2D(GL_TEXTURE_2D, level, static_cast<GLint>(internal_fmt), mip_w, mip_h, 0, base_fmt, pixel_type, nullptr);
        }

        if (is_depth_format(desc.format) || desc.is_render_target) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        } else {
            // Con un solo nivel, un filtro con mipmaps deja la textura incompleta y GL la muestrea
            // como negro: el filtro mipmap solo se usa cuando hay más de un nivel.
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mips > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }

        glBindTexture(GL_TEXTURE_2D, 0);

        if (glObjectLabel && desc.debug_name) glObjectLabel(GL_TEXTURE, tex, -1, desc.debug_name);

        slot.name            = tex;
        slot.width           = desc.extent.width;
        slot.height          = desc.extent.height;
        slot.mips            = static_cast<uint32_t>(mips);
        slot.internal_format = internal_fmt;
        slot.base_format     = base_fmt;
        slot.pixel_type      = pixel_type;
        slot.is_depth        = is_depth_format(desc.format);
        slot.alive           = true;

        return rhi::TextureHandle{ idx + k_tex_id_base };
    }

    // destroy_texture ----------------------------------------------------------------------------
    void GLDevice::destroy_texture(rhi::TextureHandle handle) {
        if (!handle.is_valid()) return;
        if (handle.id == 1) return;  // centinela = framebuffer por defecto; nunca se elimina

        GLTextureSlot& slot = tex_slot(handle);
        if (!slot.alive) return;

        glDeleteTextures(1, &slot.name);
        const uint32_t idx = static_cast<uint32_t>(handle.id - k_tex_id_base);
        free_tex_slot(idx);
    }

    // upload_texture_data ------------------------------------------------------------------------
    void GLDevice::upload_texture_data(rhi::TextureHandle handle, const uint8_t* rgba8, uint32_t width, uint32_t height) {
        if (!handle.is_valid() || handle.id == 1) return;
        GLTextureSlot& slot = tex_slot(handle);
        if (!slot.alive) return;

        glBindTexture(GL_TEXTURE_2D, slot.name);
        glTexSubImage2D(GL_TEXTURE_2D,
                        0,                             // mip level
                        0, 0,                          // xoffset, yoffset
                        static_cast<GLsizei>(width),
                        static_cast<GLsizei>(height),
                        GL_RGBA,                       // siempre origen RGBA8 según el contrato
                        GL_UNSIGNED_BYTE,
                        rgba8);

        if (slot.mips > 1)
            glGenerateMipmap(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, 0);

        // Contrato síncrono — solo se retorna una vez completada la copia en la GPU.
        glFinish();
    }

    // compile_shader_from_source -------------------------------------------------------------------
    // Los shaders del motor están escritos en HLSL y el driver de GL solo entiende GLSL, así que la
    // ruta es HLSL -> SPIR-V (shaderc, compartido con Vulkan) -> GLSL (SPIRV-Cross). Se devuelve el
    // texto GLSL para que create_shader() lo compile.
    //
    // Se genera GLSL 4.10 (el máximo de macOS): no admite layout(binding=N) en los UBO, así que cada
    // bloque se renombra a "<prefijo><registro>" y create_pipeline() lo enlaza con glUniformBlockBinding.
    // Los cbuffer usan la misma numeración que el registro HLSL bN, que es lo que espera
    // GLDescriptorSet (binding == registro).
    static constexpr char k_ubo_name_prefix[] = "anxiety_ub";
    // Las texturas HLSL (tN) y sus samplers (sN) son objetos separados; GLSL usa sampler2D combinados.
    // Cada combinado se renombra a "<prefijo><N>", con N = registro tN de la textura, y create_pipeline()
    // le asigna la unidad de textura N (la misma que usa GLDescriptorSet: GL_TEXTURE0 + binding).
    static constexpr char k_tex_name_prefix[] = "anxiety_tex";

#ifdef ANXIETY_HAVE_SPIRV_CROSS
    // Cuando SPIRV-Cross no puede expresar un cbuffer en std140 solo dice "Buffer block cannot be
    // expressed…" sin nombrar el miembro. Aquí se localizan los que rompen la alineación de std140 (un
    // vec3/vec4/matriz debe empezar en múltiplo de 16, un vec2 en múltiplo de 8): en HLSL un float3
    // puede empezar, p. ej., en el offset 36 o 52, y en std140 no.
    static void report_std140_violations(spirv_cross::CompilerGLSL& glsl, const spirv_cross::ShaderResources& res) {
        for (const auto& ubo : res.uniform_buffers) {
            const spirv_cross::SPIRType& block = glsl.get_type(ubo.base_type_id);
            for (uint32_t i = 0; i < block.member_types.size(); ++i) {
                const spirv_cross::SPIRType& m = glsl.get_type(block.member_types[i]);
                if (m.basetype == spirv_cross::SPIRType::Struct) continue;

                const uint32_t offset = glsl.get_member_decoration(ubo.base_type_id, i, spv::DecorationOffset);
                uint32_t       align  = 4;
                if (m.columns > 1 || m.vecsize >= 3) align = 16;
                else if (m.vecsize == 2)             align = 8;

                if (offset % align != 0) {
                    LOGF_ERROR(k_category, "cbuffer '{}': el miembro '{}' está en el offset {} pero std140 (OpenGL) exige alinearlo a {}. "
                               "Sustituye los float3/vec3 de relleno por escalares (float) o reordena los miembros.",
                               glsl.get_name(ubo.id), glsl.get_member_name(ubo.base_type_id, i), offset, align);
                }
            }
        }
    }
#endif

    std::vector<uint8_t> GLDevice::compile_shader_from_source(const char* source, const char* entry_point, rhi::ShaderStage stage) {
        if (!source || source[0] == '\0') {
            LOG_ERROR(k_category, "compile_shader_from_source: source nulo/vacío.");
            return {};
        }

#ifdef ANXIETY_HAVE_SPIRV_CROSS
        const std::vector<uint8_t> spirv = shader::compile_hlsl_to_spirv(source, entry_point, stage);
        if (spirv.empty() || spirv.size() % sizeof(uint32_t) != 0) {
            LOG_ERROR(k_category, "compile_shader_from_source: falló la compilación de HLSL a SPIR-V.");
            return {};
        }

        try {
            spirv_cross::CompilerGLSL glsl(reinterpret_cast<const uint32_t*>(spirv.data()), spirv.size() / sizeof(uint32_t));

            spirv_cross::CompilerGLSL::Options opts;
            opts.version                  = 410;
            opts.es                       = false;
            opts.enable_420pack_extension = false;   // sin layout(binding) — ver create_pipeline()
            opts.vertex.fixup_clipspace   = true;    // z de clip D3D/Vulkan [0,w] -> GL [-w,w]
            glsl.set_common_options(opts);

            const spirv_cross::ShaderResources res = glsl.get_shader_resources();
            for (const auto& ubo : res.uniform_buffers) {
                const uint32_t binding = glsl.get_decoration(ubo.id, spv::DecorationBinding);
                glsl.set_name(ubo.base_type_id, k_ubo_name_prefix + std::to_string(binding - shader::k_cbv_binding_base));
            }

            glsl.build_combined_image_samplers();
            for (const auto& remap : glsl.get_combined_image_samplers()) {
                const uint32_t binding = glsl.get_decoration(remap.image_id, spv::DecorationBinding);
                glsl.set_name(remap.combined_id, k_tex_name_prefix + std::to_string(binding - shader::k_srv_binding_base));
            }

            std::string text;
            try {
                text = glsl.compile();
            } catch (const spirv_cross::CompilerError&) {
                report_std140_violations(glsl, res);
                throw;
            }
            return std::vector<uint8_t>(text.begin(), text.end());
        } catch (const spirv_cross::CompilerError& e) {
            LOGF_ERROR(k_category, "SPIRV-Cross no pudo generar GLSL: {}", e.what());
            return {};
        }
#else
        (void)entry_point; (void)stage;
        LOG_ERROR(k_category, "compile_shader_from_source: compilado sin shaderc/SPIRV-Cross — no hay traducción de HLSL a GLSL.");
        return {};
#endif
    }

    // create_shader ------------------------------------------------------------------------------
    std::unique_ptr<rhi::IShader> GLDevice::create_shader(const rhi::ShaderDesc& desc, rhi::ShaderStage stage) {
        if (!desc.bytecode || desc.bytecode_size == 0) {
            LOG_ERROR(k_category, "create_shader: bytecode vacío.");
            return nullptr;
        }

        GLenum gl_stage = GL_VERTEX_SHADER;
        switch (stage) {
        case rhi::ShaderStage::Fragment: gl_stage = GL_FRAGMENT_SHADER; break;
        case rhi::ShaderStage::Compute:  gl_stage = GL_COMPUTE_SHADER;  break;
        default: break;
        }

        GLuint shader = glCreateShader(gl_stage);
        if (shader == 0) {
            LOG_ERROR(k_category, "glCreateShader falló.");
            return nullptr;
        }

        // bytecode es texto de source GLSL
        const GLchar* src = static_cast<const GLchar*>(desc.bytecode);
        const GLint   len = static_cast<GLint>(desc.bytecode_size);
        glShaderSource(shader, 1, &src, &len);
        glCompileShader(shader);

        GLint status = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE) {
            GLint log_len = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
            std::string log(static_cast<size_t>(log_len), '\0');
            glGetShaderInfoLog(shader, log_len, nullptr, log.data());
            LOGF_ERROR(k_category, "Falló la compilación del shader ({}): {}", desc.debug_name ? desc.debug_name : "?", log);
            glDeleteShader(shader);
            return nullptr;
        }

        const char* entry = desc.entry_point ? desc.entry_point : "main";
        return std::make_unique<GLShader>(shader, entry, stage);
    }

    // create_pipeline ----------------------------------------------------------------------------
    std::unique_ptr<rhi::IPipeline> GLDevice::create_pipeline(const rhi::PipelineDesc& desc) {
        if (!desc.vertex_shader || !desc.fragment_shader) {
            LOG_ERROR(k_category, "create_pipeline: se requieren shaders de vértice y de fragmento.");
            return nullptr;
        }

        const auto* vs = dynamic_cast<const GLShader*>(desc.vertex_shader);
        const auto* fs = dynamic_cast<const GLShader*>(desc.fragment_shader);
        if (!vs || !fs) {
            LOG_ERROR(k_category, "create_pipeline: los shaders deben ser instancias de GLShader.");
            return nullptr;
        }

        // Enlazar el programa
        GLuint program = glCreateProgram();
        glAttachShader(program, vs->gl_name());
        glAttachShader(program, fs->gl_name());
        glLinkProgram(program);

        GLint link_status = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &link_status);
        if (link_status == GL_FALSE) {
            GLint log_len = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
            std::string log(static_cast<size_t>(log_len), '\0');
            glGetProgramInfoLog(program, log_len, nullptr, log.data());
            LOGF_ERROR(k_category, "Falló el enlazado del programa ({}): {}",
                       desc.debug_name ? desc.debug_name : "?", log);
            glDeleteProgram(program);
            return nullptr;
        }

        // Enlaza cada uniform block a su binding point (los bloques se llaman "<prefijo><registro>",
        // ver compile_shader_from_source()). GLDescriptorSet los vincula con glBindBufferRange.
        {
            GLint block_count = 0;
            glGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &block_count);
            const size_t prefix_len = sizeof(k_ubo_name_prefix) - 1;
            for (GLint i = 0; i < block_count; ++i) {
                char name[128] = {};
                glGetActiveUniformBlockName(program, static_cast<GLuint>(i), sizeof(name), nullptr, name);
                if (std::strncmp(name, k_ubo_name_prefix, prefix_len) == 0)
                    glUniformBlockBinding(program, static_cast<GLuint>(i), static_cast<GLuint>(std::atoi(name + prefix_len)));
            }
        }

        // Asigna a cada sampler2D combinado la unidad de textura que corresponde a su registro tN.
        {
            GLint uniform_count = 0;
            glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniform_count);
            const size_t prefix_len = sizeof(k_tex_name_prefix) - 1;
            for (GLint i = 0; i < uniform_count; ++i) {
                char   name[128] = {};
                GLint  size = 0;
                GLenum type = 0;
                glGetActiveUniform(program, static_cast<GLuint>(i), sizeof(name), nullptr, &size, &type, name);
                if (std::strncmp(name, k_tex_name_prefix, prefix_len) != 0) continue;

                const GLint loc = glGetUniformLocation(program, name);
                if (loc >= 0) glProgramUniform1i(program, loc, std::atoi(name + prefix_len));
            }
        }

        // Desvincular los objetos de shader tras un enlazado exitoso
        glDetachShader(program, vs->gl_name());
        glDetachShader(program, fs->gl_name());

        // Crear el VAO plantilla. El formato de los atributos de vértice se graba en él en tiempo
        // de ejecución del command buffer mediante bind_vertex_buffer() usando glVertexAttribPointer
        // — esto evita el DSA de GL 4.5 (glVertexArrayAttribFormat, etc.), ausente en macOS 4.1 y GLES.
        GLuint vao = 0;
        glGenVertexArrays(1, &vao);

        if (glObjectLabel && desc.debug_name) glObjectLabel(GL_PROGRAM, program, -1, desc.debug_name);

        auto pipeline = std::make_unique<GLPipeline>();
        pipeline->set_program(program);
        pipeline->set_vao(vao);
        pipeline->set_name(desc.debug_name ? desc.debug_name : "");
        pipeline->set_topology(to_GL_primitive_type(desc.topology));

        // Estado del rasterizador
        {
            using CM = anxiety::rendering::rhi::CullMode;
            using FM = anxiety::rendering::rhi::FillMode;
            const auto& r = desc.rasterizer;
            const bool  cull_enabled = (r.cull_mode != CM::None);
            GLenum      cull_face    = GL_BACK;
            if (r.cull_mode == CM::Front) cull_face = GL_FRONT;
            const GLenum fill_mode = (r.fill_mode == FM::Wireframe) ? GL_LINE : GL_FILL;
            pipeline->set_rasterizer_state(cull_enabled, cull_face, r.front_face_CCW, fill_mode);
        }

        // Estado de depth
        {
            const auto& d = desc.depth_stencil;
            pipeline->set_depth_state(d.depth_test_enable, d.depth_write_enable, to_GL_depth_func(d.depth_compare_op));
        }

        // Estado de blend (attachment 0)
        {
            const auto& b = desc.blend;
            pipeline->set_blend_state(b.blend_enable,
                                       to_GL_blend_factor(b.src_color_factor),
                                       to_GL_blend_factor(b.dst_color_factor),
                                       to_GL_blend_equation(b.color_blend_op),
                                       to_GL_blend_factor(b.src_alpha_factor),
                                       to_GL_blend_factor(b.dst_alpha_factor),
                                       to_GL_blend_equation(b.alpha_blend_op));
        }

        pipeline->set_descriptor_layout(desc.descriptor_layout);
        pipeline->set_vertex_format(desc.vertex_layout.attributes, desc.vertex_layout.stride_bytes);

        return pipeline;
    }

    // create_descriptor_set ----------------------------------------------------------------------
    std::unique_ptr<rhi::IDescriptorSet> GLDevice::create_descriptor_set(const rhi::DescriptorSetLayout& layout) {
        return std::make_unique<GLDescriptorSet>(layout, this);
    }

    // create_command_buffer ----------------------------------------------------------------------
    std::unique_ptr<rhi::ICommandBuffer> GLDevice::create_command_buffer() {
        return std::make_unique<GLCommandBuffer>(this);
    }

    // create_swapchain ---------------------------------------------------------------------------
    std::unique_ptr<rhi::ISwapchain> GLDevice::create_swapchain(const rhi::SwapchainDesc& desc) {
        return std::make_unique<GLSwapchain>(desc);
    }

    // submit -----------------------------------------------------------------------------------------
    void GLDevice::submit(rhi::ICommandBuffer& cmd) {
        static_cast<GLCommandBuffer&>(cmd).execute_all();
    }

    // wait_idle --------------------------------------------------------------------------------------
    void GLDevice::wait_idle() { glFinish(); }

} // namespace anxiety::rendering::backend::opengl

#endif // ANXIETY_BACKEND_OPENGL
