#include "RmlKit/UI/Backend/SourceFileInterface.h"

#include <cstdio>

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
    return reinterpret_cast<Rml::FileHandle>(std::fopen(sourcePath(path).c_str(), "rb"));
}

void SourceFileInterface::Close(const Rml::FileHandle handle) {
    std::fclose(file(handle));
}

size_t SourceFileInterface::Read(void* buffer, const size_t size, const Rml::FileHandle handle) {
    return std::fread(buffer, 1, size, file(handle));
}

bool SourceFileInterface::Seek(const Rml::FileHandle handle, const long offset, const int origin) {
    return std::fseek(file(handle), offset, origin) == 0;
}

size_t SourceFileInterface::Tell(const Rml::FileHandle handle) {
    return static_cast<size_t>(std::ftell(file(handle)));
}

}
