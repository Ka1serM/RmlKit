#include "RmlKit/UI/Backend/EmbeddedFileInterface.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "RmlKit/UI/Backend/EmbeddedResources.h"

namespace RmlKit::UI {

Rml::FileHandle EmbeddedFileInterface::Open(const Rml::String& path) {
    if (!EmbeddedResources::exists(path))
        return 0;
    const Rml::FileHandle handle = nextHandle++;
    openFiles.emplace(handle, OpenFile{.data = EmbeddedResources::read(path)});
    return handle;
}

void EmbeddedFileInterface::Close(const Rml::FileHandle handle) {
    openFiles.erase(handle);
}

size_t EmbeddedFileInterface::Read(void* buffer, const size_t size, const Rml::FileHandle handle) {
    OpenFile& open = openFiles.at(handle);
    const size_t count = std::min(size, open.data.size() - open.position);
    std::memcpy(buffer, open.data.data() + open.position, count);
    open.position += count;
    return count;
}

bool EmbeddedFileInterface::Seek(const Rml::FileHandle handle, const long offset, const int origin) {
    OpenFile& open = openFiles.at(handle);
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

size_t EmbeddedFileInterface::Tell(const Rml::FileHandle handle) {
    return openFiles.at(handle).position;
}

size_t EmbeddedFileInterface::Length(const Rml::FileHandle handle) {
    return openFiles.at(handle).data.size();
}

bool EmbeddedFileInterface::LoadFile(const Rml::String& path, Rml::String& outData) {
    if (!EmbeddedResources::exists(path))
        return false;
    outData.assign(EmbeddedResources::read(path));
    return true;
}

}
