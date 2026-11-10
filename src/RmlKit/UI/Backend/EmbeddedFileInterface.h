#pragma once

#include <string_view>
#include <unordered_map>

#include <RmlUi/Core/FileInterface.h>

namespace RmlKit::UI {

class EmbeddedFileInterface final : public Rml::FileInterface {
public:
    Rml::FileHandle Open(const Rml::String& path) override;
    void Close(Rml::FileHandle handle) override;
    size_t Read(void* buffer, size_t size, Rml::FileHandle handle) override;
    bool Seek(Rml::FileHandle handle, long offset, int origin) override;
    size_t Tell(Rml::FileHandle handle) override;
    size_t Length(Rml::FileHandle handle) override;
    bool LoadFile(const Rml::String& path, Rml::String& outData) override;

private:
    struct OpenFile {
        std::string_view data;
        size_t position = 0;
    };

    std::unordered_map<Rml::FileHandle, OpenFile> openFiles;
    Rml::FileHandle nextHandle = 1;
};

}
