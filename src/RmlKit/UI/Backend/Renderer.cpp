#include "RmlKit/UI/Backend/Renderer.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <string_view>
#include <vector>

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Log.h>
#include <plutovg.h>

#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

Renderer::Renderer() {
    if (!*this)
        fatal("The OpenGL 3 renderer failed to initialize");
    constexpr Rml::byte transparent[4] = {};
    placeholder = RenderInterface_GL3::GenerateTexture({transparent, sizeof(transparent)}, {1, 1});
    if (!placeholder)
        fatal("The OpenGL 3 renderer failed to create the placeholder texture");
}

Renderer::~Renderer() {
    for (const auto& [key, texture] : uploadedTextures)
        RenderInterface_GL3::ReleaseTexture(texture.handle);
    RenderInterface_GL3::ReleaseTexture(placeholder);
}

void Renderer::setExternalTextureReader(const std::string_view prefix, ExternalTextureReader* const reader) {
    this->prefix = prefix;
    this->reader = reader;
}

void Renderer::addExternalTexture(const Rml::String& key, const Rml::TextureHandle handle, const Rml::Vector2i dimensions) {
    if (dimensions.x <= 0 || dimensions.y <= 0)
        fatal("an external texture has a positive width and height");
    const Rml::String source = sourceOf(key);
    if (!readingSources.erase(source))
        fatal(std::format("The external texture '{}' was added without a read in flight.", key));
    const auto [found, added] = uploadedTextures.try_emplace(key, UploadedTexture{handle, dimensions});
    if (!added)
        fatal(std::format("The external texture '{}' was added twice before RmlUi loaded it.", key));
    if (Rml::ReleaseTexture(source, this)) {
        arrivedSources.insert(source);
        return;
    }
    uploadedTextures.erase(found);
    RenderInterface_GL3::ReleaseTexture(handle);
}

void Renderer::failExternalTexture(const Rml::String& key) {
    if (!readingSources.erase(sourceOf(key)))
        fatal(std::format("The external texture '{}' failed without a read in flight.", key));
}

void Renderer::reloadExternalTextures() {
    readingSources.clear();
    for (const auto& [key, texture] : uploadedTextures)
        RenderInterface_GL3::ReleaseTexture(texture.handle);
    uploadedTextures.clear();
    for (const Rml::String& key : loadedKeys)
        Rml::ReleaseTexture(sourceOf(key), this);
}

Rml::TextureHandle Renderer::LoadTexture(Rml::Vector2i& textureDimensions, const Rml::String& source) {
    if (!reader || !source.starts_with(prefix))
        return loadImageFile(textureDimensions, source);

    const Rml::String key = source.substr(prefix.size());
    loadedKeys.insert(key);
    const auto uploaded = uploadedTextures.find(key);
    if (uploaded == uploadedTextures.end()) {
        if (readingSources.insert(source).second)
            reader->read(key);
        textureDimensions = {1, 1};
        return placeholder;
    }
    const UploadedTexture texture = uploaded->second;
    uploadedTextures.erase(uploaded);
    textureDimensions = texture.dimensions;
    return texture.handle;
}

Rml::TextureHandle Renderer::loadImageFile(Rml::Vector2i& textureDimensions, const Rml::String& source) {
    Rml::String file;
    if (!Rml::GetFileInterface()->LoadFile(source, file)) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "The image '%s' could not be read.", source.c_str());
        return {};
    }
    const std::unique_ptr<plutovg_surface_t, decltype(&plutovg_surface_destroy)> surface(
        plutovg_surface_load_from_image_data(file.data(), static_cast<int>(file.size())), plutovg_surface_destroy);
    if (!surface) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "The image '%s' is not a PNG or JPEG file.", source.c_str());
        return {};
    }
    const int width = plutovg_surface_get_width(surface.get());
    const int height = plutovg_surface_get_height(surface.get());
    const int stride = plutovg_surface_get_stride(surface.get());
    const unsigned char* const data = plutovg_surface_get_data(surface.get());
    std::vector<Rml::byte> pixels(static_cast<std::size_t>(width) * height * 4);
    Rml::byte* target = pixels.data();
    for (int y = 0; y < height; ++y) {
        const auto* const row = reinterpret_cast<const std::uint32_t*>(data + static_cast<std::ptrdiff_t>(y) * stride);
        for (int x = 0; x < width; ++x) {
            const std::uint32_t argb = row[x];
            *target++ = static_cast<Rml::byte>(argb >> 16);
            *target++ = static_cast<Rml::byte>(argb >> 8);
            *target++ = static_cast<Rml::byte>(argb);
            *target++ = static_cast<Rml::byte>(argb >> 24);
        }
    }
    textureDimensions = {width, height};
    return GenerateTexture(pixels, textureDimensions);
}

void Renderer::ReleaseTexture(const Rml::TextureHandle textureHandle) {
    if (textureHandle == placeholder)
        return;
    RenderInterface_GL3::ReleaseTexture(textureHandle);
}

}
