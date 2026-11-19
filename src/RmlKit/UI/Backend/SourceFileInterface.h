#pragma once

#include <memory>
#include <string_view>
#include <unordered_map>

#include <RmlUi/Core/FileInterface.h>

namespace RmlKit::UI {

class SourceFileInterface final : public Rml::FileInterface {
public:
    Rml::FileHandle Open(const Rml::String& path) override;
    void Close(Rml::FileHandle handle) override;
    size_t Read(void* buffer, size_t size, Rml::FileHandle handle) override;
    bool Seek(Rml::FileHandle handle, long offset, int origin) override;
    size_t Tell(Rml::FileHandle handle) override;

private:
    //! A platform file from memory; its handle is its address, so it cannot be the same as a FILE*.
    struct MemoryFile {
        std::string_view data;
        size_t position = 0;
    };

    std::unordered_map<Rml::FileHandle, std::unique_ptr<MemoryFile>> memoryFiles;
};

}
