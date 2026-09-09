#pragma once

#include <rhi/rhi.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// A single-queue frame graph. Resource versions follow pass declaration order;
// explicit dependencies can order otherwise independent work. Native resource
// allocation, lifetime and state between frames belong to the RHI backend.
namespace rg {
using ResourceHandle = uint32_t;
using PassHandle = uint32_t;

struct Use {
    ResourceHandle resource;
    rhi::ResourceState state;
};

class RenderGraph {
public:
    using Record = std::function<void(rhi::CommandList&)>;

    // Reset declarations for a new frame; never reset backend resource states.
    void reset();
    // The first import defines initialization. Later aliases share its handle
    // and cannot upgrade an uninitialized resource merely by importing it again.
    ResourceHandle import_resource(std::string name, rhi::Resource resource,
                                   bool initialized = true);
    PassHandle add_pass(std::string name, std::vector<Use> uses, Record record);
    void depends_on(PassHandle after, PassHandle before);

    // A failed compile leaves no executable plan. Successful graph mutations
    // invalidate the plan; execute/debug queries require a fresh compile.
    void compile();
    void execute(rhi::CommandList& commands);
    std::string export_dot() const;
    std::vector<std::string> pass_names() const;
    std::size_t pass_count() const noexcept { return _passes.size(); }
    bool is_compiled() const noexcept { return _compiled; }

private:
    struct ResourceHash {
        std::size_t operator()(rhi::Resource resource) const noexcept {
            const auto idHash = std::hash<uint64_t>{}(resource.id);
            const auto kindHash = std::hash<uint8_t>{}(static_cast<uint8_t>(resource.kind));
            return idHash ^ (kindHash + std::size_t(0x9e3779b9u) + (idHash << 6) + (idHash >> 2));
        }
    };
    struct ResourceInfo {
        std::string name;
        rhi::Resource native;
        bool initialized;
    };
    struct Pass {
        std::string name;
        std::vector<Use> uses;
        Record record;
    };
    struct Edge {
        PassHandle before;
        PassHandle after;
        std::vector<ResourceHandle> resources;
        bool explicitDependency = false;
    };

    void require_mutable() const;
    void require_compiled() const;
    void invalidate() noexcept;

    std::vector<ResourceInfo> _resources;
    std::unordered_map<std::string, ResourceHandle> _resourceNames;
    std::unordered_map<rhi::Resource, ResourceHandle, ResourceHash> _resourceHandles;
    std::vector<Pass> _passes;
    std::unordered_map<std::string, PassHandle> _passNames;
    std::vector<std::pair<PassHandle, PassHandle>> _dependencies;
    std::vector<PassHandle> _order;
    std::vector<Edge> _edges;
    bool _compiled = false;
    bool _executing = false;
};
} // namespace rg
