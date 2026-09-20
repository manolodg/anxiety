#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

// Tipos de RHI y render graph (no requieren GPU para los tests del grafo).
#include "rhi/RHITypes.h"
#include "rhi/ShaderTypes.h"
#include "rhi/VertexLayout.h"
#include "rhi/IDescriptorSet.h"
#include "rhi/PipelineDesc.h"
#include "rhi/IPipeline.h"
#include "graph/RenderGraph.h"
#include "graph/RenderPass.h"

#include <string>
#include <vector>

// Mock mínimo de command buffer ---------------------------------------------------------------
struct MockCommandBuffer final : anxiety::rendering::rhi::ICommandBuffer {
    struct BarrierRecord {
        anxiety::rendering::rhi::TextureHandle handle;
        anxiety::rendering::rhi::ResourceState before;
        anxiety::rendering::rhi::ResourceState after;
    };

    std::vector<std::string>   log;                     // operaciones en orden (barrier / clear / ...)
    std::vector<BarrierRecord> barriers;                // todas las llamadas a barrier, en orden

    void begin() override { log.push_back("begin"); }
    void end()   override { log.push_back("end"); }

    void resource_barrier(anxiety::rendering::rhi::TextureHandle h, anxiety::rendering::rhi::ResourceState before, anxiety::rendering::rhi::ResourceState after) override {
        barriers.push_back({ h, before, after });
        log.push_back("barrier");
    }
    void clear_render_target(anxiety::rendering::rhi::TextureHandle, const anxiety::rendering::rhi::ClearColor&) override { log.push_back("clear"); }
    void clear_depth_stencil(anxiety::rendering::rhi::TextureHandle, float, uint8_t)                             override { log.push_back("clear_depth"); }

    void bind_pipeline(anxiety::rendering::rhi::IPipeline&) override { log.push_back("bind_pipeline"); }
    void bind_descriptor_set(uint32_t, anxiety::rendering::rhi::IDescriptorSet&) override { log.push_back("bind_ds"); }
    void bind_vertex_buffer(uint32_t, anxiety::rendering::rhi::BufferHandle, uint64_t, uint32_t) override { log.push_back("bind_vb"); }
    void bind_index_buffer(anxiety::rendering::rhi::BufferHandle, uint64_t, bool) override { log.push_back("bind_ib"); }

    void draw(uint32_t, uint32_t, uint32_t, uint32_t) override { log.push_back("draw"); }
    void draw_indexed(uint32_t, uint32_t, uint32_t, int32_t, uint32_t) override { log.push_back("draw_indexed"); }
    void dispatch(uint32_t, uint32_t, uint32_t) override { log.push_back("dispatch"); }
};

// Mocks mínimos de pipeline / descriptor-set (solo CPU) -----------------------------------------
struct MockPipeline final : anxiety::rendering::rhi::IPipeline {
    std::string_view debug_name() const noexcept override { return "MockPipeline"; }
};

struct MockDescriptorSet final : anxiety::rendering::rhi::IDescriptorSet {
    bool updated = false;
    void update(const std::vector<anxiety::rendering::rhi::DescriptorWrite>&) override { updated = true; }
};

using namespace anxiety::rendering;

// Parte 1 — Comprobaciones de tipos de la RHI (en tiempo de compilación y de valor) --------------
TEST_CASE("rhi_handle_default_invalid", "[rendering]") {
    rhi::BufferHandle  bh;
    rhi::TextureHandle th;
    REQUIRE(!bh.is_valid());
    REQUIRE(!th.is_valid());
}

TEST_CASE("rhi_handle_nonzero_valid", "[rendering]") {
    rhi::TextureHandle th{ 1 };
    REQUIRE(th.is_valid());
}

TEST_CASE("rhi_handle_equality", "[rendering]") {
    anxiety::rendering::rhi::TextureHandle a{ 3 }, b{ 3 }, c{ 7 };
    REQUIRE(a == b);
    REQUIRE(!(a == c));
}

TEST_CASE("rhi_clear_color_defaults", "[rendering]") {
    rhi::ClearColor cc;
    REQUIRE(cc.r == 0.0f);
    REQUIRE(cc.g == 0.0f);
    REQUIRE(cc.b == 0.0f);
    REQUIRE(cc.a == 1.0f);
}

TEST_CASE("rhi_extent_defaults", "[rendering]") {
    rhi::Extent2D e;
    REQUIRE(e.width == 0u);
    REQUIRE(e.height == 0u);
}

TEST_CASE("rhi_buffer_usage_flags", "[rendering]") {
    using BU = rhi::BufferUsage;
    const auto combined = BU::Vertex | BU::Index;
    REQUIRE(rhi::has_flag(combined, BU::Vertex));
    REQUIRE(rhi::has_flag(combined, BU::Index));
    REQUIRE(!rhi::has_flag(combined, BU::Uniform));
}

// Parte 2 — Tipos de pipeline / descriptor de la RHI ----------------------------------------------
TEST_CASE("pipeline_desc_topology_default", "[rendering]") {
    anxiety::rendering::rhi::PipelineDesc pd;
    REQUIRE(pd.topology == anxiety::rendering::rhi::PrimitiveTopology::TriangleList);
    REQUIRE(pd.rasterizer.cull_mode == anxiety::rendering::rhi::CullMode::Back);
    REQUIRE(!pd.depth_stencil.depth_test_enable);
}

TEST_CASE("pipeline_desc_vertex_layout", "[rendering]") {
    anxiety::rendering::rhi::PipelineDesc pd;
    pd.vertex_layout.attributes.push_back({ "POSITION", 0, anxiety::rendering::rhi::VertexFormat::Float3, 0, 0 });
    pd.vertex_layout.attributes.push_back({ "COLOR",    0, anxiety::rendering::rhi::VertexFormat::Float4, 0, 12 });
    pd.vertex_layout.stride_bytes = 28;
    REQUIRE(pd.vertex_layout.attributes.size() == 2u);
    REQUIRE(pd.vertex_layout.stride_bytes == 28u);
}

TEST_CASE("descriptor_binding_types", "[rendering]") {
    anxiety::rendering::rhi::DescriptorSetLayout layout;
    layout.bindings.push_back({ 0, anxiety::rendering::rhi::DescriptorType::UniformBuffer });
    layout.bindings.push_back({ 1, anxiety::rendering::rhi::DescriptorType::Texture });
    REQUIRE(layout.bindings.size() == 2u);
    REQUIRE(layout.bindings[0].type == anxiety::rendering::rhi::DescriptorType::UniformBuffer);
    REQUIRE(layout.bindings[1].type == anxiety::rendering::rhi::DescriptorType::Texture);
}

TEST_CASE("descriptor_set_update", "[rendering]") {
    MockDescriptorSet ds;
    REQUIRE(!ds.updated);
    anxiety::rendering::rhi::BufferHandle buf{ 1 };
    ds.update({ { 0, anxiety::rendering::rhi::DescriptorType::UniformBuffer, buf } });
    REQUIRE(ds.updated);
}

// Parte 3 — RenderGraph (solo CPU, sin barriers todavía) ------------------------------------------
TEST_CASE("rg_empty_graph", "[rendering]") {
    graph::RenderGraph g;
    g.compile();
    MockCommandBuffer cmd;
    g.execute(cmd);
    REQUIRE(g.pass_count() == 0u);
    REQUIRE(cmd.log.empty());
}

TEST_CASE("rg_single_pass_executes", "[rendering]") {
    graph::RenderGraph g;

    bool executed = false;
    g.add_pass({
        .name    = "TestPass",
        .execute = [&](rhi::ICommandBuffer&) { executed = true; }
    });

    g.compile();
    MockCommandBuffer cmd;
    g.execute(cmd);

    REQUIRE(executed);
}

TEST_CASE("rg_import_texture", "[rendering]") {
    graph::RenderGraph g;

    auto h = g.import_texture("rt0", anxiety::rendering::rhi::TextureHandle{ 42 }, rhi::ResourceState::Present);
    REQUIRE(h.is_valid());
    REQUIRE(g.imported_texture_count() == 1u);
}

TEST_CASE("rg_dependency_write_before_read", "[rendering]") {
    // El pase A escribe T, el pase B lee T.
    // Orden de registro: primero B, luego A.
    // Orden de ejecución esperado: A y luego B.
    graph::RenderGraph g;

    rhi::TextureHandle phys{ 1 };
    const auto T = g.import_texture("T", phys, rhi::ResourceState::Undefined);

    std::vector<std::string> order;

    g.add_pass({
        .name    = "B_reader",
        .reads = {{ T, anxiety::rendering::rhi::ResourceState::ShaderResource }},
        .execute = [&](rhi::ICommandBuffer&) { order.push_back("B"); }
    });
    g.add_pass({
        .name    = "A_writer",
        .writes = {{ T, anxiety::rendering::rhi::ResourceState::RenderTarget }},
        .execute = [&](rhi::ICommandBuffer&) { order.push_back("A"); }
    });

    g.compile();
    MockCommandBuffer cmd;
    g.execute(cmd);

    REQUIRE(order.size() == 2u);
    REQUIRE(order[0] == std::string("A"));
    REQUIRE(order[1] == std::string("B"));
}

TEST_CASE("rg_multiple_passes_ordered", "[rendering]") {
    // Cadena: P0 escribe T → P1 lee T, escribe U → P2 lee U.
    // Registrados como P2, P1, P0 (orden inverso).
    graph::RenderGraph g;

    rhi::TextureHandle physT{ 1 }, physU{ 2 };
    const auto T = g.import_texture("T", physT);
    const auto U = g.import_texture("U", physU);

    std::vector<int> order;

    g.add_pass({
        .name    = "P2",
        .reads = {{ U, anxiety::rendering::rhi::ResourceState::ShaderResource }},
        .execute = [&](rhi::ICommandBuffer&) { order.push_back(2); }
    });
    g.add_pass({
        .name    = "P1",
        .reads = {{ T, anxiety::rendering::rhi::ResourceState::ShaderResource }},
        .writes = {{ U, anxiety::rendering::rhi::ResourceState::RenderTarget }},
        .execute = [&](rhi::ICommandBuffer&) { order.push_back(1); }
    });
    g.add_pass({
        .name    = "P0",
        .writes = {{ T, anxiety::rendering::rhi::ResourceState::RenderTarget }},
        .execute = [&](rhi::ICommandBuffer&) { order.push_back(0); }
        });

    g.compile();
    MockCommandBuffer cmd;
    g.execute(cmd);

    REQUIRE(order.size() == 3u);
    REQUIRE(order[0] == 0);
    REQUIRE(order[1] == 1);
    REQUIRE(order[2] == 2);
}

TEST_CASE("rg_reset_clears_state", "[rendering]") {
    graph::RenderGraph g;
    rhi::TextureHandle phys{ 1 };
    g.import_texture("x", phys);
    g.add_pass({
        .name    = "P",
        .execute = [](rhi::ICommandBuffer&) {}
    });
    g.compile();

    g.reset();

    REQUIRE(g.pass_count() == 0u);
    REQUIRE(g.imported_texture_count() == 0u);
}

TEST_CASE("rg_multiple_writers_same_texture", "[rendering]") {
    // P0 y P1 escriben ambos T. P2 lee T → depende de ambos.
    graph::RenderGraph g;
    const auto T = g.import_texture("T", rhi::TextureHandle{ 7 });

    std::vector<std::string> order;
    g.add_pass({
        .name    = "P2",
        .reads = {{ T, anxiety::rendering::rhi::ResourceState::ShaderResource }},
        .execute = [&](rhi::ICommandBuffer&) { order.push_back("P2"); }
    });
    g.add_pass({
        .name    = "P0",
        .writes = {{ T, anxiety::rendering::rhi::ResourceState::RenderTarget }},
        .execute = [&](rhi::ICommandBuffer&) { order.push_back("P0"); }
    });
    g.add_pass({
        .name    = "P1",
        .writes = {{ T, anxiety::rendering::rhi::ResourceState::RenderTarget }},
        .execute = [&](rhi::ICommandBuffer&) { order.push_back("P1"); }
    });

    g.compile();
    MockCommandBuffer cmd;
    g.execute(cmd);

    REQUIRE(order.size() == 3u);
    // P2 debe ser el último.
    REQUIRE(order.back() == std::string("P2"));
}

// Parte 4 — Inserción automática de barriers -------------------------------------------------------

TEST_CASE("rg_auto_barrier_present_to_rt", "[rendering]") {
    // Importa el backbuffer con estado inicial Present, escritura RenderTarget. El grafo debe
    // emitir el barrier Present→RenderTarget antes del pase.
    anxiety::rendering::graph::RenderGraph g;
    anxiety::rendering::rhi::TextureHandle phys{ 10 };
    const auto bb = g.import_texture("bb", phys, anxiety::rendering::rhi::ResourceState::Present, anxiety::rendering::rhi::ResourceState::Present);

    g.add_pass({
        .name    = "ClearPass",
        .writes  = {{ bb, anxiety::rendering::rhi::ResourceState::RenderTarget }},
        .execute = [](anxiety::rendering::rhi::ICommandBuffer&) {}
    });

    g.compile();
    MockCommandBuffer cmd;
    g.execute(cmd);

    // Esperado: 1 barrier previo al pase (Present→RenderTarget) + 1 barrier final (RenderTarget→Present).
    REQUIRE(cmd.barriers.size() == 2u);
    REQUIRE(cmd.barriers[0].handle == phys);
    REQUIRE(cmd.barriers[0].before == anxiety::rendering::rhi::ResourceState::Present);
    REQUIRE(cmd.barriers[0].after == anxiety::rendering::rhi::ResourceState::RenderTarget);
    REQUIRE(cmd.barriers[1].before == anxiety::rendering::rhi::ResourceState::RenderTarget);
    REQUIRE(cmd.barriers[1].after == anxiety::rendering::rhi::ResourceState::Present);
}

TEST_CASE("rg_auto_barrier_no_transition_needed", "[rendering]") {
    // Importa una textura ya en estado RenderTarget; el pase escribe como RenderTarget. No debe
    // emitirse ningún barrier.
    anxiety::rendering::graph::RenderGraph g;
    anxiety::rendering::rhi::TextureHandle phys{ 5 };
    const auto h = g.import_texture("rt", phys, anxiety::rendering::rhi::ResourceState::RenderTarget);

    g.add_pass({
        .name    = "Pass",
        .writes  = {{ h, anxiety::rendering::rhi::ResourceState::RenderTarget }},
        .execute = [](anxiety::rendering::rhi::ICommandBuffer&) {}
    });

    g.compile();
    MockCommandBuffer cmd;
    g.execute(cmd);

    REQUIRE(cmd.barriers.empty());
}

TEST_CASE("rg_auto_barrier_read_after_write", "[rendering]") {
    // P0 escribe T como RenderTarget; P1 lee T como ShaderResource.
    // El grafo debe emitir: barrier RenderTarget→ShaderResource antes de P1.
    anxiety::rendering::graph::RenderGraph g;
    anxiety::rendering::rhi::TextureHandle phys{ 3 };
    const auto T = g.import_texture("T", phys, anxiety::rendering::rhi::ResourceState::Undefined);

    g.add_pass({
        .name    = "P0",
        .writes  = {{ T, anxiety::rendering::rhi::ResourceState::RenderTarget }},
        .execute = [](anxiety::rendering::rhi::ICommandBuffer&) {}
    });
    g.add_pass({
        .name    = "P1",
        .reads   = {{ T, anxiety::rendering::rhi::ResourceState::ShaderResource }},
        .execute = [](anxiety::rendering::rhi::ICommandBuffer&) {}
    });

    g.compile();
    MockCommandBuffer cmd;
    g.execute(cmd);

    // Dos barriers: Undefined→RenderTarget (antes de P0) y RenderTarget→ShaderResource (antes de P1).
    REQUIRE(cmd.barriers.size() == 2u);
    REQUIRE(cmd.barriers[0].before == anxiety::rendering::rhi::ResourceState::Undefined);
    REQUIRE(cmd.barriers[0].after == anxiety::rendering::rhi::ResourceState::RenderTarget);
    REQUIRE(cmd.barriers[1].before == anxiety::rendering::rhi::ResourceState::RenderTarget);
    REQUIRE(cmd.barriers[1].after == anxiety::rendering::rhi::ResourceState::ShaderResource);
}

TEST_CASE("rg_final_barrier_skipped_when_undefined", "[rendering]") {
    // Importada con finalState=Undefined: no debe emitirse ningún barrier final.
    anxiety::rendering::graph::RenderGraph g;
    anxiety::rendering::rhi::TextureHandle phys{ 9 };
    // se omite finalState → por defecto Undefined
    const auto h = g.import_texture("tex", phys, anxiety::rendering::rhi::ResourceState::ShaderResource);

    g.add_pass({
        .name    = "P",
        .reads   = {{ h, anxiety::rendering::rhi::ResourceState::ShaderResource }},
        .execute = [](anxiety::rendering::rhi::ICommandBuffer&) {}
    });

    g.compile();
    MockCommandBuffer cmd;
    g.execute(cmd);

    REQUIRE(g.final_barriers().empty());
}

TEST_CASE("rg_pass_barriers_introspection", "[rendering]") {
    // Verifica que pass_barriers() devuelve los barriers precompilados de un pase dado.
    anxiety::rendering::graph::RenderGraph g;
    anxiety::rendering::rhi::TextureHandle phys{ 11 };
    const auto bb = g.import_texture("bb", phys, anxiety::rendering::rhi::ResourceState::Present, anxiety::rendering::rhi::ResourceState::Present);

    g.add_pass({
        .name    = "ClearPass",
        .writes  = {{ bb, anxiety::rendering::rhi::ResourceState::RenderTarget }},
        .execute = [](anxiety::rendering::rhi::ICommandBuffer&) {}
    });

    g.compile();

    const auto& pb = g.pass_barriers(0);
    REQUIRE(pb.size() == 1u);
    REQUIRE(pb[0].before == anxiety::rendering::rhi::ResourceState::Present);
    REQUIRE(pb[0].after == anxiety::rendering::rhi::ResourceState::RenderTarget);

    const auto& fb = g.final_barriers();
    REQUIRE(fb.size() == 1u);
    REQUIRE(fb[0].before == anxiety::rendering::rhi::ResourceState::RenderTarget);
    REQUIRE(fb[0].after == anxiety::rendering::rhi::ResourceState::Present);
}

TEST_CASE("rg_barriers_execute_order", "[rendering]") {
    // Verifica que los barriers aparecen en el log antes del callback execute del pase.
    anxiety::rendering::graph::RenderGraph g;
    anxiety::rendering::rhi::TextureHandle phys{ 20 };
    const auto bb = g.import_texture("bb", phys, anxiety::rendering::rhi::ResourceState::Present, anxiety::rendering::rhi::ResourceState::Present);

    g.add_pass({
        .name    = "ClearPass",
        .writes  = {{ bb, anxiety::rendering::rhi::ResourceState::RenderTarget }},
        .execute = [](anxiety::rendering::rhi::ICommandBuffer& cmd) { cmd.clear_render_target({}, {}); }
    });

    g.compile();
    MockCommandBuffer cmd;
    g.execute(cmd);

    // Log esperado: barrier (previo al pase), clear, barrier (final).
    REQUIRE(cmd.log.size() == 3u);
    REQUIRE(cmd.log[0] == std::string("barrier"));
    REQUIRE(cmd.log[1] == std::string("clear"));
    REQUIRE(cmd.log[2] == std::string("barrier"));
}

// Parte 5 — Pipeline + binding vía mock (solo CPU) -------------------------------------------------
TEST_CASE("rg_triangle_pass_bind_sequence", "[rendering]") {
    // Construye un render graph con un pase de triángulo y verifica el orden de las llamadas bind.
    anxiety::rendering::graph::RenderGraph g;
    anxiety::rendering::rhi::TextureHandle phys{ 30 };
    const auto rt = g.import_texture("rt", phys, anxiety::rendering::rhi::ResourceState::RenderTarget);

    MockPipeline     pipe;
    MockDescriptorSet ds;
    anxiety::rendering::rhi::BufferHandle vb{ 7 };

    g.add_pass({
        .name    = "TrianglePass",
        .writes  = {{ rt, anxiety::rendering::rhi::ResourceState::RenderTarget }},
        .execute = [&](anxiety::rendering::rhi::ICommandBuffer& cmd) {
            cmd.bind_pipeline(pipe);
            cmd.bind_descriptor_set(0, ds);
            cmd.bind_vertex_buffer(0, vb, 0, 28);
            cmd.draw(3, 1, 0, 0);
        }
        });

    g.compile();
    MockCommandBuffer cmd;
    g.execute(cmd);

    REQUIRE(cmd.log.size() == 4u);
    REQUIRE(cmd.log[0] == std::string("bind_pipeline"));
    REQUIRE(cmd.log[1] == std::string("bind_ds"));
    REQUIRE(cmd.log[2] == std::string("bind_vb"));
    REQUIRE(cmd.log[3] == std::string("draw"));
}

TEST_CASE("rg_draw_indexed_recorded", "[rendering]") {
    anxiety::rendering::graph::RenderGraph g;
    anxiety::rendering::rhi::TextureHandle phys{ 31 };
    const auto rt = g.import_texture("rt", phys, anxiety::rendering::rhi::ResourceState::RenderTarget);

    MockPipeline     pipe;
    MockDescriptorSet ds;
    anxiety::rendering::rhi::BufferHandle vb{ 8 }, ib{ 9 };

    g.add_pass({
        .name    = "IndexedPass",
        .writes  = {{ rt, anxiety::rendering::rhi::ResourceState::RenderTarget }},
        .execute = [&](anxiety::rendering::rhi::ICommandBuffer& cmd) {
            cmd.bind_pipeline(pipe);
            cmd.bind_vertex_buffer(0, vb, 0, 28);
            cmd.bind_index_buffer(ib, 0, true);
            cmd.draw_indexed(6, 1, 0, 0, 0);
        }
        });

    g.compile();
    MockCommandBuffer cmd;
    g.execute(cmd);

    REQUIRE(cmd.log.size() >= 4u);
    REQUIRE(cmd.log.back() == std::string("draw_indexed"));
}

// Parte 6 — Dispositivo DX12 (solo Windows, se omite con elegancia si no está disponible) ---------

#ifdef _WIN32
#include "backend/dx12/DX12Device.h"
#include <windows.h>

namespace {
    // Ventana Win32 oculta mínima, solo para tener un HWND real con el que crear un swapchain —
    // el mismo patrón que testing/bridge/anxiety/tests_bridge.cpp, pero en esta unidad de
    // compilación aparte (Catch2 no comparte fixtures entre ejecutables de test).
    HWND create_hidden_test_window() {
        static bool registered = false;
        const wchar_t* class_name = L"AnxietyRenderingTestWindow";

        if (!registered) {
            WNDCLASSW wc{};
            wc.lpfnWndProc = DefWindowProcW;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.lpszClassName = class_name;
            RegisterClassW(&wc);
            registered = true;
        }

        return CreateWindowExW(0, class_name, L"", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    }
}

static constexpr const char* kTestVS = R"hlsl(
struct VSIn  { float3 pos : POSITION; float4 col : COLOR; };
struct VSOut { float4 pos : SV_Position; float4 col : COLOR; };
cbuffer CB : register(b0) { float4 tint; };
VSOut VSMain(VSIn i) { VSOut o; o.pos=float4(i.pos,1); o.col=i.col*tint; return o; }
)hlsl";

static constexpr const char* kTestPS = R"hlsl(
struct VSOut { float4 pos : SV_Position; float4 col : COLOR; };
float4 PSMain(VSOut i) : SV_Target { return i.col; }
)hlsl";

TEST_CASE("rhi_dx12_device_create", "[rendering]") {
    auto dev = std::make_unique<backend::dx12::DX12Device>(false);
    // D3D12 puede no estar disponible en algunos entornos de CI — se da por válido en silencio.
    if (!dev->is_valid()) return;
    REQUIRE(dev->is_valid());
}

TEST_CASE("rhi_dx12_backend_name", "[rendering]") {
    auto dev = std::make_unique<backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    REQUIRE(dev->backend_name() == std::string_view{ "DirectX 12" });
}

TEST_CASE("rhi_dx12_compile_vertex_shader", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    auto bc = dev->compile_shader_from_source(kTestVS, "VSMain", anxiety::rendering::rhi::ShaderStage::Vertex);
    REQUIRE(!bc.empty());
}

TEST_CASE("rhi_dx12_compile_pixel_shader", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    auto bc = dev->compile_shader_from_source(kTestPS, "PSMain", anxiety::rendering::rhi::ShaderStage::Fragment);
    REQUIRE(!bc.empty());
}

TEST_CASE("rhi_dx12_create_pipeline", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;

    auto vsBc = dev->compile_shader_from_source(kTestVS, "VSMain", anxiety::rendering::rhi::ShaderStage::Vertex);
    auto psBc = dev->compile_shader_from_source(kTestPS, "PSMain", anxiety::rendering::rhi::ShaderStage::Fragment);
    if (vsBc.empty() || psBc.empty()) return;

    auto vs = dev->create_shader({ vsBc.data(), vsBc.size(), "VSMain" }, anxiety::rendering::rhi::ShaderStage::Vertex);
    auto ps = dev->create_shader({ psBc.data(), psBc.size(), "PSMain" }, anxiety::rendering::rhi::ShaderStage::Fragment);
    REQUIRE(vs != nullptr);
    REQUIRE(ps != nullptr);

    anxiety::rendering::rhi::PipelineDesc pd;
    pd.vertex_shader   = vs.get();
    pd.fragment_shader = ps.get();
    pd.vertex_layout   = {
        .attributes = {
            { "POSITION", 0, anxiety::rendering::rhi::VertexFormat::Float3, 0,  0 },
            { "COLOR",    0, anxiety::rendering::rhi::VertexFormat::Float4, 0, 12 },
        },
        .stride_bytes = 28
    };
    pd.topology             = anxiety::rendering::rhi::PrimitiveTopology::TriangleList;
    pd.rasterizer.cull_mode = anxiety::rendering::rhi::CullMode::None;
    pd.render_target_fmts   = { anxiety::rendering::rhi::Format::BGRA8_Unorm };
    pd.descriptor_layout    = { .bindings = { { 0, anxiety::rendering::rhi::DescriptorType::UniformBuffer } } };

    auto pipeline = dev->create_pipeline(pd);
    REQUIRE(pipeline != nullptr);
}

TEST_CASE("rhi_dx12_create_vertex_buffer", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;

    float verts[] = { 0.f, 0.5f, 0.f,  0.5f, -0.5f, 0.f,  -0.5f, -0.5f, 0.f };
    anxiety::rendering::rhi::BufferDesc bd{ sizeof(verts), anxiety::rendering::rhi::BufferUsage::Vertex, "TestVB" };
    auto handle = dev->create_buffer(bd, verts, sizeof(verts));
    REQUIRE(handle.is_valid());
    dev->destroy_buffer(handle);
}

TEST_CASE("rhi_dx12_create_uniform_buffer", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;

    float tint[] = { 1.f, 1.f, 1.f, 1.f };
    anxiety::rendering::rhi::BufferDesc bd{ 256, anxiety::rendering::rhi::BufferUsage::Uniform, "TestCB" };
    auto handle = dev->create_buffer(bd, tint, sizeof(tint));
    REQUIRE(handle.is_valid());
    dev->destroy_buffer(handle);
}

TEST_CASE("rhi_dx12_create_descriptor_set", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;

    anxiety::rendering::rhi::DescriptorSetLayout layout{ .bindings = { { 0, anxiety::rendering::rhi::DescriptorType::UniformBuffer } } };
    auto ds = dev->create_descriptor_set(layout);
    REQUIRE(ds != nullptr);

    float tint[] = { 1.f, 1.f, 1.f, 1.f };
    anxiety::rendering::rhi::BufferDesc bd{ 256, anxiety::rendering::rhi::BufferUsage::Uniform, "CB" };
    auto buf = dev->create_buffer(bd, tint, sizeof(tint));
    ds->update({ { 0, anxiety::rendering::rhi::DescriptorType::UniformBuffer, buf } });
    // Sin caída == éxito.
}

TEST_CASE("rhi_dx12_record_triangle_commands", "[rendering]") {
    // Graba una secuencia completa de bind de pipeline + draw sin enviarla a la GPU.
    // Verifica que no hay caída durante la grabación de comandos (la validación ocurre al ejecutar).
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;

    auto vsBc = dev->compile_shader_from_source(kTestVS, "VSMain", anxiety::rendering::rhi::ShaderStage::Vertex);
    auto psBc = dev->compile_shader_from_source(kTestPS, "PSMain", anxiety::rendering::rhi::ShaderStage::Fragment);
    if (vsBc.empty() || psBc.empty()) return;

    auto vs = dev->create_shader({ vsBc.data(), vsBc.size(), "VSMain" }, anxiety::rendering::rhi::ShaderStage::Vertex);
    auto ps = dev->create_shader({ psBc.data(), psBc.size(), "PSMain" }, anxiety::rendering::rhi::ShaderStage::Fragment);

    anxiety::rendering::rhi::PipelineDesc pd;
    pd.vertex_shader        = vs.get(); pd.fragment_shader = ps.get();
    pd.vertex_layout        = {
        .attributes = {{"POSITION",0,anxiety::rendering::rhi::VertexFormat::Float3,0,0}, {"COLOR",0,anxiety::rendering::rhi::VertexFormat::Float4,0,12}},
        .stride_bytes = 28 };
    pd.topology             = anxiety::rendering::rhi::PrimitiveTopology::TriangleList;
    pd.rasterizer.cull_mode = anxiety::rendering::rhi::CullMode::None;
    pd.render_target_fmts   = { anxiety::rendering::rhi::Format::BGRA8_Unorm };
    pd.descriptor_layout    = { .bindings = { { 0, anxiety::rendering::rhi::DescriptorType::UniformBuffer } } };
    auto pipeline = dev->create_pipeline(pd);
    if (!pipeline) return;

    struct V { float x, y, z, r, g, b, a; };
    V verts[] = { {0,0.5f,0,1,0,0,1},{0.5f,-0.5f,0,0,1,0,1},{-0.5f,-0.5f,0,0,0,1,1} };
    auto vb = dev->create_buffer({ sizeof(verts), anxiety::rendering::rhi::BufferUsage::Vertex }, verts, sizeof(verts));
    float tint[] = { 1,1,1,1 };
    auto cb = dev->create_buffer({ 256, anxiety::rendering::rhi::BufferUsage::Uniform }, tint, sizeof(tint));
    auto ds = dev->create_descriptor_set(pd.descriptor_layout);
    ds->update({ { 0, anxiety::rendering::rhi::DescriptorType::UniformBuffer, cb } });

    auto cmd = dev->create_command_buffer();
    REQUIRE(cmd != nullptr);
    cmd->begin();
    cmd->bind_pipeline(*pipeline);
    cmd->bind_descriptor_set(0, *ds);
    cmd->bind_vertex_buffer(0, vb, 0, sizeof(V));
    cmd->draw(3, 1, 0, 0);
    cmd->end();
    // Sin caída durante la grabación == éxito.

    dev->destroy_buffer(vb);
    dev->destroy_buffer(cb);
}

TEST_CASE("rhi_dx12_command_buffer_create", "[rendering]") {
    auto dev = std::make_unique<backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    auto cmd = dev->create_command_buffer();
    REQUIRE(cmd != nullptr);
}

TEST_CASE("rhi_dx12_command_buffer_begin_end", "[rendering]") {
    auto dev = std::make_unique<backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    auto cmd = dev->create_command_buffer();
    REQUIRE(cmd != nullptr);
    cmd->begin();
    cmd->end();
    // Sin caída == éxito.
}

TEST_CASE("rhi_dx12_wait_idle", "[rendering]") {
    auto dev = std::make_unique<backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    // waitIdle sobre un dispositivo inactivo no debe bloquearse (deadlock).
    dev->wait_idle();
    dev->wait_idle();   // idempotente
}

// Regresión: el heap de RTV de DX12Device tiene k_rtv_heap_size (64) slots. Antes de que
// DX12Device::free_texture_slot() liberara el RTV del backbuffer al desregistrarlo,
// DX12Swapchain::resize() (que libera y vuelve a crear los backbuffers en cada llamada) filtraba
// image_count slots por resize sin devolverlos nunca al asignador — un simple redimensionado
// continuo de un panel acababa disparando el assert "Heap de RTV agotado" hacia el resize nº 32.
// 100 resizes (bastantes más que 64 / image_count) confirma que ahora se reutilizan.
TEST_CASE("rhi_dx12_swapchain_repeated_resize_does_not_exhaust_rtv_heap", "[rendering]") {
    auto dev = std::make_unique<backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;

    HWND hwnd = create_hidden_test_window();
    REQUIRE(hwnd != nullptr);

    rhi::SwapchainDesc desc;
    desc.surface_type = rhi::NativeSurfaceType::Win32;
    desc.native_window_handle = hwnd;
    desc.extent = { 640, 480 };
    desc.image_count = 2;

    auto swapchain = dev->create_swapchain(desc);
    REQUIRE(swapchain != nullptr);

    for (int i = 0; i < 100; ++i) {
        const uint32_t w = 640 + static_cast<uint32_t>(i % 16);
        const uint32_t h = 480 + static_cast<uint32_t>(i % 16);
        swapchain->resize({ w, h });
    }

    // Si llegamos aquí sin que el assert de allocate_RTV() haya saltado, el fix funciona.
    SUCCEED();

    swapchain.reset();
    DestroyWindow(hwnd);
}

// Parte 7 — SceneRenderer de DX12 (solo Windows, se omite si no hay GPU disponible) ---------------
#include "scene/SceneComponents.h"
#include "scene/SceneMath.h"
#include "scene/SceneRenderer.h"
#include "materials/MaterialHandle.h"
#include "materials/MaterialInstance.h"
#include "materials/MaterialManager.h"
#include "World.h"

// Solo CPU: valores por defecto de los componentes -------------------------------------------------
TEST_CASE("scene_transform_default", "[rendering]") {
    anxiety::rendering::scene::Transform t;
    REQUIRE(t.position[0] == 0.0f);
    REQUIRE(t.position[1] == 0.0f);
    REQUIRE(t.position[2] == 0.0f);
    REQUIRE(t.rotation[3] == 1.0f);
    REQUIRE(t.scale[0] == 1.0f);
    REQUIRE(t.scale[1] == 1.0f);
    REQUIRE(t.scale[2] == 1.0f);
}

TEST_CASE("scene_camera_default", "[rendering]") {
    anxiety::rendering::scene::Camera cam;
    REQUIRE((cam.fov_Y > 1.f && cam.fov_Y < 1.1f));
    REQUIRE(cam.near_Z > 0.0f);
    REQUIRE(cam.far_Z > cam.near_Z);
    REQUIRE(cam.aspect_ratio > 1.0f);
}

TEST_CASE("scene_mesh_renderer_invalid_default", "[rendering]") {
    anxiety::rendering::scene::MeshRenderer mr;
    REQUIRE(!mr.vertex_buffer.is_valid());
    REQUIRE(!mr.index_buffer.is_valid());
    REQUIRE(mr.index_count == 0u);
}

// Solo CPU: SceneMath -----------------------------------------------------------------------------
TEST_CASE("scene_math_identity", "[rendering]") {
    using namespace anxiety::rendering::scene;
    Mat4 I = mat4_identity();
    REQUIRE(I.m[0][0] == 1.0f);
    REQUIRE(I.m[1][1] == 1.0f);
    REQUIRE(I.m[2][2] == 1.0f);
    REQUIRE(I.m[3][3] == 1.0f);
    REQUIRE(I.m[0][1] == 0.0f);
}

TEST_CASE("scene_math_mul_identity", "[rendering]") {
    using namespace anxiety::rendering::scene;
    Mat4 I = mat4_identity();
    Mat4 T = mat4_translate({ 1.f, 2.f, 3.f });
    Mat4 R = mat4_mul(T, I);
    REQUIRE(R.m[0][3] == 1.0f);
    REQUIRE(R.m[1][3] == 2.0f);
    REQUIRE(R.m[2][3] == 3.0f);
}

TEST_CASE("scene_math_translate", "[rendering]") {
    using namespace anxiety::rendering::scene;
    Mat4 T = mat4_translate({ 5.f, -2.f, 1.f });
    REQUIRE(T.m[0][3] == 5.0f);
    REQUIRE(T.m[1][3] == -2.0f);
    REQUIRE(T.m[2][3] == 1.0f);
    REQUIRE(T.m[3][3] == 1.0f);
}

TEST_CASE("scene_math_scale", "[rendering]") {
    using namespace anxiety::rendering::scene;
    Mat4 S = mat4_scale({ 2.f, 3.f, 4.f });
    REQUIRE(S.m[0][0] == 2.0f);
    REQUIRE(S.m[1][1] == 3.0f);
    REQUIRE(S.m[2][2] == 4.0f);
    REQUIRE(S.m[3][3] == 1.0f);
    REQUIRE(S.m[0][1] == 0.0f);
}

TEST_CASE("scene_math_perspective_lh", "[rendering]") {
    using namespace anxiety::rendering::scene;
    const float fov = 1.0472f; // 60 grados
    Mat4 P = mat4_perspective_LH(fov, 16.f / 9.f, 0.1f, 1000.f);
    // P[3][2] debe ser 1 para una perspectiva LH (w = z)
    REQUIRE(P.m[3][2] == 1.0f);
    REQUIRE(P.m[3][3] == 0.0f);
    // Las entradas de la diagonal deben ser distintas de cero
    REQUIRE(P.m[0][0] != 0.0f);
    REQUIRE(P.m[1][1] != 0.0f);
}

TEST_CASE("scene_math_lookat_lh", "[rendering]") {
    using namespace anxiety::rendering::scene;
    // Cámara en (0,0,-5) mirando al origen
    Mat4 V = mat4_look_at_LH({ 0.f,0.f,-5.f }, { 0.f,0.f,0.f }, { 0.f,1.f,0.f });
    // La matriz de vista debe tener traslación en Z = -5 (la cámara mueve el mundo -5 en Z)
    // adelante = (0,0,1), sin rotación, traslación = componente del ojo
    REQUIRE(V.m[3][3] == 1.0f);
    REQUIRE(V.m[0][0] != 0.0f);
}

// SceneRenderer de DX12 (tests de GPU) --------------------------------------------------------------
TEST_CASE("dx12_scene_renderer_create", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::scene::SceneRenderer sr(*dev);
    REQUIRE(!sr.is_ready());
    REQUIRE(sr.world() == nullptr);
}

TEST_CASE("dx12_scene_upload_mesh", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::scene::SceneRenderer sr(*dev);

    struct V { float x, y, z, r, g, b, a; };
    V verts[] = {
        { 0.f,  0.5f, 0.f, 1.f, 0.f, 0.f, 1.f },
        { 0.5f,-0.5f, 0.f, 0.f, 1.f, 0.f, 1.f },
        {-0.5f,-0.5f, 0.f, 0.f, 0.f, 1.f, 1.f },
    };
    uint32_t indices[] = { 0, 1, 2 };

    auto h = sr.upload_mesh(verts, sizeof(verts), indices, sizeof(indices));
    REQUIRE(h.vertex_buffer.is_valid());
    REQUIRE(h.index_buffer.is_valid());
    sr.destroy_mesh(h);
}

TEST_CASE("dx12_scene_build_passes_no_world", "[rendering]") {
    // build_passes sin un world adjunto debe ser un no-op.
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::scene::SceneRenderer sr(*dev);

    anxiety::rendering::graph::RenderGraph graph;
    auto bbHandle = graph.import_texture("bb", anxiety::rendering::rhi::TextureHandle{ 1 }, anxiety::rendering::rhi::ResourceState::Present, anxiety::rendering::rhi::ResourceState::Present);
    sr.build_passes(graph, bbHandle, anxiety::rendering::rhi::TextureHandle{ 1 }, { 0.f,0.f,0.f,1.f });
    // Sin un world, no debe añadirse ningún pase.
    REQUIRE(graph.pass_count() == 0u);
}

TEST_CASE("dx12_scene_build_passes_with_world", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::scene::SceneRenderer sr(*dev);

    anxiety::ecs::World world;
    sr.attach_world(&world);
    REQUIRE(sr.world() != nullptr);
    REQUIRE(sr.is_ready());

    using namespace anxiety::rendering::scene;

    auto cam = world.create_entity();
    Transform camT{};
    camT.position[2] = -3.f;
    world.add_component<Transform>(cam, camT);
    world.add_component<Camera>(cam, Camera{ 1.0472f, 0.1f, 1000.0f, 16.0f / 9.0f });

    anxiety::rendering::graph::RenderGraph graph;
    auto bbHandle = graph.import_texture("bb", anxiety::rendering::rhi::TextureHandle{ 1 }, anxiety::rendering::rhi::ResourceState::Present, anxiety::rendering::rhi::ResourceState::Present);
    sr.build_passes(graph, bbHandle, anxiety::rendering::rhi::TextureHandle{ 1 }, { 0.1f,0.18f,0.4f,1.f });
    REQUIRE(graph.pass_count() >= 1u);
}

// Parte 8 — Sistema de materiales (solo Windows, se omite si no hay GPU disponible) -------------

// Solo CPU: semántica de los handles -----------------------------------------------------------
TEST_CASE("material_handle_default_invalid", "[rendering]") {
    anxiety::rendering::materials::MaterialHandle         mh;
    anxiety::rendering::materials::MaterialInstanceHandle ih;
    REQUIRE(!mh.is_valid());
    REQUIRE(!ih.is_valid());
}

TEST_CASE("material_handle_nonzero_valid", "[rendering]") {
    anxiety::rendering::materials::MaterialHandle h{ 1 };
    REQUIRE(h.is_valid());
}

TEST_CASE("material_handle_equality", "[rendering]") {
    anxiety::rendering::materials::MaterialHandle a{ 2 }, b{ 2 }, c{ 9 };
    REQUIRE(a == b);
    REQUIRE(!(a == c));
}

// Solo CPU: MaterialInstance -----------------------------------------------------------------
TEST_CASE("material_instance_default_params", "[rendering]") {
    // MaterialInstance por defecto: color base blanco, flag dirty activo.
    anxiety::rendering::materials::MaterialInstance inst;
    const auto& p = inst.params();
    REQUIRE(p.base_color[0] == 1.0f);
    REQUIRE(p.base_color[1] == 1.0f);
    REQUIRE(p.base_color[2] == 1.0f);
    REQUIRE(p.base_color[3] == 1.0f);
    REQUIRE(inst.is_dirty());
}

TEST_CASE("material_instance_set_base_color", "[rendering]") {
    anxiety::rendering::materials::MaterialInstance inst;
    inst.clear_dirty();
    REQUIRE(!inst.is_dirty());
    inst.set_base_color(1.f, 0.f, 0.5f, 0.8f);
    REQUIRE(inst.params().base_color[0] == 1.0f);
    REQUIRE(inst.params().base_color[1] == 0.0f);
    REQUIRE(inst.params().base_color[2] == 0.5f);
    REQUIRE(inst.params().base_color[3] == 0.8f);
    REQUIRE(inst.is_dirty());
}

// DX12: MaterialManager --------------------------------------------------------------------------
TEST_CASE("dx12_material_manager_create", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::materials::MaterialManager mgr(*dev);
    // El material unlit por defecto debe cargarse correctamente desde ANXIETY_ASSETS_DIR.
    REQUIRE(mgr.default_unlit().is_valid());
}

TEST_CASE("dx12_material_manager_load_unlit", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::materials::MaterialManager mgr(*dev);
    if (!mgr.default_unlit().is_valid()) return;  // directorio de assets no disponible

    auto* mat = mgr.get_material(mgr.default_unlit());
    REQUIRE(mat != nullptr);
    REQUIRE(mat->is_valid());
    REQUIRE(mat->name() == std::string_view{ "unlit" });
}

TEST_CASE("dx12_material_manager_cached_load", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::materials::MaterialManager mgr(*dev);
    if (!mgr.default_unlit().is_valid()) return;

    // Cargar dos veces el mismo nombre debe devolver el mismo handle.
    auto h1 = mgr.load_material("unlit", "shaders/unlit.hlsl");
    auto h2 = mgr.load_material("unlit", "shaders/unlit.hlsl");
    REQUIRE(h1 == h2);
}

TEST_CASE("dx12_material_create_instance", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::materials::MaterialManager mgr(*dev);
    if (!mgr.default_unlit().is_valid()) return;

    auto inst = mgr.create_instance(mgr.default_unlit());
    REQUIRE(inst.is_valid());
    auto* ip = mgr.get_instance(inst);
    REQUIRE(ip != nullptr);
    REQUIRE(ip->material() == mgr.default_unlit());
}

TEST_CASE("dx12_material_set_base_color", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::materials::MaterialManager mgr(*dev);
    if (!mgr.default_unlit().is_valid()) return;

    auto inst = mgr.create_instance(mgr.default_unlit());
    mgr.set_base_color(inst, 1.f, 0.f, 0.f, 1.f);
    auto* ip = mgr.get_instance(inst);
    REQUIRE(ip->params().base_color[0] == 1.0f);
    REQUIRE(ip->params().base_color[1] == 0.0f);
}

TEST_CASE("dx12_material_invalid_instance_is_safe", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::materials::MaterialManager mgr(*dev);
    // Llamar con un handle inválido no debe provocar un crash.
    mgr.set_base_color({}, 1.f, 0.f, 0.f, 1.f);
    REQUIRE(mgr.get_instance({}) == nullptr);
}

TEST_CASE("dx12_scene_with_material", "[rendering]") {
    // Pipeline completo: MaterialManager + SceneRenderer + dos entidades con malla.
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::materials::MaterialManager mgr(*dev);
    if (!mgr.default_unlit().is_valid()) return;

    anxiety::rendering::scene::SceneRenderer sr(*dev, &mgr);

    anxiety::ecs::World world;
    sr.attach_world(&world);

    using namespace anxiety::rendering::scene;

    // Cámara
    auto cam = world.create_entity();
    Transform camT{};
    camT.position[2] = -3.f;
    world.add_component<Transform>(cam, camT);
    world.add_component<Camera>(cam, Camera{ 1.0472f, 0.1f, 1000.f, 16.f / 9.f });

    // Malla del triángulo
    struct V { float x, y, z, r, g, b, a; };
    V verts[] = {
        {  0.0f,  0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        {  0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
        { -0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f },
    };
    uint32_t idx[] = { 0, 1, 2 };

    auto h = sr.upload_mesh(verts, sizeof(verts), idx, sizeof(idx));
    REQUIRE(h.vertex_buffer.is_valid());
    REQUIRE(h.index_buffer.is_valid());

    // Crea dos instancias con tintes distintos
    auto red = mgr.create_instance(mgr.default_unlit());
    auto blue = mgr.create_instance(mgr.default_unlit());
    mgr.set_base_color(red, 1.0f, 0.0f, 0.0f, 1.0f);
    mgr.set_base_color(blue, 0.0f, 0.0f, 1.0f, 1.0f);

    // Entidad con malla roja
    {
        auto ent = world.create_entity();
        Transform t{};
        t.position[0] = -0.5f;
        world.add_component<Transform>(ent, t);
        MeshRenderer mr{};
        mr.vertex_buffer = h.vertex_buffer;
        mr.index_buffer = h.index_buffer;
        mr.index_count = 3;
        mr.vertex_stride = sizeof(V);
        mr.material_instance = red;
        world.add_component<MeshRenderer>(ent, mr);
    }
    // Entidad con malla azul
    {
        auto ent = world.create_entity();
        Transform t{};
        t.position[0] = 0.5f;
        world.add_component<Transform>(ent, t);
        MeshRenderer mr{};
        mr.vertex_buffer = h.vertex_buffer;
        mr.index_buffer = h.index_buffer;
        mr.index_count = 3;
        mr.vertex_stride = sizeof(V);
        mr.material_instance = blue;
        world.add_component<MeshRenderer>(ent, mr);
    }

    anxiety::rendering::graph::RenderGraph graph;
    auto bb_handle = graph.import_texture("bb", anxiety::rendering::rhi::TextureHandle{ 1 }, anxiety::rendering::rhi::ResourceState::Present, anxiety::rendering::rhi::ResourceState::Present);
    sr.build_passes(graph, bb_handle, anxiety::rendering::rhi::TextureHandle{ 1 }, { 0.1f, 0.18f, 0.4f, 1.0f });

    // Se esperan ClearPass + ScenePass (dos entidades dibujables).
    REQUIRE(graph.pass_count() == 2u);

    sr.destroy_mesh(h);
}

// Tests de texturas + sampler + importador de assets de GPU ------------------------------------

// Tests de ImageLoader en CPU (no requieren GPU) ----------------------------------------------
#include "textures/ImageLoader.h"

TEST_CASE("image_loader_empty_memory_returns_invalid", "[rendering]") {
    auto img = anxiety::rendering::textures::load_from_memory(nullptr, 0);
    REQUIRE(!img.is_valid());
}

TEST_CASE("image_loader_zero_size_returns_invalid", "[rendering]") {
    const uint8_t kDummy[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    auto img = anxiety::rendering::textures::load_from_memory(kDummy, 0);
    REQUIRE(!img.is_valid());
}

TEST_CASE("image_loader_empty_path_returns_invalid", "[rendering]") {
    auto img = anxiety::rendering::textures::load_from_file("");
    REQUIRE(!img.is_valid());
}

TEST_CASE("image_loader_nonexistent_file_returns_invalid", "[rendering]") {
    auto img = anxiety::rendering::textures::load_from_file("__no_such_file__.png");
    REQUIRE(!img.is_valid());
}

// Tests de TextureManager sobre GPU --------------------------------------------------------------
#include "textures/TextureManager.h"

TEST_CASE("dx12_texture_manager_create", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    // El constructor crea la textura nula — no debe provocar un crash.
    anxiety::rendering::textures::TextureManager mgr(*dev);
    REQUIRE(mgr.null_texture().is_valid());
}

TEST_CASE("dx12_texture_manager_load_from_memory", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::textures::TextureManager mgr(*dev);

    // Damero RGBA8 2×2
    const uint8_t kPixels[2 * 2 * 4] = {
        255,0,255,255,   0,255,255,255,
        0,255,255,255,   255,0,255,255,
    };
    auto h = mgr.load_from_memory(kPixels, 2, 2, "TestChecker");
    REQUIRE(h.is_valid());
}

TEST_CASE("dx12_texture_manager_1x1_white", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::textures::TextureManager mgr(*dev);

    auto h = mgr.load_from_memory(nullptr, 0, 0, "BadInput");
    REQUIRE(!h.is_valid());

    // La textura nula debe seguir siendo válida
    REQUIRE(mgr.null_texture().is_valid());
}

TEST_CASE("dx12_texture_manager_destroy", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::textures::TextureManager mgr(*dev);

    constexpr uint8_t kWhite[4] = { 255,255,255,255 };
    auto h = mgr.load_from_memory(kWhite, 1, 1, "TempTex");
    REQUIRE(h.is_valid());
    mgr.destroy(h);
    // Tras destroy, el handle ya no debe estar en la lista de propiedad;
    // llamar de nuevo a destroy con el mismo handle no debe provocar un crash.
    mgr.destroy(h);
}

// Tests de material con textura ---------------------------------------------------------------
TEST_CASE("dx12_material_manager_default_textured", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::materials::MaterialManager mgr(*dev);
    // default_unlit_textured puede ser inválido si falta el fichero de shader en CI;
    // el test solo comprueba que no revienta.
    if (!mgr.default_unlit_textured().is_valid()) return;
    auto* mat = mgr.get_material(mgr.default_unlit_textured());
    REQUIRE(mat != nullptr);
    REQUIRE(mat->is_valid());
    REQUIRE(mat->name() == std::string_view{ "unlit_textured" });
}

TEST_CASE("dx12_material_set_albedo_texture", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::materials::MaterialManager mgr(*dev);
    if (!mgr.default_unlit_textured().is_valid()) return;

    anxiety::rendering::textures::TextureManager tmg(*dev);

    auto inst = mgr.create_instance(mgr.default_unlit_textured());
    REQUIRE(inst.is_valid());

    auto* ip = mgr.get_instance(inst);
    REQUIRE(!ip->albedo_texture().is_valid());
    REQUIRE(ip->params().use_texture == 0);

    constexpr uint8_t kW[4] = { 255,255,255,255 };
    auto tex = tmg.load_from_memory(kW, 1, 1, "UnitTex");
    mgr.set_albedo_texture(inst, tex);

    REQUIRE(ip->albedo_texture().is_valid());
    REQUIRE(ip->params().use_texture != 0);
}

TEST_CASE("dx12_material_instance_params_size", "[rendering]") {
    // MaterialParams debe ocupar exactamente 32 bytes y caber en un CB de 256 bytes.
    static_assert(sizeof(anxiety::rendering::materials::MaterialParams) == 64);
    static_assert(sizeof(anxiety::rendering::materials::MaterialParams) <= 256);
    REQUIRE(sizeof(anxiety::rendering::materials::MaterialParams) == 64u);
}

TEST_CASE("dx12_scene_with_textured_material", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::materials::MaterialManager mgr(*dev);
    if (!mgr.default_unlit_textured().is_valid()) return;

    anxiety::rendering::textures::TextureManager tmg(*dev);
    anxiety::rendering::scene::SceneRenderer sr(*dev, &mgr, &tmg);

    anxiety::ecs::World world;
    sr.attach_world(&world);

    using namespace anxiety::rendering::scene;

    // Cámara
    auto cam = world.create_entity();
    Transform camT{}; camT.position[2] = -3.f;
    world.add_component<Transform>(cam, camT);
    world.add_component<Camera>(cam, Camera{ 1.0472f, 0.1f, 1000.f, 16.f / 9.f });

    // Vértice del quad (POSITION + COLOR + UV = 36 bytes).
    struct QV { float x, y, z, r, g, b, a, u, v; };
    QV verts[] = {
        { -0.5f,  0.5f, 0.f,  1,1,1,1,  0,0 },
        {  0.5f,  0.5f, 0.f,  1,1,1,1,  1,0 },
        {  0.5f, -0.5f, 0.f,  1,1,1,1,  1,1 },
        { -0.5f, -0.5f, 0.f,  1,1,1,1,  0,1 },
    };
    uint32_t idx[] = { 0,1,2, 0,2,3 };

    // Crea la textura
    constexpr uint8_t kPx[4] = { 255,0,255,255 };
    auto tex = tmg.load_from_memory(kPx, 1, 1, "TestTex");
    REQUIRE(tex.is_valid());

    auto inst = mgr.create_instance(mgr.default_unlit_textured());
    mgr.set_albedo_texture(inst, tex);

    auto h = sr.upload_mesh(verts, sizeof(verts), idx, sizeof(idx));
    REQUIRE(h.vertex_buffer.is_valid());

    auto ent = world.create_entity();
    Transform t{};
    world.add_component<Transform>(ent, t);
    MeshRenderer mr{};
    mr.vertex_buffer     = h.vertex_buffer;
    mr.index_buffer      = h.index_buffer;
    mr.index_count       = 6;
    mr.vertex_stride     = sizeof(QV);
    mr.material_instance = inst;
    world.add_component<MeshRenderer>(ent, mr);

    anxiety::rendering::graph::RenderGraph graph;
    auto bbHandle = graph.import_texture("bb", anxiety::rendering::rhi::TextureHandle{ 1 }, anxiety::rendering::rhi::ResourceState::Present, anxiety::rendering::rhi::ResourceState::Present);
    sr.build_passes(graph, bbHandle, anxiety::rendering::rhi::TextureHandle{ 1 }, { 0.1f, 0.18f, 0.4f, 1.0f });

    REQUIRE(graph.pass_count() == 2u);

    sr.destroy_mesh(h);
}

// Parte 10 — Sistema de materiales PBR (solo Windows, se omite si no hay GPU disponible) ---------

// Structs de los componentes de luz ----------------------------------------------------------------
TEST_CASE("pbr_directional_light_layout", "[rendering]") {
    using DL = anxiety::rendering::scene::DirectionalLight;
    static_assert(std::is_trivially_copyable_v<DL>);
    REQUIRE(sizeof(DL) == 32u);
    DL dl{};
    dl.direction[0] = 0.f; dl.direction[1] = -1.f; dl.direction[2] = 0.f;
    dl.intensity = 2.f;
    dl.color[0] = 1.f; dl.color[1] = 0.9f; dl.color[2] = 0.8f;
    REQUIRE(dl.intensity == 2.0f);
}

TEST_CASE("pbr_point_light_layout", "[rendering]") {
    using PL = anxiety::rendering::scene::PointLight;
    static_assert(std::is_trivially_copyable_v<PL>);
    REQUIRE(sizeof(PL) == 32u);
    PL pl{};
    pl.color[0] = 1.f; pl.color[1] = 0.5f; pl.color[2] = 0.f;
    pl.intensity = 5.f;
    pl.range = 10.f;
    REQUIRE(pl.range == 10.0f);
}

TEST_CASE("pbr_gpu_structs_layout", "[rendering]") {
    using namespace anxiety::rendering::scene;
    static_assert(sizeof(GpuPerObject) == 128);
    static_assert(sizeof(GpuPointLight) == 32);
    static_assert(sizeof(GpuLightsCB) == 576);
    REQUIRE(sizeof(GpuPerObject) == 128u);
    REQUIRE(sizeof(GpuPointLight) == 32u);
    REQUIRE(sizeof(GpuLightsCB) == 576u);
}

TEST_CASE("pbr_material_params_pbr_fields", "[rendering]") {
    using MP = anxiety::rendering::materials::MaterialParams;
    MP p{};
    p.metallic       = 0.8f;
    p.roughness      = 0.2f;
    p.emissive[0]    = 1.f; p.emissive[1] = 0.f; p.emissive[2] = 0.f;
    p.use_normal_tex = 1;
    p.use_orm_tex    = 0;
    REQUIRE(p.metallic == 0.8f);
    REQUIRE(p.roughness == 0.2f);
    REQUIRE(p.emissive[0] == 1.0f);
    REQUIRE(p.use_normal_tex == 1);
    REQUIRE(p.use_orm_tex == 0);
}

TEST_CASE("pbr_material_instance_setters", "[rendering]") {
    // Verifica el layout de los parámetros PBR directamente a través de MaterialParams (solo CPU).
    anxiety::rendering::materials::MaterialParams p{};
    p.metallic       = 0.5f;
    p.roughness      = 0.3f;
    p.emissive[0]    = 0.1f;
    p.emissive[1]    = 0.2f;
    p.emissive[2]    = 0.3f;
    p.use_normal_tex = 0;
    p.use_orm_tex    = 0;

    REQUIRE(p.metallic == 0.5f);
    REQUIRE(p.roughness == 0.3f);
    REQUIRE(p.emissive[1] == 0.2f);
    REQUIRE(p.use_normal_tex == 0);
    REQUIRE(p.use_orm_tex == 0);
}

TEST_CASE("dx12_material_manager_load_pbr", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::materials::MaterialManager mgr(*dev);
    REQUIRE(mgr.default_PBR().is_valid());
}

TEST_CASE("dx12_material_pbr_create_instance", "[rendering]") {
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::materials::MaterialManager mgr(*dev);
    if (!mgr.default_PBR().is_valid()) return;

    auto inst = mgr.create_instance(mgr.default_PBR());
    REQUIRE(inst.is_valid());

    mgr.set_metallic(inst, 1.0f);
    mgr.set_roughness(inst, 0.1f);
    mgr.set_emissive(inst, 0.f, 0.5f, 0.f);

    auto* ptr = mgr.get_instance(inst);
    REQUIRE(ptr != nullptr);
    REQUIRE(ptr->params().metallic == 1.0f);
    REQUIRE(ptr->params().roughness == 0.1f);
    REQUIRE(ptr->params().emissive[1] == 0.5f);
}

TEST_CASE("dx12_pbr_scene_with_lights", "[rendering]") {
    // Escena headless completa: cámara + luz direccional + luz puntual + quad PBR.
    auto dev = std::make_unique<anxiety::rendering::backend::dx12::DX12Device>(false);
    if (!dev->is_valid()) return;
    anxiety::rendering::materials::MaterialManager mgr(*dev);
    if (!mgr.default_PBR().is_valid()) return;

    anxiety::rendering::textures::TextureManager tmg(*dev);
    anxiety::rendering::scene::SceneRenderer sr(*dev, &mgr, &tmg);

    anxiety::ecs::World world;
    sr.attach_world(&world);

    using namespace anxiety::rendering::scene;

    // Cámara
    {
        auto cam = world.create_entity();
        Transform camT{}; camT.position[2] = -5.f;
        world.add_component<Transform>(cam, camT);
        world.add_component<Camera>(cam, Camera{ 1.0472f, 0.1f, 1000.f, 16.f / 9.f });
    }

    // Luz direccional
    {
        auto dlEnt = world.create_entity();
        world.add_component<Transform>(dlEnt, Transform{});
        DirectionalLight dl{};
        dl.direction[0] = 0.f; dl.direction[1] = -1.f; dl.direction[2] = 0.f;
        dl.intensity = 2.f;
        dl.color[0] = 1.f; dl.color[1] = 1.f; dl.color[2] = 1.f;
        world.add_component<DirectionalLight>(dlEnt, dl);
    }

    // Luz puntual
    {
        auto plEnt = world.create_entity();
        Transform t{}; t.position[0] = 2.f; t.position[1] = 2.f;
        world.add_component<Transform>(plEnt, t);
        PointLight pl{};
        pl.color[0] = 1.f; pl.color[1] = 0.4f; pl.color[2] = 0.1f;
        pl.intensity = 5.f; pl.range = 10.f;
        world.add_component<PointLight>(plEnt, pl);
    }

    // Quad PBR (POSITION+NORMAL+TEXCOORD+TANGENT = 48 bytes/vértice)
    struct PbrV { float px, py, pz, nx, ny, nz, u, v, tx, ty, tz, tw; };
    PbrV verts[] = {
        { -0.5f,  0.5f, 0.f,  0.f,0.f,-1.f,  0.f,0.f,  1.f,0.f,0.f,1.f },
        {  0.5f,  0.5f, 0.f,  0.f,0.f,-1.f,  1.f,0.f,  1.f,0.f,0.f,1.f },
        {  0.5f, -0.5f, 0.f,  0.f,0.f,-1.f,  1.f,1.f,  1.f,0.f,0.f,1.f },
        { -0.5f, -0.5f, 0.f,  0.f,0.f,-1.f,  0.f,1.f,  1.f,0.f,0.f,1.f },
    };
    uint32_t idx[] = { 0,1,2, 0,2,3 };

    auto mi = mgr.create_instance(mgr.default_PBR());
    mgr.set_metallic(mi, 0.5f);
    mgr.set_roughness(mi, 0.4f);

    auto h = sr.upload_mesh(verts, sizeof(verts), idx, sizeof(idx));
    REQUIRE(h.vertex_buffer.is_valid());

    {
        auto ent = world.create_entity();
        Transform t{};
        world.add_component<Transform>(ent, t);
        MeshRenderer mr{};
        mr.vertex_buffer     = h.vertex_buffer;
        mr.index_buffer      = h.index_buffer;
        mr.index_count       = 6;
        mr.vertex_stride     = sizeof(PbrV);
        mr.material_instance = mi;
        world.add_component<MeshRenderer>(ent, mr);
    }

    anxiety::rendering::graph::RenderGraph graph;
    auto bbHandle = graph.import_texture("bb", anxiety::rendering::rhi::TextureHandle{ 1 }, anxiety::rendering::rhi::ResourceState::Present, anxiety::rendering::rhi::ResourceState::Present);
    sr.build_passes(graph, bbHandle, anxiety::rendering::rhi::TextureHandle{ 1 }, { 0.05f, 0.05f, 0.05f, 1.0f });

    REQUIRE(graph.pass_count() == 2u);

    sr.destroy_mesh(h);
}

#endif // _WIN32
