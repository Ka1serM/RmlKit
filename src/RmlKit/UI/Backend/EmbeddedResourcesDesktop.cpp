#include "RmlKit/UI/Backend/EmbeddedResources.h"

#include <algorithm>
#include <format>
#include <optional>

#include "RmlKit/UI/Backend/Fatal.h"

#if PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace RmlKit::UI {

namespace {

struct Resource {
    std::string_view path;
    int id;
};

constexpr Resource resources[] = {
#include "embedded_resource_index.inc"
};

static_assert(std::ranges::is_sorted(resources, {}, &Resource::path), "The resource index must be sorted by path");

#if PLATFORM_WINDOWS

std::string_view bytes(const int id) {
    HRSRC info = FindResourceA(nullptr, MAKEINTRESOURCEA(id + 1), RT_RCDATA);
    HGLOBAL data = info ? LoadResource(nullptr, info) : nullptr;
    if (!data)
        fatal(std::format("The resource {} is missing from the executable", id + 1));
    return {static_cast<const char*>(LockResource(data)), SizeofResource(nullptr, info)};
}

#else

struct ResourceBytes {
    const char* begin;
    const char* end;
};

extern "C" const ResourceBytes resource_table[];

std::string_view bytes(const int id) {
    return {resource_table[id].begin, resource_table[id].end};
}

#endif

std::optional<int> find(const std::string_view path) {
    const auto found = std::ranges::lower_bound(resources, path, {}, &Resource::path);
    if (found == std::end(resources) || found->path != path)
        return std::nullopt;
    return found->id;
}

}

bool EmbeddedResources::exists(const std::string& path) {
    return find(path).has_value();
}

std::string_view EmbeddedResources::read(const std::string& path) {
    const std::optional<int> id = find(path);
    if (!id)
        fatal(std::format("The resource {} is not embedded", path));
    return bytes(*id);
}

std::vector<std::string> EmbeddedResources::list(const std::string& directory) {
    const std::string prefix = directory + "/";
    std::vector<std::string> names;
    for (const Resource& resource : resources) {
        if (!resource.path.starts_with(prefix))
            continue;
        const std::string_view name = resource.path.substr(prefix.size());
        if (name.contains('/'))
            continue;
        names.emplace_back(name);
    }
    return names;
}

std::vector<std::string> EmbeddedResources::files(const std::string& directory) {
    const std::string prefix = directory + "/";
    std::vector<std::string> paths;
    for (auto resource = std::ranges::lower_bound(resources, prefix, {}, &Resource::path);
         resource != std::end(resources) && resource->path.starts_with(prefix); ++resource)
        paths.emplace_back(resource->path.substr(prefix.size()));
    return paths;
}

}
