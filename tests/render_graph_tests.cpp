#include <vk_render_graph.h>

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

template<class Function>
void rejects(Function&& function, const std::string& fragment)
{
    try {
        function();
    } catch (const std::exception& error) {
        check(std::string(error.what()).find(fragment) != std::string::npos,
              "Exception did not identify the expected failure");
        return;
    }
    throw std::runtime_error("Expected an exception");
}

struct MockCommands final : rhi::CommandList {
    struct Transition {
        rhi::Resource resource;
        rhi::ResourceState state;
    };
    std::vector<std::string> events;
    std::vector<Transition> transitions;
    int labelDepth = 0;
    bool failTransition = false;

    void transition(rhi::Resource resource, rhi::ResourceState state) override {
        events.push_back("transition");
        if (failTransition) throw std::runtime_error("transition failed");
        transitions.push_back({resource, state});
    }
    void begin_label(const char* name) override {
        events.push_back(std::string("begin:") + name);
        ++labelDepth;
    }
    void end_label() override {
        events.push_back("end");
        --labelDepth;
    }
    void bind_pipeline(rhi::Pipeline) override {}
    void bind_descriptor_set(rhi::Pipeline, uint32_t, rhi::DescriptorSet) override {}
    void push_constants(rhi::Pipeline, rhi::Stage, const void*, uint32_t) override {}
    void dispatch(uint32_t, uint32_t, uint32_t) override { events.push_back("dispatch"); }
    void trace_rays(const rhi::ShaderBindingTable&, uint32_t, uint32_t, uint32_t) override {}
    void draw(uint32_t, uint32_t, uint32_t, uint32_t) override {}
    void copy_image(rhi::Resource, rhi::Resource, uint32_t, uint32_t, uint32_t) override {}
    void fill_buffer(rhi::Resource, uint32_t) override {}
    void begin_render_pass(rhi::RenderTarget, const rhi::ClearValues&) override {}
    void end_render_pass() override {}
    void timestamp(rhi::QueryPool, uint32_t, rhi::Stage) override { events.push_back("timestamp"); }
};

const auto no_op = [](rhi::CommandList&) {};
rg::Use access(rg::ResourceHandle resource, rhi::Access flags,
               rhi::Stage stage = rhi::Stage::Compute,
               rhi::Layout layout = rhi::Layout::General)
{
    return {resource, {stage, flags, layout}};
}

void records_real_work_in_dependency_order()
{
    rg::RenderGraph graph;
    const rhi::Resource native{41, rhi::ResourceKind::Image};
    const auto image = graph.import_resource("lighting", native, false);
    graph.add_pass("trace", {access(image, rhi::Access::ShaderWrite, rhi::Stage::RayTracing)},
                   [](rhi::CommandList& commands) { commands.dispatch(1, 1, 1); });
    graph.add_pass("shade", {access(image, rhi::Access::ShaderRead)},
                   [](rhi::CommandList& commands) { commands.dispatch(1, 1, 1); });
    graph.add_pass("timing", {}, [](rhi::CommandList& commands) {
        commands.timestamp({1}, 2, rhi::Stage::Bottom);
    });
    graph.compile();
    MockCommands commands;
    graph.execute(commands);
    const std::vector<std::string> expected{
        "begin:trace", "transition", "dispatch", "end",
        "begin:shade", "transition", "dispatch", "end",
        "begin:timing", "timestamp", "end"
    };
    check(commands.events == expected, "Transitions and actual commands must execute inside pass labels");
    check(commands.transitions.size() == 2 && commands.transitions[0].resource == native,
          "Execution must transition the imported physical resource");
    check(commands.transitions[0].state.stages == rhi::Stage::RayTracing,
          "Ray-tracing stage must not become compute or fragment");
    check(commands.labelDepth == 0, "Labels must be balanced");
}

void stable_topological_sort()
{
    rg::RenderGraph graph;
    check(graph.pass_count() == 0, "A new graph must report no declared passes");
    const auto a = graph.add_pass("A", {}, no_op);
    graph.add_pass("B", {}, no_op);
    const auto c = graph.add_pass("C", {}, no_op);
    check(graph.pass_count() == 3, "Pass count must be available before compilation");
    graph.depends_on(a, c);
    graph.depends_on(a, c);
    graph.compile();
    check(graph.pass_names() == std::vector<std::string>{"B", "C", "A"},
          "Explicit dependencies must reorder work with stable ready-pass ordering");
    const auto firstDot = graph.export_dot();
    graph.compile();
    check(graph.export_dot() == firstDot, "Recompilation must not append old physical state or edges");
    graph.reset();
    check(graph.pass_count() == 0, "Reset must clear the declared pass count");
}

void rejects_uninitialized_reads_and_read_modify_write()
{
    for (const auto flags : {rhi::Access::ShaderRead,
                            rhi::Access::ShaderRead | rhi::Access::ShaderWrite}) {
        rg::RenderGraph graph;
        const auto image = graph.import_resource("uninitialized history", {1, rhi::ResourceKind::Image}, false);
        graph.add_pass("temporal reuse", {access(image, flags)}, no_op);
        rejects([&] { graph.compile(); }, "uninitialized history");
        check(!graph.is_compiled(), "Failed compile must invalidate any execution plan");
        MockCommands commands;
        rejects([&] { graph.execute(commands); }, "compiled");
        check(commands.events.empty(), "Failed compilation must prevent partial execution");
    }
    rg::RenderGraph graph;
    const auto buffer = graph.import_resource("gradient", {2, rhi::ResourceKind::Buffer}, false);
    graph.add_pass("initialize", {access(buffer, rhi::Access::TransferWrite, rhi::Stage::Transfer)}, no_op);
    graph.add_pass("train", {access(buffer, rhi::Access::ShaderRead | rhi::Access::ShaderWrite)}, no_op);
    graph.compile();
    check(graph.pass_names() == std::vector<std::string>{"initialize", "train"},
          "A prior write must initialize a resource for read-modify-write");
}

void all_access_kinds_participate_in_hazards()
{
    const std::vector<rhi::Access> reads{
        rhi::Access::ShaderRead, rhi::Access::UniformRead, rhi::Access::ColorRead,
        rhi::Access::DepthRead, rhi::Access::TransferRead, rhi::Access::HostRead,
        rhi::Access::VertexRead, rhi::Access::IndexRead, rhi::Access::IndirectRead,
        rhi::Access::MemoryRead
    };
    for (const auto read : reads) {
        rg::RenderGraph graph;
        const auto resource = graph.import_resource("shared", {1, rhi::ResourceKind::Buffer});
        const auto reader = graph.add_pass("reader", {access(resource, read, rhi::Stage::AllCommands)}, no_op);
        const auto writer = graph.add_pass("writer", {access(resource, rhi::Access::ShaderWrite)}, no_op);
        graph.depends_on(reader, writer);
        rejects([&] { graph.compile(); }, "cycle");
    }
    const std::vector<rhi::Access> writes{
        rhi::Access::ShaderWrite, rhi::Access::ColorWrite, rhi::Access::DepthWrite,
        rhi::Access::TransferWrite, rhi::Access::HostWrite, rhi::Access::MemoryWrite
    };
    for (const auto write : writes) {
        rg::RenderGraph graph;
        const auto resource = graph.import_resource("shared", {1, rhi::ResourceKind::Buffer}, false);
        const auto writer = graph.add_pass("writer", {access(resource, write, rhi::Stage::AllCommands)}, no_op);
        const auto reader = graph.add_pass("reader", {access(resource, rhi::Access::ShaderRead)}, no_op);
        graph.depends_on(writer, reader);
        rejects([&] { graph.compile(); }, "cycle");
    }
}

void aliases_share_all_readers_and_writers()
{
    rg::RenderGraph graph;
    const rhi::Resource native{17, rhi::ResourceKind::Image};
    const auto source = graph.import_resource("accumulation output", native);
    const auto alias = graph.import_resource("denoiser source", native);
    check(source == alias, "Imported aliases must canonicalize by physical identity");
    const auto buffer = graph.import_resource("same numeric id, another kind", {17, rhi::ResourceKind::Buffer});
    check(buffer != source, "Image and buffer IDs must use separate identities");
    const auto first = graph.add_pass("display", {access(source, rhi::Access::ShaderRead)}, no_op);
    const auto second = graph.add_pass("denoise", {access(alias, rhi::Access::ShaderRead)}, no_op);
    const auto writer = graph.add_pass("overwrite", {access(alias, rhi::Access::ShaderWrite)}, no_op);
    graph.compile();
    const auto dot = graph.export_dot();
    check(dot.find("p0 -> p2") != std::string::npos && dot.find("p1 -> p2") != std::string::npos,
          "Overwrite must wait for every reader through every alias");
    graph.depends_on(first, writer);
    rejects([&] { graph.compile(); }, "cycle");
    (void)second;

    graph.reset();
    const auto image = graph.import_resource("image", native, false);
    const auto w0 = graph.add_pass("write0", {access(image, rhi::Access::ShaderWrite)}, no_op);
    const auto w1 = graph.add_pass("write1", {access(image, rhi::Access::ShaderWrite)}, no_op);
    graph.depends_on(w0, w1);
    rejects([&] { graph.compile(); }, "cycle");
}

void validates_declarations_without_damaging_a_valid_plan()
{
    rg::RenderGraph graph;
    const rhi::Resource native{3, rhi::ResourceKind::Buffer};
    const auto buffer = graph.import_resource("buffer", native);
    const auto pass = graph.add_pass("valid", {access(buffer, rhi::Access::ShaderRead)}, no_op);
    graph.compile();
    rejects([&] { graph.add_pass("valid", {}, no_op); }, "duplicate");
    rejects([&] { graph.add_pass("no callback", {}, {}); }, "recording");
    rejects([&] { graph.add_pass("bad resource", {access(99, rhi::Access::ShaderRead)}, no_op); }, "Unknown resource");
    rejects([&] { graph.add_pass("bad stage", {access(buffer, rhi::Access::ShaderRead, rhi::Stage::None)}, no_op); }, "stage");
    rejects([&] { graph.depends_on(pass, 99); }, "Unknown");
    rejects([&] { graph.depends_on(pass, pass); }, "itself");
    rejects([&] { graph.import_resource("buffer", {4, rhi::ResourceKind::Buffer}); }, "another");
    check(graph.import_resource("alias", native, false) == buffer,
          "Later aliases must preserve the first import's initialization state");
    check(graph.is_compiled(), "Rejected declaration must leave the previous valid graph intact");
    check(graph.import_resource("alias", native) == buffer, "A valid alias should reuse the import");
    check(graph.is_compiled(), "A new alias alone must not invalidate the execution plan");
    graph.add_pass("new pass", {}, no_op);
    check(!graph.is_compiled(), "A new pass must invalidate the execution plan");
    rejects([&] { graph.pass_names(); }, "compiled");
    rejects([&] { graph.export_dot(); }, "compiled");
}

void merges_compatible_uses_and_rejects_layout_conflicts()
{
    rg::RenderGraph graph;
    const auto image = graph.import_resource("image", {1, rhi::ResourceKind::Image});
    rejects([&] {
        graph.add_pass("conflicting layouts",
            {access(image, rhi::Access::ShaderRead),
             access(image, rhi::Access::ShaderWrite, rhi::Stage::Compute, rhi::Layout::TransferDestination)}, no_op);
    }, "Conflicting");
    graph.add_pass("combined use",
        {access(image, rhi::Access::ShaderRead, rhi::Stage::RayTracing),
         access(image, rhi::Access::ShaderWrite, rhi::Stage::Compute)}, no_op);
    graph.compile();
    MockCommands commands;
    graph.execute(commands);
    check(commands.transitions.size() == 1, "One pass must have one merged transition per resource");
    check(commands.transitions[0].state.access == (rhi::Access::ShaderRead | rhi::Access::ShaderWrite),
          "Merged transition must preserve read and write access");
    check(commands.transitions[0].state.stages == (rhi::Stage::RayTracing | rhi::Stage::Compute),
          "Merged transition must preserve every stage");
}

void alias_import_cannot_initialize_an_unwritten_output()
{
    rg::RenderGraph graph;
    const rhi::Resource native{7, rhi::ResourceKind::Image};
    const auto producer = graph.import_resource("producer output", native, false);
    const auto consumer = graph.import_resource("consumer input", native);
    check(producer == consumer, "Output/input aliases must share a handle");
    check(graph.import_resource("producer output", native) == producer,
          "Reimporting the same name must preserve the original initialization");
    graph.add_pass("consumer", {access(consumer, rhi::Access::ShaderRead)}, no_op);
    rejects([&] { graph.compile(); }, "uninitialized");
}

void cycles_reset_and_backend_state_separation()
{
    rg::RenderGraph graph;
    MockCommands commands;
    const auto image = graph.import_resource("old name", {9, rhi::ResourceKind::Image});
    graph.add_pass("old frame", {access(image, rhi::Access::ShaderWrite)}, no_op);
    graph.compile();
    graph.execute(commands);
    const auto recorded = commands.events;
    graph.reset();
    check(commands.events == recorded && commands.transitions.size() == 1,
          "Graph reset must not call or clear the persistent backend");
    graph.compile();
    check(graph.pass_names().empty(), "Reset must remove all old passes");
    graph.execute(commands);
    check(commands.events == recorded, "Empty graph must execute no work");

    const auto a = graph.add_pass("cycle A", {}, no_op);
    const auto b = graph.add_pass("cycle B", {}, no_op);
    graph.depends_on(a, b);
    graph.depends_on(b, a);
    rejects([&] { graph.compile(); }, "cycle A");
    check(!graph.is_compiled(), "An explicit cycle must leave no executable plan");
    graph.reset();
    graph.add_pass("recovered", {}, no_op);
    graph.compile();
    check(graph.pass_names() == std::vector<std::string>{"recovered"}, "Reset must recover from a failed compile");
}

void resource_lookup_survives_growth_and_resets()
{
    rg::RenderGraph graph;
    constexpr uint32_t count = 2048;
    for (uint32_t id = 0; id < count; ++id) {
        graph.import_resource("image " + std::to_string(id), {id, rhi::ResourceKind::Image}, false);
        graph.import_resource("buffer " + std::to_string(id), {id, rhi::ResourceKind::Buffer});
    }
    for (uint32_t id = count; id-- > 0;) {
        const auto image = graph.import_resource("image alias " + std::to_string(id), {id, rhi::ResourceKind::Image});
        const auto buffer = graph.import_resource("buffer alias " + std::to_string(id), {id, rhi::ResourceKind::Buffer});
        check(image == id * 2 && buffer == id * 2 + 1,
              "Resource identity must survive lookup-table growth and include the resource kind");
    }
    std::vector<rg::Use> writes;
    std::vector<rg::Use> reads;
    for (uint32_t handle = 0; handle < count * 2; ++handle) {
        writes.push_back(access(handle, rhi::Access::ShaderWrite));
        reads.push_back(access(handle, rhi::Access::ShaderRead));
    }
    for (uint32_t handle = count * 2; handle-- > 0;)
        reads.push_back(access(handle, rhi::Access::ShaderRead, rhi::Stage::RayTracing));
    graph.add_pass("initialize scene resources", std::move(writes), no_op);
    graph.add_pass("read scene with aliases", std::move(reads), no_op);
    graph.compile();
    MockCommands commands;
    graph.execute(commands);
    check(commands.transitions.size() == count * 4,
          "Large duplicate use lists must transition every resource only once per pass");
    for (uint32_t handle = 0; handle < count * 2; ++handle) {
        const auto& transition = commands.transitions[count * 2 + handle];
        check(transition.resource.id == handle / 2 &&
              transition.resource.kind == (handle % 2 ? rhi::ResourceKind::Buffer : rhi::ResourceKind::Image),
              "Merging duplicate uses must preserve first-use order");
        check(transition.state.stages == (rhi::Stage::Compute | rhi::Stage::RayTracing),
              "Merging large alias lists must retain every reader stage");
    }
    graph.reset();
    // Reimport in a different order: stale physical handles must not survive reset.
    const auto first = graph.import_resource("image alias 2047", {count - 1, rhi::ResourceKind::Image}, false);
    const auto next = graph.import_resource("buffer 0", {0, rhi::ResourceKind::Buffer});
    check(first == 0 && next == 1, "Reset must clear physical and name lookups together");
    graph.add_pass("read newly uninitialized import", {access(first, rhi::Access::ShaderRead)}, no_op);
    rejects([&] { graph.compile(); }, "uninitialized");
}

void callback_errors_balance_labels_and_allow_recovery()
{
    rg::RenderGraph graph;
    MockCommands commands;
    graph.add_pass("throws", {}, [](rhi::CommandList&) { throw std::runtime_error("recording failed"); });
    graph.add_pass("must not run", {}, [](rhi::CommandList& cmd) { cmd.dispatch(1, 1, 1); });
    graph.compile();
    rejects([&] { graph.execute(commands); }, "recording failed");
    check(commands.labelDepth == 0 && commands.events == std::vector<std::string>{"begin:throws", "end"},
          "Callback failure must close its label and stop later passes");
    graph.reset();
    graph.add_pass("illegal mutation", {}, [&](rhi::CommandList&) { graph.reset(); });
    graph.compile();
    rejects([&] { graph.execute(commands); }, "during execution");
    check(commands.labelDepth == 0, "Mutation failure must also balance labels");
    graph.reset();

    const auto image = graph.import_resource("image", {1, rhi::ResourceKind::Image});
    graph.add_pass("transition throws", {access(image, rhi::Access::ShaderRead)}, no_op);
    graph.compile();
    commands.failTransition = true;
    rejects([&] { graph.execute(commands); }, "transition failed");
    check(commands.labelDepth == 0, "Transition failure must close its pass label");
    graph.reset();
}

void dot_escapes_debug_names()
{
    rg::RenderGraph graph;
    const auto resource = graph.import_resource("image \"quoted\"\\path", {1, rhi::ResourceKind::Image});
    graph.add_pass("write \"quoted\"\nline", {access(resource, rhi::Access::ShaderWrite)}, no_op);
    graph.add_pass("read", {access(resource, rhi::Access::ShaderRead)}, no_op);
    graph.compile();
    const auto dot = graph.export_dot();
    check(dot.find("write \\\"quoted\\\"\\nline") != std::string::npos, "DOT pass labels must escape quotes/newlines");
    check(dot.find("image \\\"quoted\\\"\\\\path") != std::string::npos, "DOT resource labels must escape quotes/backslashes");
}
} // namespace

int main()
{
    const std::vector<std::pair<const char*, void(*)()>> tests{
        {"execution", records_real_work_in_dependency_order},
        {"stable topological order", stable_topological_sort},
        {"initialization", rejects_uninitialized_reads_and_read_modify_write},
        {"access hazards", all_access_kinds_participate_in_hazards},
        {"resource aliases and WAR/WAW", aliases_share_all_readers_and_writers},
        {"declaration validation", validates_declarations_without_damaging_a_valid_plan},
        {"duplicate resource uses", merges_compatible_uses_and_rejects_layout_conflicts},
        {"alias initialization", alias_import_cannot_initialize_an_unwritten_output},
        {"cycles and reset", cycles_reset_and_backend_state_separation},
        {"physical resource lookup", resource_lookup_survives_growth_and_resets},
        {"execution errors", callback_errors_balance_labels_and_allow_recovery},
        {"DOT escaping", dot_escapes_debug_names}
    };
    for (const auto& [name, test] : tests) {
        try { test(); }
        catch (const std::exception& error) {
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
            return 1;
        }
    }
    std::cout << "PASS: " << tests.size() << " render graph tests\n";
    return 0;
}
