#include <vk_render_graph.h>

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <sstream>
#include <stdexcept>

namespace rg {
namespace {
std::string dot_escape(const std::string& text)
{
    std::string escaped;
    for (char character : text) {
        switch (character) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped += character; break;
        }
    }
    return escaped;
}
} // namespace

void RenderGraph::require_mutable() const
{
    if (_executing)
        throw std::logic_error("Cannot mutate or compile a render graph during execution.");
}

void RenderGraph::require_compiled() const
{
    if (!_compiled)
        throw std::logic_error("Render graph must be compiled after its last change.");
}

void RenderGraph::invalidate() noexcept
{
    _compiled = false;
    _order.clear();
    _edges.clear();
}

void RenderGraph::reset()
{
    require_mutable();
    invalidate();
    _resources.clear();
    _resourceNames.clear();
    _resourceHandles.clear();
    _passes.clear();
    _passNames.clear();
    _dependencies.clear();
}

ResourceHandle RenderGraph::import_resource(std::string name, rhi::Resource resource,
                                          bool initialized)
{
    require_mutable();
    if (name.empty())
        throw std::invalid_argument("Render graph resource name cannot be empty.");

    const auto named = _resourceNames.find(name);
    if (named != _resourceNames.end() && !(_resources[named->second].native == resource))
        throw std::invalid_argument("Resource name refers to another imported resource: " + name);

    const auto physical = _resourceHandles.find(resource);
    if (physical != _resourceHandles.end()) {
        // An alias must have the same hazard history as the original import.
        // The first import owns initialization; consumers commonly import its
        // output again with the default argument. Never let that upgrade state.
        // Adding another name does not change the compiled plan.
        _resourceNames.emplace(std::move(name), physical->second);
        return physical->second;
    }

    if (_resources.size() >= std::numeric_limits<ResourceHandle>::max())
        throw std::length_error("Too many render graph resources.");
    const auto handle = ResourceHandle(_resources.size());
    _resources.push_back({name, resource, initialized});
    try {
        _resourceHandles.emplace(resource, handle);
        _resourceNames.emplace(std::move(name), handle);
    } catch (...) {
        _resourceHandles.erase(resource);
        _resources.pop_back();
        throw;
    }
    invalidate();
    return handle;
}

PassHandle RenderGraph::add_pass(std::string name, std::vector<Use> uses, Record record)
{
    require_mutable();
    if (name.empty() || _passNames.contains(name))
        throw std::invalid_argument("Empty or duplicate render graph pass name: " + name);
    if (!record)
        throw std::invalid_argument("Render graph pass has no recording function: " + name);

    std::vector<Use> merged;
    merged.reserve(uses.size());
    std::unordered_map<ResourceHandle, std::size_t> mergedIndices;
    mergedIndices.reserve(uses.size());
    for (const auto& use : uses) {
        if (use.resource >= _resources.size())
            throw std::out_of_range("Unknown resource in render graph pass: " + name);
        if (use.state.access != rhi::Access::None && use.state.stages == rhi::Stage::None)
            throw std::invalid_argument("Resource access has no pipeline stage in pass: " + name);
        const auto [position, inserted] = mergedIndices.emplace(use.resource, merged.size());
        if (inserted) {
            merged.push_back(use);
        } else {
            auto& previous = merged[position->second];
            if (previous.state.layout != use.state.layout)
                throw std::invalid_argument("Conflicting resource layouts in pass " + name + ": " +
                                            _resources[use.resource].name);
            previous.state.stages = previous.state.stages | use.state.stages;
            previous.state.access = previous.state.access | use.state.access;
        }
    }

    if (_passes.size() >= std::numeric_limits<PassHandle>::max())
        throw std::length_error("Too many render graph passes.");
    const auto handle = PassHandle(_passes.size());
    _passes.push_back({name, std::move(merged), std::move(record)});
    try {
        _passNames.emplace(std::move(name), handle);
    } catch (...) {
        _passes.pop_back();
        throw;
    }
    invalidate();
    return handle;
}

void RenderGraph::depends_on(PassHandle after, PassHandle before)
{
    require_mutable();
    if (after >= _passes.size() || before >= _passes.size())
        throw std::out_of_range("Unknown render graph pass in dependency.");
    if (after == before)
        throw std::invalid_argument("A render graph pass cannot depend on itself: " + _passes[after].name);
    const auto dependency = std::make_pair(before, after);
    if (std::find(_dependencies.begin(), _dependencies.end(), dependency) == _dependencies.end()) {
        _dependencies.push_back(dependency);
        invalidate();
    }
}

void RenderGraph::compile()
{
    require_mutable();
    invalidate();
    struct History {
        std::optional<PassHandle> writer;
        std::vector<PassHandle> readers;
        bool initialized = false;
    };
    std::vector<History> histories(_resources.size());
    for (size_t index = 0; index < _resources.size(); ++index)
        histories[index].initialized = _resources[index].initialized;

    std::map<std::pair<PassHandle, PassHandle>, Edge> edges;
    auto add_edge = [&](PassHandle before, PassHandle after,
                        std::optional<ResourceHandle> resource) {
        if (before == after) return;
        auto entry = edges.try_emplace(std::make_pair(before, after),
                                      Edge{before, after, {}, false}).first;
        auto& edge = entry->second;
        if (resource) {
            if (std::find(edge.resources.begin(), edge.resources.end(), *resource) == edge.resources.end())
                edge.resources.push_back(*resource);
        } else {
            edge.explicitDependency = true;
        }
    };

    // Reads consume the most recently declared version. Writes wait for its
    // producer and every reader, then begin a new version of the same resource.
    for (PassHandle pass = 0; pass < _passes.size(); ++pass) {
        for (const auto& use : _passes[pass].uses) {
            auto& history = histories[use.resource];
            const bool read = rhi::reads(use.state.access);
            const bool write = rhi::writes(use.state.access);
            if (read && !history.initialized)
                throw std::logic_error("Read of uninitialized resource '" + _resources[use.resource].name +
                                       "' in pass '" + _passes[pass].name + "'.");
            if ((read || write) && history.writer)
                add_edge(*history.writer, pass, use.resource);
            if (write) {
                for (const auto reader : history.readers)
                    add_edge(reader, pass, use.resource);
                history.readers.clear();
                history.writer = pass;
                history.initialized = true;
            } else if (read) {
                history.readers.push_back(pass);
            }
        }
    }
    for (const auto& [before, after] : _dependencies)
        add_edge(before, after, std::nullopt);

    std::vector<std::vector<PassHandle>> outgoing(_passes.size());
    std::vector<size_t> incoming(_passes.size(), 0);
    for (const auto& [key, edge] : edges) {
        outgoing[edge.before].push_back(edge.after);
        ++incoming[edge.after];
    }
    std::priority_queue<PassHandle, std::vector<PassHandle>, std::greater<PassHandle>> ready;
    for (PassHandle pass = 0; pass < _passes.size(); ++pass)
        if (incoming[pass] == 0) ready.push(pass);

    std::vector<PassHandle> order;
    order.reserve(_passes.size());
    while (!ready.empty()) {
        const auto pass = ready.top();
        ready.pop();
        order.push_back(pass);
        for (const auto next : outgoing[pass])
            if (--incoming[next] == 0) ready.push(next);
    }
    if (order.size() != _passes.size()) {
        std::string names;
        for (PassHandle pass = 0; pass < _passes.size(); ++pass)
            if (incoming[pass] != 0)
                names += (names.empty() ? "" : ", ") + _passes[pass].name;
        throw std::logic_error("Render graph dependency cycle; blocked passes: " + names);
    }

    std::vector<Edge> compiledEdges;
    compiledEdges.reserve(edges.size());
    for (auto& [key, edge] : edges) compiledEdges.push_back(std::move(edge));
    _order = std::move(order);
    _edges = std::move(compiledEdges);
    _compiled = true;
}

void RenderGraph::execute(rhi::CommandList& commands)
{
    require_mutable();
    require_compiled();
    _executing = true;
    struct ExecutionGuard {
        bool& executing;
        ~ExecutionGuard() { executing = false; }
    } guard{_executing};

    for (const auto index : _order) {
        const auto& pass = _passes[index];
        commands.begin_label(pass.name.c_str());
        try {
            for (const auto& use : pass.uses)
                commands.transition(_resources[use.resource].native, use.state);
            pass.record(commands);
        } catch (...) {
            // Keep debug labels balanced without replacing the original error.
            try { commands.end_label(); } catch (...) {}
            throw;
        }
        commands.end_label();
    }
}

std::vector<std::string> RenderGraph::pass_names() const
{
    require_compiled();
    std::vector<std::string> names;
    names.reserve(_order.size());
    for (const auto pass : _order) names.push_back(_passes[pass].name);
    return names;
}

std::string RenderGraph::export_dot() const
{
    require_compiled();
    std::ostringstream dot;
    dot << "digraph RenderGraph {\n  rankdir=LR;\n";
    for (const auto pass : _order)
        dot << "  p" << pass << " [label=\"" << dot_escape(_passes[pass].name) << "\"];\n";
    for (const auto& edge : _edges) {
        std::string label = edge.explicitDependency ? "explicit" : "";
        for (const auto resource : edge.resources) {
            if (!label.empty()) label += ", ";
            label += _resources[resource].name;
        }
        dot << "  p" << edge.before << " -> p" << edge.after
            << " [label=\"" << dot_escape(label) << "\"];\n";
    }
    dot << "}\n";
    return dot.str();
}
} // namespace rg
