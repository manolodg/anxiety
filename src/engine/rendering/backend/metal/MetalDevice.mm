#ifdef ANXIETY_BACKEND_METAL

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include "MetalDevice.h"
#include "MetalHelpers.h"
#include "MetalCommandBuffer.h"
#include "MetalSwapchain.h"
#include "MetalShader.h"
#include "MetalPipeline.h"
#include "MetalDescriptorSet.h"
#include "../../shader/HlslCompiler.h"
#include "Logger.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <dispatch/dispatch.h>

#ifdef ANXIETY_HAVE_SPIRV_CROSS
#include <spirv_msl.hpp>
#endif

namespace anxiety::rendering::backend::metal {
    // Construcción / destrucción --------------------------------------------------------------------
    MetalDevice::MetalDevice(bool enable_validation) {
        @autoreleasepool {
            id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
            if (!dev) {
                LOGF_ERROR("Metal", "MTLCreateSystemDefaultDevice() devolvió nil");
                return;
            }

            id<MTLCommandQueue> queue = [dev newCommandQueue];
            if (!queue) {
                LOGF_ERROR("Metal", "newCommandQueue() devolvió nil");
                return;
            }

            // Retiene los objetos ObjC y los guarda como void* por seguridad en C++.
            m_device        = (__bridge_retained void*)dev;
            m_command_queue = (__bridge_retained void*)queue;

            if (enable_validation) LOGF_INFO("Metal", "Se solicitó la capa de validación de Metal (define MTL_DEBUG_LAYER=1 en el entorno)");

            LOGF_INFO("Metal", "MetalDevice creado — GPU: {}", [[dev name] UTF8String]);
            m_valid = true;
        }
    }

    MetalDevice::~MetalDevice() {
        wait_idle();

        if (m_command_queue) {
            // Libera el objeto ObjC retenido.
            (void)(__bridge_transfer id<MTLCommandQueue>)m_command_queue;
            m_command_queue = nullptr;
        }
        if (m_device) {
            (void)(__bridge_transfer id<MTLDevice>)m_device;
            m_device = nullptr;
        }
    }

    // Gestión de slots -----------------------------------------------------------------------------
    uint32_t MetalDevice::alloc_tex_slot() {
        if (!m_texture_free.empty()) {
            uint32_t idx = m_texture_free.back();
            m_texture_free.pop_back();
            return idx;
        }
        m_textures.push_back({});
        return static_cast<uint32_t>(m_textures.size() - 1);
    }

    void MetalDevice::free_tex_slot(uint32_t idx) {
        m_textures[idx] = {};
        m_texture_free.push_back(idx);
    }

    uint32_t MetalDevice::alloc_buf_slot() {
        if (!m_buffer_free.empty()) {
            uint32_t idx = m_buffer_free.back();
            m_buffer_free.pop_back();
            return idx;
        }
        m_buffers.push_back({});
        return static_cast<uint32_t>(m_buffers.size() - 1);
    }

    void MetalDevice::free_buf_slot(uint32_t idx) {
        m_buffers[idx] = {};
        m_buffer_free.push_back(idx);
    }

    // Accesores de slots -------------------------------------------------------------------------
    MetalTextureSlot& MetalDevice::tex_slot(rhi::TextureHandle h) { return m_textures[static_cast<uint32_t>(h.id) - 1]; }
    MetalBufferSlot&  MetalDevice::buf_slot(rhi::BufferHandle h)  { return m_buffers[static_cast<uint32_t>(h.id) - 1]; }

    // Buffer -------------------------------------------------------------------------------------
    rhi::BufferHandle MetalDevice::create_buffer(const rhi::BufferDesc& desc, const void* initial_data, size_t initial_data_sz) {
        @autoreleasepool {
            id<MTLDevice> dev = (__bridge id<MTLDevice>)m_device;

            // Los buffers uniform/storage usan almacenamiento shared (CPU↔GPU sin copia).
            // Los buffers de vértices/índices usan almacenamiento managed (la CPU escribe, GPU-privado tras didModifyRange).
            MTLResourceOptions opts;
            const bool is_uniform = anxiety::rendering::rhi::has_flag(desc.usage, anxiety::rendering::rhi::BufferUsage::Uniform)
                                 || anxiety::rendering::rhi::has_flag(desc.usage, anxiety::rendering::rhi::BufferUsage::Storage);
            if (is_uniform) {
                opts = MTLResourceStorageModeShared;
            } else {
                opts = MTLResourceStorageModeManaged;
            }

            id<MTLBuffer> buf = [dev newBufferWithLength:(NSUInteger)desc.size_bytes options:opts];
            if (!buf) {
                LOGF_ERROR("Metal", "newBufferWithLength:{} falló", desc.size_bytes);
                return {};
            }

            if (desc.debug_name) buf.label = [NSString stringWithUTF8String:desc.debug_name];

            void* mapped_ptr = [buf contents];  // no nil para buffers shared/managed

            if (initial_data && initial_data_sz > 0) {
                size_t copy_size = std::min(initial_data_sz, (size_t)desc.size_bytes);
                std::memcpy(mapped_ptr, initial_data, copy_size);
                if (opts == MTLResourceStorageModeManaged) {
                    [buf didModifyRange:NSMakeRange(0, copy_size)];
                }
            }

            uint32_t idx = alloc_buf_slot();
            auto& slot   = m_buffers[idx];
            slot.buffer  = (__bridge_retained void*)buf;
            slot.mapped  = mapped_ptr;
            slot.size    = desc.size_bytes;
            slot.alive   = true;

            return { static_cast<uint64_t>(idx) + 1 };
        }
    }

    void MetalDevice::destroy_buffer(rhi::BufferHandle handle) {
        if (!handle.is_valid()) return;
        uint32_t idx  = static_cast<uint32_t>(handle.id) - 1;
        auto&    slot = m_buffers[idx];
        if (!slot.alive) return;
        if (slot.buffer) {
            (void)(__bridge_transfer id<MTLBuffer>)slot.buffer;
            slot.buffer = nullptr;
        }
        slot.mapped = nullptr;
        slot.alive  = false;
        free_buf_slot(idx);
    }

    void MetalDevice::write_buffer(rhi::BufferHandle handle, const void* data, size_t offset, size_t size) {
        if (!handle.is_valid()) return;
        auto& slot = buf_slot(handle);
        if (!slot.mapped || !data) return;

        std::memcpy(static_cast<uint8_t*>(slot.mapped) + offset, data, size);

        // Notifica a Metal que el rango del lado de la CPU está sucio, para buffers managed.
        id<MTLBuffer> buf = (__bridge id<MTLBuffer>)slot.buffer;
        if (buf.storageMode == MTLStorageModeManaged) {
            [buf didModifyRange:NSMakeRange(offset, size)];
        }
    }

    // Textura ------------------------------------------------------------------------------------
    anxiety::rendering::rhi::TextureHandle MetalDevice::create_texture(const anxiety::rendering::rhi::TextureDesc& desc) {
        @autoreleasepool {
            id<MTLDevice> dev = (__bridge id<MTLDevice>)m_device;

            MTLTextureDescriptor* td = [[MTLTextureDescriptor alloc] init];
            td.textureType      = MTLTextureType2D;
            td.pixelFormat      = to_MTL_pixel_format(desc.format);
            td.width            = desc.extent.width;
            td.height           = desc.extent.height;
            td.mipmapLevelCount = desc.mip_levels;
            td.arrayLength      = desc.array_size;
            td.storageMode      = MTLStorageModePrivate;         // solo GPU por defecto

            MTLTextureUsage usage = MTLTextureUsageShaderRead;
            if (desc.is_render_target) usage |= MTLTextureUsageRenderTarget;
            if (desc.is_depth_target)  usage |= MTLTextureUsageRenderTarget;
            td.usage = usage;

            // Las texturas que subimos vía replaceRegion necesitan almacenamiento accesible desde la CPU.
            if (!desc.is_render_target && !desc.is_depth_target) td.storageMode = MTLStorageModeManaged;

            id<MTLTexture> tex = [dev newTextureWithDescriptor:td];
            if (!tex) {
                LOGF_ERROR("Metal", "newTextureWithDescriptor falló ({}x{})", desc.extent.width, desc.extent.height);
                return {};
            }

            if (desc.debug_name) tex.label = [NSString stringWithUTF8String:desc.debug_name];

            uint32_t idx = alloc_tex_slot();
            auto& slot   = m_textures[idx];
            slot.texture  = (__bridge_retained void*)tex;
            slot.width    = desc.extent.width;
            slot.height   = desc.extent.height;
            slot.is_depth = desc.is_depth_target;
            slot.external = false;
            slot.alive    = true;

            return { static_cast<uint64_t>(idx) + 1 };
        }
    }

    void MetalDevice::destroy_texture(anxiety::rendering::rhi::TextureHandle handle) {
        if (!handle.is_valid()) return;
        uint32_t idx  = static_cast<uint32_t>(handle.id) - 1;
        auto&    slot = m_textures[idx];
        if (!slot.alive) return;
        if (slot.texture && !slot.external) {
            (void)(__bridge_transfer id<MTLTexture>)slot.texture;
            slot.texture = nullptr;
        }
        slot.alive = false;
        free_tex_slot(idx);
    }

    void MetalDevice::upload_texture_data(rhi::TextureHandle handle, const uint8_t* rgba8, uint32_t width, uint32_t height) {
        if (!handle.is_valid() || !rgba8) return;
        @autoreleasepool {
            auto& slot = tex_slot(handle);
            id<MTLTexture> tex = (__bridge id<MTLTexture>)slot.texture;

            MTLRegion region = MTLRegionMake2D(0, 0, width, height);
            NSUInteger bytes_per_row = width * 4;  // RGBA8 = 4 bytes por píxel
            [tex replaceRegion:region mipmapLevel:0 withBytes:rgba8 bytesPerRow:bytes_per_row];
        }
    }

    // Registro de texturas externas (usado por el swapchain) --------------------------------------
    anxiety::rendering::rhi::TextureHandle MetalDevice::register_external_texture(void* mtl_texture, uint32_t w, uint32_t h) {
        uint32_t idx = alloc_tex_slot();
        auto& slot   = m_textures[idx];
        slot.texture   = mtl_texture;                     // NO bridge_retained — el llamador conserva la vida útil
        slot.width     = w;
        slot.height    = h;
        slot.is_depth  = false;
        slot.external  = true;
        slot.alive     = true;
        return { static_cast<uint64_t>(idx) + 1 };
    }

    void MetalDevice::unregister_texture(rhi::TextureHandle h) {
        if (!h.is_valid()) return;
        uint32_t idx = static_cast<uint32_t>(h.id) - 1;
        auto& slot   = m_textures[idx];
        if (!slot.alive) return;
        // Las texturas externas no se liberan aquí — el CAMetalLayer es su propietario.
        slot.texture = nullptr;
        slot.alive   = false;
        free_tex_slot(idx);
    }

    // Compilación de shaders -------------------------------------------------------------------------
    // Los shaders del motor son HLSL: shaderc los compila a SPIR-V y SPIRV-Cross los traduce a MSL. El
    // "bytecode" que devolvemos es el texto MSL en UTF-8; create_shader() lo compila a MTLLibrary.
#ifdef ANXIETY_HAVE_SPIRV_CROSS
    // Fija los slots MSL de cada recurso según los rangos de binding de SPIR-V (ver HlslCompiler.h y
    // k_msl_* en MetalHelpers.h).
    static void add_msl_bindings(spirv_cross::CompilerMSL& msl, const spirv_cross::SmallVector<spirv_cross::Resource>& resources,
                                 uint32_t binding_base, uint32_t slot_base, bool buffer, bool texture, bool sampler) {
        for (const auto& r : resources) {
            spirv_cross::MSLResourceBinding rb{};
            rb.stage    = msl.get_execution_model();
            rb.desc_set = msl.get_decoration(r.id, spv::DecorationDescriptorSet);
            rb.binding  = msl.get_decoration(r.id, spv::DecorationBinding);
            const uint32_t slot = slot_base + (rb.binding - binding_base);
            if (buffer)  rb.msl_buffer  = slot;
            if (texture) rb.msl_texture = slot;
            if (sampler) rb.msl_sampler = slot;
            msl.add_msl_resource_binding(rb);
        }
    }
#endif

    std::vector<uint8_t> MetalDevice::compile_shader_from_source(const char* source, const char* entry_point, rhi::ShaderStage stage) {
        if (!source || !*source) {
            LOGF_ERROR("Metal", "compile_shader_from_source: source nulo/vacío.");
            return {};
        }
#ifdef ANXIETY_HAVE_SPIRV_CROSS
        const std::vector<uint8_t> spirv = shader::compile_hlsl_to_spirv(source, entry_point, stage);
        if (spirv.empty() || spirv.size() % sizeof(uint32_t) != 0) {
            LOGF_ERROR("Metal", "compile_shader_from_source: falló la compilación de HLSL a SPIR-V.");
            return {};
        }

        std::string msl_source;
        try {
            spirv_cross::CompilerMSL msl(reinterpret_cast<const uint32_t*>(spirv.data()), spirv.size() / sizeof(uint32_t));

            spirv_cross::CompilerMSL::Options opts;
            opts.platform = spirv_cross::CompilerMSL::Options::macOS;
            opts.set_msl_version(2, 1);
            msl.set_msl_options(opts);

            const spirv_cross::ShaderResources res = msl.get_shader_resources();
            add_msl_bindings(msl, res.uniform_buffers, shader::k_cbv_binding_base,     k_msl_cbv_buffer_base, true,  false, false);
            add_msl_bindings(msl, res.storage_buffers, shader::k_uav_binding_base,     k_msl_uav_buffer_base, true,  false, false);
            add_msl_bindings(msl, res.separate_images, shader::k_srv_binding_base,     0,                     false, true,  false);
            add_msl_bindings(msl, res.separate_samplers, shader::k_sampler_binding_base, 0,                   false, false, true);

            msl_source = msl.compile();
        } catch (const spirv_cross::CompilerError& e) {
            LOGF_ERROR("Metal", "compile_shader_from_source: SPIRV-Cross falló al generar MSL: {}", e.what());
            return {};
        }

        // Valida el MSL compilándolo ahora para detectar errores en el momento de la carga.
        @autoreleasepool {
            id<MTLDevice> dev = (__bridge id<MTLDevice>)m_device;
            NSString* src = [NSString stringWithUTF8String:msl_source.c_str()];
            NSError*  err = nil;
            id<MTLLibrary> lib = [dev newLibraryWithSource:src options:nil error:&err];
            if (!lib) {
                LOGF_ERROR("Metal", "error de compilación MSL: {}\n{}", [[err localizedDescription] UTF8String], msl_source);
                return {};
            }
        }
        return std::vector<uint8_t>(msl_source.begin(), msl_source.end());
#else
        (void)entry_point; (void)stage;
        LOGF_ERROR("Metal", "compile_shader_from_source: compilado sin shaderc/SPIRV-Cross — no hay traducción de HLSL a MSL.");
        return {};
#endif
    }

    std::unique_ptr<rhi::IShader> MetalDevice::create_shader(const rhi::ShaderDesc& desc, rhi::ShaderStage stage) {
        @autoreleasepool {
            id<MTLDevice> dev = (__bridge id<MTLDevice>)m_device;

            // El bytecode es el texto fuente MSL.
            NSString* src = [[NSString alloc]
                initWithBytes:desc.bytecode
                       length:desc.bytecode_size
                     encoding:NSUTF8StringEncoding];

            NSError* err = nil;
            id<MTLLibrary> lib = [dev newLibraryWithSource:src options:nil error:&err];
            if (!lib) {
                LOGF_ERROR("Metal", "create_shader — error de compilación MSL: {}", [[err localizedDescription] UTF8String]);
                return nullptr;
            }

            const char* ep = desc.entry_point ? desc.entry_point : "main";
            // SPIRV-Cross renombra "main" a "main0" en MSL (main es reservado).
            NSString* entry = [NSString stringWithUTF8String:(std::strcmp(ep, "main") == 0 ? "main0" : ep)];
            id<MTLFunction> fn = [lib newFunctionWithName:entry];
            if (!fn) {
                LOGF_ERROR("Metal", "create_shader — función '{}' no encontrada en la library", ep);
                return nullptr;
            }

            return std::make_unique<MetalShader>(
                (__bridge_retained void*)lib,
                (__bridge_retained void*)fn,
                ep,
                stage);
        }
    }

    // Pipeline ---------------------------------------------------------------------------------------
    std::unique_ptr<rhi::IPipeline> MetalDevice::create_pipeline(const rhi::PipelineDesc& desc) {
        @autoreleasepool {
            id<MTLDevice> dev = (__bridge id<MTLDevice>)m_device;

            // Descriptor del render pipeline
            MTLRenderPipelineDescriptor* rpd = [[MTLRenderPipelineDescriptor alloc] init];

            if (desc.debug_name) rpd.label = [NSString stringWithUTF8String:desc.debug_name];

            // Función de vértice
            if (desc.vertex_shader) {
                auto* vs = static_cast<const MetalShader*>(desc.vertex_shader);
                rpd.vertexFunction = (__bridge id<MTLFunction>)vs->mtl_function();
            }

            // Función de fragmento
            if (desc.fragment_shader) {
                auto* fs = static_cast<const MetalShader*>(desc.fragment_shader);
                rpd.fragmentFunction = (__bridge id<MTLFunction>)fs->mtl_function();
            }

            // Descriptor de vértices
            if (!desc.vertex_layout.attributes.empty()) {
                MTLVertexDescriptor* vd = [[MTLVertexDescriptor alloc] init];
                // El índice de atributo es la posición en el layout: coincide con el location que shaderc
                // asigna por orden de declaración de la entrada del vertex shader (igual que Vulkan).
                uint32_t attr_index = 0;
                for (const auto& attr : desc.vertex_layout.attributes) {
                    vd.attributes[attr_index].format      = to_MTL_vertex_format(attr.format);
                    vd.attributes[attr_index].offset      = attr.byte_offset;
                    vd.attributes[attr_index].bufferIndex = attr.input_slot;
                    ++attr_index;
                }
                // Todos los atributos del mismo input slot comparten una única entrada de layout.
                const uint32_t slot = desc.vertex_layout.attributes.front().input_slot;
                vd.layouts[slot].stride       = desc.vertex_layout.stride_bytes;
                vd.layouts[slot].stepRate     = 1;
                vd.layouts[slot].stepFunction = MTLVertexStepFunctionPerVertex;
                rpd.vertexDescriptor = vd;
            }

            // Adjuntos de color
            for (size_t i = 0; i < desc.render_target_fmts.size(); ++i) {
                auto att = rpd.colorAttachments[i];
                att.pixelFormat = to_MTL_pixel_format(desc.render_target_fmts[i]);

                const auto& b = desc.blend;
                att.blendingEnabled             = b.blend_enable;
                att.rgbBlendOperation           = to_MTL_blend_operation(b.color_blend_op);
                att.alphaBlendOperation         = to_MTL_blend_operation(b.alpha_blend_op);
                att.sourceRGBBlendFactor        = to_MTL_blend_factor(b.src_color_factor);
                att.destinationRGBBlendFactor   = to_MTL_blend_factor(b.dst_color_factor);
                att.sourceAlphaBlendFactor      = to_MTL_blend_factor(b.src_alpha_factor);
                att.destinationAlphaBlendFactor = to_MTL_blend_factor(b.dst_alpha_factor);
            }

            // Formato de profundidad
            if (desc.depth_stencil.depth_format != anxiety::rendering::rhi::Format::Unknown) {
                rpd.depthAttachmentPixelFormat = to_MTL_pixel_format(desc.depth_stencil.depth_format);
            }

            NSError* err = nil;
            id<MTLRenderPipelineState> pso = [dev newRenderPipelineStateWithDescriptor:rpd error:&err];
            if (!pso) {
                LOGF_ERROR("Metal", "create_pipeline falló: {}", [[err localizedDescription] UTF8String]);
                return nullptr;
            }

            // Estado de depth-stencil
            id<MTLDepthStencilState> dss = nil;
            if (desc.depth_stencil.depth_test_enable || desc.depth_stencil.depth_write_enable) {
                MTLDepthStencilDescriptor* dsd = [[MTLDepthStencilDescriptor alloc] init];
                dsd.depthCompareFunction =
                    desc.depth_stencil.depth_test_enable
                        ? to_MTL_compare_function(desc.depth_stencil.depth_compare_op)
                        : MTLCompareFunctionAlways;
                dsd.depthWriteEnabled = desc.depth_stencil.depth_write_enable;
                dss = [dev newDepthStencilStateWithDescriptor:dsd];
            }

            auto pipeline = std::make_unique<MetalPipeline>(
                (__bridge_retained void*)pso,
                dss ? (__bridge_retained void*)dss : nullptr,
                desc.debug_name ? desc.debug_name : "",
                desc.topology,
                desc.rasterizer.cull_mode,
                desc.rasterizer.fill_mode,
                desc.rasterizer.front_face_CCW);

            // Samplers estáticos (registro sN de HLSL -> [[sampler(N)]] en MSL)
            for (const auto& sd : desc.static_samplers) {
                MTLSamplerDescriptor* sampd = [[MTLSamplerDescriptor alloc] init];
                sampd.minFilter    = to_MTL_sampler_min_mag_filter(sd.filter);
                sampd.magFilter    = to_MTL_sampler_min_mag_filter(sd.filter);
                sampd.mipFilter    = sd.filter == rhi::FilterMode::Nearest ? MTLSamplerMipFilterNearest : MTLSamplerMipFilterLinear;
                sampd.sAddressMode = to_MTL_sampler_address_mode(sd.address_U);
                sampd.tAddressMode = to_MTL_sampler_address_mode(sd.address_V);
                sampd.rAddressMode = to_MTL_sampler_address_mode(sd.address_W);
                sampd.lodMinClamp  = sd.min_lod;
                sampd.lodMaxClamp  = sd.max_lod;
                sampd.maxAnisotropy = sd.filter == rhi::FilterMode::Anisotropic ? std::max(1u, std::min(16u, sd.max_anisotropy)) : 1;
                id<MTLSamplerState> ss = [dev newSamplerStateWithDescriptor:sampd];
                if (!ss) {
                    LOGF_ERROR("Metal", "create_pipeline: no se pudo crear el sampler s{}", sd.shader_register);
                    continue;
                }
                pipeline->add_static_sampler(sd.shader_register, (__bridge_retained void*)ss);
            }
            return pipeline;
        }
    }

    // Descriptor set -----------------------------------------------------------------------------------
    std::unique_ptr<rhi::IDescriptorSet> MetalDevice::create_descriptor_set(const rhi::DescriptorSetLayout& layout) {
        return std::make_unique<MetalDescriptorSet>(this, layout);
    }
    // Command buffer -----------------------------------------------------------------------------
    std::unique_ptr<anxiety::rendering::rhi::ICommandBuffer> MetalDevice::create_command_buffer() { return std::make_unique<MetalCommandBuffer>(this); }

    // Swapchain ----------------------------------------------------------------------------------
    std::unique_ptr<anxiety::rendering::rhi::ISwapchain> MetalDevice::create_swapchain(const anxiety::rendering::rhi::SwapchainDesc& desc) { return std::make_unique<MetalSwapchain>(this, desc); }

    // Envío y sincronización -------------------------------------------------------------------
    void MetalDevice::submit(anxiety::rendering::rhi::ICommandBuffer& cmd) {
        @autoreleasepool {
            auto& mcmd = static_cast<MetalCommandBuffer&>(cmd);
            id<MTLCommandBuffer> mtl_cmd = (__bridge id<MTLCommandBuffer>)mcmd.mtl_command_buffer();
            if (mtl_cmd) [mtl_cmd commit];
        }
    }

    void MetalDevice::wait_idle() {
        @autoreleasepool {
            id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)m_command_queue;
            if (!queue) return;
            // Crea un command buffer de un solo uso y espera a que termine. Actúa como una fence de GPU que vacía la cola.
            id<MTLCommandBuffer> fence = [queue commandBuffer];
            [fence commit];
            [fence waitUntilCompleted];
        }
    }
} // namespace anxiety::rendering::backend::metal

#endif // ANXIETY_BACKEND_METAL
