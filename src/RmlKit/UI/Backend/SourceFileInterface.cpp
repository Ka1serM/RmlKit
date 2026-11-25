#include "RmlKit/UI/Backend/SourceFileInterface.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <optional>

#include "RmlKit/UI/Backend/EmbeddedResources.h"
#include "RmlKit/UI/Backend/WindowButtonLayout.h"

namespace RmlKit::UI {

namespace {

Rml::String sourcePath(const Rml::String& path) {
    if (path.starts_with("UI/RmlKit/"))
        return Rml::String(KIT_UI_DIR) + "/" + path;
    const char* root = path.starts_with("UI/") ? UI_SOURCE_DIR : ASSET_SOURCE_DIR;
    return Rml::String(root) + "/" + path;
}

std::FILE* file(const Rml::FileHandle handle) {
    return reinterpret_cast<std::FILE*>(handle);
}

}

Rml::FileHandle SourceFileInterface::Open(const Rml::String& path) {
    if (const std::optional<std::string_view> data = readPlatformFile(path)) {
        auto memory = std::make_unique<MemoryFile>(MemoryFile{.data = *data});
        const auto handle = reinterpret_cast<Rml::FileHandle>(memory.get());
        memoryFiles.emplace(handle, std::move(memory));
        return handle;
    }
    if (std::FILE* opened = std::fopen(sourcePath(path).c_str(), "rb"))
        return reinterpret_cast<Rml::FileHandle>(opened);
    //! A file outside the source folders, such as the UI of a plugin library, comes from the embedded resources.
    if (!EmbeddedResources::exists(path))
        return 0;
    auto memory = std::make_unique<MemoryFile>(MemoryFile{.data = EmbeddedResources::read(path)});
    const auto handle = reinterpret_cast<Rml::FileHandle>(memory.get());
    memoryFiles.emplace(handle, std::move(memory));
    return handle;
}

void SourceFileInterface::Close(const Rml::FileHandle handle) {
    if (!memoryFiles.erase(handle))
        std::fclose(file(handle));
}

size_t SourceFileInterface::Read(void* buffer, const size_t size, const Rml::FileHandle handle) {
    const auto memory = memoryFiles.find(handle);
    if (memory == memoryFiles.end())
        return std::fread(buffer, 1, size, file(handle));
    MemoryFile& open = *memory->second;
    const size_t count = std::min(size, open.data.size() - open.position);
    std::memcpy(buffer, open.data.data() + open.position, count);
    open.position += count;
    return count;
}

bool SourceFileInterface::Seek(const Rml::FileHandle handle, const long offset, const int origin) {
    const auto memory = memoryFiles.find(handle);
    if (memory == memoryFiles.end())
        return std::fseek(file(handle), offset, origin) == 0;
    MemoryFile& open = *memory->second;
    long base = 0;
    if (origin == SEEK_CUR)
        base = static_cast<long>(open.position);
    else if (origin == SEEK_END)
        base = static_cast<long>(open.data.size());
    const long position = base + offset;
    if (position < 0 || position > static_cast<long>(open.data.size()))
        return false;
    open.position = static_cast<size_t>(position);
    return true;
}

size_t SourceFileInterface::Tell(const Rml::FileHandle handle) {
    const auto memory = memoryFiles.find(handle);
    if (memory == memoryFiles.end())
        return static_cast<size_t>(std::ftell(file(handle)));
    return memory->second->position;
}

}
