#pragma once

#include <RmlUi/Core/FileInterface.h>

namespace RmlKit::UI {

class SourceFileInterface final : public Rml::FileInterface {
public:
    Rml::FileHandle Open(const Rml::String& path) override;
    void Close(Rml::FileHandle handle) override;
    size_t Read(void* buffer, size_t size, Rml::FileHandle handle) override;
    bool Seek(Rml::FileHandle handle, long offset, int origin) override;
    size_t Tell(Rml::FileHandle handle) override;
};

}
