#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <NoorRHI/NoorRHI.h>
#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/Types.h>
#include <RmlUi/Core/Vertex.h>

#include "RmlKit/UI/Backend/RenderShaders.h"

namespace RmlKit::UI {

class ExternalTextureReader {
public:
    virtual void read(const Rml::String& key) = 0;

protected:
    ~ExternalTextureReader() = default;
};

enum class TextureMode : std::uint32_t {
    Premultiplied = RenderShaders::premultipliedTexture,
    Straight = RenderShaders::straightTexture,
    MaskRed = RenderShaders::maskTexture,
    MaskGreen,
    MaskBlue,
    MaskAlpha,
};

class Renderer final : public Rml::RenderInterface {
public:
    Renderer(NoorRHI::Device& device, NoorRHI::ImageFormat targetFormat);
    ~Renderer() override;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    NoorRHI::Device& getDevice() const { return device; }
    NoorRHI::CommandList& record();
    Rml::TextureHandle addTexture(NoorRHI::Image image, TextureMode mode);
    const NoorRHI::Image& getTexture(Rml::TextureHandle handle) const { return textures.get(handle).image; }

    void beginFrame(Rml::Vector2i size);
    NoorRHI::CommandList endFrame(const NoorRHI::Image& target);

    void setExternalTextureReader(std::string_view prefix, ExternalTextureReader* reader);
    void addExternalTexture(const Rml::String& key, Rml::TextureHandle handle, Rml::Vector2i dimensions);
    void failExternalTexture(const Rml::String& key);
    bool readsExternalTexture(const Rml::String& source) const { return readingSources.contains(source); }
    void reloadExternalTextures();
    std::unordered_set<Rml::String> takeArrivedSources() { return std::exchange(arrivedSources, {}); }

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
    void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

    Rml::TextureHandle LoadTexture(Rml::Vector2i& textureDimensions, const Rml::String& source) override;
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i sourceDimensions) override;
    void ReleaseTexture(Rml::TextureHandle texture) override;

    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;

    void EnableClipMask(bool enable) override;
    void RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation) override;

    void SetTransform(const Rml::Matrix4f* transform) override;

    Rml::LayerHandle PushLayer() override;
    void CompositeLayers(Rml::LayerHandle source, Rml::LayerHandle destination, Rml::BlendMode blendMode,
                         Rml::Span<const Rml::CompiledFilterHandle> filters) override;
    void PopLayer() override;

    Rml::TextureHandle SaveLayerAsTexture() override;
    Rml::CompiledFilterHandle SaveLayerAsMaskImage() override;

    Rml::CompiledFilterHandle CompileFilter(const Rml::String& name, const Rml::Dictionary& parameters) override;
    void ReleaseFilter(Rml::CompiledFilterHandle filter) override;

    Rml::CompiledShaderHandle CompileShader(const Rml::String& name, const Rml::Dictionary& parameters) override;
    void RenderShader(Rml::CompiledShaderHandle shader, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override;
    void ReleaseShader(Rml::CompiledShaderHandle shader) override;

private:
    template <class T>
    class Slots {
    public:
        std::uintptr_t add(T value) {
            if (free.empty()) {
                items.emplace_back(std::move(value));
                return items.size();
            }
            const std::uintptr_t handle = free.back();
            free.pop_back();
            items[handle - 1].emplace(std::move(value));
            return handle;
        }
        T& get(const std::uintptr_t handle) { return *items[handle - 1]; }
        const T& get(const std::uintptr_t handle) const { return *items[handle - 1]; }
        void remove(const std::uintptr_t handle) {
            items[handle - 1].reset();
            free.push_back(handle);
        }

    private:
        std::vector<std::optional<T>> items;
        std::vector<std::uintptr_t> free;
    };

    enum class Program : std::uint8_t {
        Color,
        ColorClipped,
        Texture,
        TextureClipped,
        Gradient,
        GradientClipped,
        MaskReplace,
        MaskIncrement,
        StencilFill,
        LayerClear,
        Composite,
        CompositeClipped,
        CompositeReplace,
        CompositeReplaceClipped,
        Copy,
        CopyBlended,
        Clear,
        ColorMatrix,
        BlendMask,
        Blur,
        DropShadow,
        Present,
        Count,
    };

    enum class FilterType : std::uint8_t { Opacity, Blur, DropShadow, ColorMatrix, MaskImage };

    struct Geometry {
        NoorRHI::Buffer<Rml::Vertex> vertices;
        NoorRHI::Buffer<int> indices;
        std::uint32_t indexCount = 0;
    };

    struct Filter {
        FilterType type = FilterType::Opacity;
        float value = 0.0f;
        Rml::Vector2f offset;
        NoorRHI::float4 color;
        std::array<NoorRHI::float4, 4> colorMatrix{};
    };

    struct Gradient {
        std::uint32_t function = 0;
        Rml::Vector2f start;
        Rml::Vector2f vector;
        NoorRHI::Buffer<NoorRHI::float4> colors;
        NoorRHI::Buffer<float> positions;
        std::uint32_t stopCount = 0;
    };

    struct Texture {
        NoorRHI::Image image;
        TextureMode mode = TextureMode::Premultiplied;
    };

    struct UploadedTexture {
        Rml::TextureHandle handle;
        Rml::Vector2i dimensions;
    };

    void openPipelines(NoorRHI::ImageFormat targetFormat);
    void openTargets(Rml::Vector2i size);
    NoorRHI::Image openImage(Rml::Vector2i size, NoorRHI::ImageFormat format, NoorRHI::ImageUsage usage, std::uint32_t samples) const;

    void draw(const NoorRHI::Image& target, Program program, const RenderShaders::PassArguments& arguments);
    void drawGeometry(Program program, Rml::CompiledGeometryHandle geometry, RenderShaders::GeometryArguments arguments);
    RenderShaders::GeometryArguments describeGeometry(Rml::Vector2f translation) const;
    void fillStencil(std::uint32_t value);
    void beginPass(const NoorRHI::Image& target);
    void endPass();
    void applyScissor();
    void setScissor(std::optional<Rml::Rectanglei> region);
    void setStencilReference(std::uint32_t reference);
    void resolve(Rml::LayerHandle layer);
    void swapPostprocess();
    Program clipped(Program unclipped) const;
    Rml::Rectanglei getScissorOrViewport() const;

    RenderShaders::PassArguments fullscreen(const NoorRHI::Image& texture) const;
    NoorRHI::float4 toClipRect(Rml::Rectanglei region) const;
    NoorRHI::float4 toUvRect(Rml::Rectanglei region) const;
    NoorRHI::float4 toLimits(Rml::Rectanglei region) const;

    void renderFilters(Rml::Span<const Rml::CompiledFilterHandle> filters);
    void blur(float sigma, const NoorRHI::Image& sourceDestination, const NoorRHI::Image& temporary, Rml::Rectanglei window);
    static std::array<NoorRHI::float4, 4> toRows(const Rml::Matrix4f& matrix);

    Rml::String sourceOf(const Rml::String& key) const { return prefix + key; }
    Rml::TextureHandle loadImageFile(Rml::Vector2i& textureDimensions, const Rml::String& source);

    NoorRHI::Device& device;
    NoorRHI::CommandList commands;
    std::array<NoorRHI::GraphicsPipeline, static_cast<std::size_t>(Program::Count)> pipelines;
    NoorRHI::Sampler repeatSampler;
    NoorRHI::Sampler clampSampler;

    Slots<Texture> textures;
    Slots<Geometry> geometries;
    Slots<Filter> filters;
    Slots<Gradient> gradients;

    Rml::Vector2i viewport;
    std::vector<NoorRHI::Image> layers;
    std::size_t layerCount = 0;
    NoorRHI::Image stencil;
    std::array<NoorRHI::Image, 3> postprocess;
    NoorRHI::Image blendMask;

    NoorRHI::Image passTarget;
    std::optional<Rml::Rectanglei> scissor;
    bool clipMask = false;
    std::uint32_t stencilTest = 1;
    std::uint32_t stencilReference = 0;
    Rml::Matrix4f projection;
    std::array<NoorRHI::float4, 4> transform{};

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
