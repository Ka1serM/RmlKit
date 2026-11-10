#include "RmlKit/UI/Backend/EmbeddedResources.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <mutex>
#include <unordered_map>

#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

namespace {

std::filesystem::path location(const std::string& path) {
    return std::filesystem::path("/") / path;
}

std::mutex mutex;
std::unordered_map<std::string, std::string> loaded;

}

bool EmbeddedResources::exists(const std::string& path) {
    return std::filesystem::is_regular_file(location(path));
}

std::string_view EmbeddedResources::read(const std::string& path) {
    const std::scoped_lock lock(mutex);
    if (const auto found = loaded.find(path); found != loaded.end())
        return found->second;
    std::ifstream stream(location(path), std::ios::binary);
    if (!stream)
        fatal(std::format("The resource {} is not embedded", path));
    return loaded.emplace(path, std::string(std::istreambuf_iterator<char>(stream), {})).first->second;
}

std::vector<std::string> EmbeddedResources::list(const std::string& directory) {
    std::vector<std::string> names;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(location(directory)))
        if (entry.is_regular_file())
            names.push_back(entry.path().filename().string());
    return names;
}

std::vector<std::string> EmbeddedResources::files(const std::string& directory) {
    const std::filesystem::path root = location(directory);
    std::vector<std::string> paths;
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(root))
        if (entry.is_regular_file())
            paths.push_back(entry.path().lexically_relative(root).generic_string());
    return paths;
}

}
