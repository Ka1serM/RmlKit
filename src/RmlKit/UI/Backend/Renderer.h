#pragma once

#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <RmlUi/Core/Types.h>
#include <RmlUi_Renderer_GL3.h>

namespace RmlKit::UI {

class ExternalTextureReader {
public:
    virtual void read(const Rml::String& key) = 0;

protected:
    ~ExternalTextureReader() = default;
};

class Renderer : public RenderInterface_GL3 {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void setExternalTextureReader(std::string_view prefix, ExternalTextureReader* reader);
    void addExternalTexture(const Rml::String& key, Rml::TextureHandle handle, Rml::Vector2i dimensions);
    void failExternalTexture(const Rml::String& key);
    bool readsExternalTexture(const Rml::String& source) const { return readingSources.contains(source); }
    void reloadExternalTextures();
    std::unordered_set<Rml::String> takeArrivedSources() { return std::exchange(arrivedSources, {}); }

    Rml::TextureHandle LoadTexture(Rml::Vector2i& textureDimensions, const Rml::String& source) override;
    void ReleaseTexture(Rml::TextureHandle textureHandle) override;

private:
    struct UploadedTexture {
        Rml::TextureHandle handle;
        Rml::Vector2i dimensions;
    };

    Rml::String sourceOf(const Rml::String& key) const { return prefix + key; }
    Rml::TextureHandle loadImageFile(Rml::Vector2i& textureDimensions, const Rml::String& source);

    Rml::String prefix;
    ExternalTextureReader* reader = nullptr;
    std::unordered_map<Rml::String, UploadedTexture> uploadedTextures;
    std::unordered_set<Rml::String> readingSources;
    std::unordered_set<Rml::String> loadedKeys;
    std::unordered_set<Rml::String> arrivedSources;
    //! RmlUi releases the texture once per source, so ReleaseTexture keeps it.
    Rml::TextureHandle placeholder = {};
};

}
