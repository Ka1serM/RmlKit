#include "RmlKit/UI/Backend/Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <string_view>
#include <vector>

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/DecorationTypes.h>
#include <RmlUi/Core/Dictionary.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Log.h>
#include <RmlUi/Core/Math.h>
#include <plutovg.h>

#include "RenderShaders.slang.h"
#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

namespace {

constexpr NoorRHI::ImageFormat colorFormat = NoorRHI::ImageFormat::Rgba8Unorm;
constexpr NoorRHI::ImageFormat stencilFormat = NoorRHI::ImageFormat::D32FloatS8Uint;
constexpr std::uint32_t layerSamples = 4;
constexpr int blurWeightCount = static_cast<int>(RenderShaders::blurTaps / 2 + 1);

NoorRHI::float4 toFloat4(const Rml::ColourbPremultiplied color) {
    constexpr float scale = 1.0f / 255.0f;
    return {color.red * scale, color.green * scale, color.blue * scale, color.alpha * scale};
}

NoorRHI::Rect toRect(const Rml::Rectanglei region) {
    return {region.Left(), region.Top(), static_cast<std::uint32_t>(std::max(region.Width(), 0)), static_cast<std::uint32_t>(std::max(region.Height(), 0))};
}

void sigmaToParameters(const float desiredSigma, int& passLevel, float& sigma) {
    constexpr int maxPasses = 10;
    constexpr float maxSinglePassSigma = 3.0f;
    passLevel = std::clamp(Rml::Math::Log2(static_cast<int>(desiredSigma * (2.0f / maxSinglePassSigma))), 0, maxPasses);
    sigma = std::clamp(desiredSigma / static_cast<float>(1 << passLevel), 0.0f, maxSinglePassSigma);
}

NoorRHI::float4 blurWeights(const float sigma) {
    std::array<float, blurWeightCount> weights{};
    float normalization = 0.0f;
    for (int i = 0; i < blurWeightCount; ++i) {
        if (std::abs(sigma) < 0.1f)
            weights[i] = static_cast<float>(i == 0);
        else
            weights[i] = std::exp(-static_cast<float>(i * i) / (2.0f * sigma * sigma)) / (std::sqrt(2.0f * Rml::Math::RMLUI_PI) * sigma);
        normalization += (i == 0 ? 1.0f : 2.0f) * weights[i];
    }
    return {weights[0] / normalization, weights[1] / normalization, weights[2] / normalization, weights[3] / normalization};
}

}

Renderer::Renderer(NoorRHI::Device& device, const NoorRHI::ImageFormat targetFormat) : device(device), commands(device) {
    openPipelines(targetFormat);
    if (auto opened = repeatSampler.open(device, {.filter = NoorRHI::Filter::Linear}); !opened)
        fatal(std::format("The renderer cannot open a sampler: {}", opened.error().message));
    const NoorRHI::SamplerDesc clamp{.filter = NoorRHI::Filter::Linear,
                                     .addressU = NoorRHI::AddressMode::ClampToEdge,
                                     .addressV = NoorRHI::AddressMode::ClampToEdge,
                                     .addressW = NoorRHI::AddressMode::ClampToEdge};
    if (auto opened = clampSampler.open(device, clamp); !opened)
        fatal(std::format("The renderer cannot open a sampler: {}", opened.error().message));
    constexpr Rml::byte transparent[4] = {};
    placeholder = GenerateTexture({transparent, sizeof(transparent)}, {1, 1});
}

Renderer::~Renderer() = default;

void Renderer::openPipelines(const NoorRHI::ImageFormat targetFormat) {
    using NoorRHI::BlendMode;
    using NoorRHI::CompareOp;
    using NoorRHI::StencilOp;
    enum class Target : std::uint8_t { Layer, Postprocess, Present };
    const NoorRHI::StencilState none;
    const NoorRHI::StencilState test{.compare = CompareOp::Equal, .pass = StencilOp::Keep};
    const NoorRHI::StencilState replace{.compare = CompareOp::Always, .pass = StencilOp::Replace};
    const NoorRHI::StencilState increment{.compare = CompareOp::Always, .pass = StencilOp::Increment};
    const auto open = [&](const Program program, const NoorRHI::Shader& vertex, const NoorRHI::Shader& fragment, const Target target, const BlendMode blend,
                          const NoorRHI::StencilState stencil, const bool colorWrite = true) {
        NoorRHI::GraphicsPipelineDesc desc{
            .vertex = vertex,
            .fragment = fragment,
            .state = {.cull = NoorRHI::CullMode::None, .blend = blend, .colorWrite = colorWrite, .stencil = stencil},
            .colorFormat = target == Target::Present ? targetFormat : colorFormat,
        };
        if (target == Target::Layer) {
            desc.depthFormat = stencilFormat;
            desc.samples = layerSamples;
        }
        if (auto opened = pipelines[static_cast<std::size_t>(program)].open(device, desc); !opened)
            fatal(std::format("The renderer cannot open a pipeline: {}", opened.error().message));
    };
    const NoorRHI::Shader& geometry = RenderShaders::geometryMain;
    const NoorRHI::Shader& pass = RenderShaders::passMain;
    open(Program::Color, geometry, RenderShaders::colorMain, Target::Layer, BlendMode::PremultipliedAlpha, none);
    open(Program::ColorClipped, geometry, RenderShaders::colorMain, Target::Layer, BlendMode::PremultipliedAlpha, test);
    open(Program::Texture, geometry, RenderShaders::textureMain, Target::Layer, BlendMode::PremultipliedAlpha, none);
    open(Program::TextureClipped, geometry, RenderShaders::textureMain, Target::Layer, BlendMode::PremultipliedAlpha, test);
    open(Program::Gradient, geometry, RenderShaders::gradientMain, Target::Layer, BlendMode::PremultipliedAlpha, none);
    open(Program::GradientClipped, geometry, RenderShaders::gradientMain, Target::Layer, BlendMode::PremultipliedAlpha, test);
    open(Program::MaskReplace, geometry, RenderShaders::colorMain, Target::Layer, BlendMode::Opaque, replace, false);
    open(Program::MaskIncrement, geometry, RenderShaders::colorMain, Target::Layer, BlendMode::Opaque, increment, false);
    open(Program::StencilFill, pass, RenderShaders::clearMain, Target::Layer, BlendMode::Opaque, replace, false);
    open(Program::LayerClear, pass, RenderShaders::clearMain, Target::Layer, BlendMode::Opaque, none);
    open(Program::Composite, pass, RenderShaders::copyMain, Target::Layer, BlendMode::PremultipliedAlpha, none);
    open(Program::CompositeClipped, pass, RenderShaders::copyMain, Target::Layer, BlendMode::PremultipliedAlpha, test);
    open(Program::CompositeReplace, pass, RenderShaders::copyMain, Target::Layer, BlendMode::Opaque, none);
    open(Program::CompositeReplaceClipped, pass, RenderShaders::copyMain, Target::Layer, BlendMode::Opaque, test);
    open(Program::Copy, pass, RenderShaders::copyMain, Target::Postprocess, BlendMode::Opaque, none);
    open(Program::CopyBlended, pass, RenderShaders::copyMain, Target::Postprocess, BlendMode::PremultipliedAlpha, none);
    open(Program::Clear, pass, RenderShaders::clearMain, Target::Postprocess, BlendMode::Opaque, none);
    open(Program::ColorMatrix, pass, RenderShaders::colorMatrixMain, Target::Postprocess, BlendMode::Opaque, none);
    open(Program::BlendMask, pass, RenderShaders::blendMaskMain, Target::Postprocess, BlendMode::Opaque, none);
    open(Program::Blur, pass, RenderShaders::blurMain, Target::Postprocess, BlendMode::Opaque, none);
    open(Program::DropShadow, pass, RenderShaders::dropShadowMain, Target::Postprocess, BlendMode::Opaque, none);
    open(Program::Present, pass, RenderShaders::copyMain, Target::Present, BlendMode::PremultipliedAlpha, none);
}

NoorRHI::Image Renderer::openImage(const Rml::Vector2i size, const NoorRHI::ImageFormat format, const NoorRHI::ImageUsage usage,
                                   const std::uint32_t samples) const {
    NoorRHI::Image image;
    const NoorRHI::ImageDesc desc{
        .width = static_cast<std::uint32_t>(size.x), .height = static_cast<std::uint32_t>(size.y), .format = format, .usage = usage, .samples = samples};
    if (auto opened = image.open(device, desc); !opened)
        fatal(std::format("The renderer cannot open a {} x {} image: {}", size.x, size.y, opened.error().message));
    return image;
}

void Renderer::openTargets(const Rml::Vector2i size) {
    endPass();
    viewport = size;
    layers.clear();
    stencil = openImage(size, stencilFormat, NoorRHI::ImageUsage::DepthAttachment, layerSamples);
    constexpr NoorRHI::ImageUsage usage = NoorRHI::ImageUsage::ColorAttachment | NoorRHI::ImageUsage::Sampled;
    for (NoorRHI::Image& image : postprocess)
        image = openImage(size, colorFormat, usage, 1);
    blendMask = openImage(size, colorFormat, usage, 1);
}

NoorRHI::CommandList& Renderer::record() {
    endPass();
    return commands;
}

Rml::TextureHandle Renderer::addTexture(NoorRHI::Image image) {
    return textures.add(std::move(image));
}

void Renderer::beginFrame(const Rml::Vector2i size) {
    if (size != viewport)
        openTargets(size);
    projection = Rml::Matrix4f::ProjectOrtho(0.0f, static_cast<float>(size.x), static_cast<float>(size.y), 0.0f, -10000.0f, 10000.0f);
    SetTransform(nullptr);
    scissor.reset();
    clipMask = false;
    stencilTest = 1;
    layerCount = 0;
    PushLayer();
}

NoorRHI::CommandList Renderer::endFrame(const NoorRHI::Image& target) {
    resolve(0);
    layerCount = 0;
    commands.beginRender({.color = &target, .clear = true, .clearColor = {0.0f, 0.0f, 0.0f, 1.0f}});
    commands.draw(pipelines[static_cast<std::size_t>(Program::Present)], 6, 1, fullscreen(postprocess[0]));
    commands.endRender();
    return std::exchange(commands, NoorRHI::CommandList(device));
}

void Renderer::beginPass(const NoorRHI::Image& target) {
    if (passTarget.getBackend() == target.getBackend())
        return;
    endPass();
    const bool layer = target.getSamples() > 1;
    commands.beginRender({.color = &target, .depth = layer ? &stencil : nullptr, .clear = false});
    passTarget = target;
    applyScissor();
    if (layer)
        commands.setStencilReference(stencilReference);
}

void Renderer::endPass() {
    if (!passTarget)
        return;
    commands.endRender();
    passTarget = {};
}

void Renderer::applyScissor() {
    if (!passTarget)
        return;
    if (!scissor)
        return commands.setScissor(std::nullopt);
    commands.setScissor(toRect(*scissor));
}

void Renderer::setScissor(const std::optional<Rml::Rectanglei> region) {
    scissor = region && region->Valid() ? region : std::nullopt;
    applyScissor();
}

void Renderer::setStencilReference(const std::uint32_t reference) {
    stencilReference = reference;
    if (passTarget && passTarget.getSamples() > 1)
        commands.setStencilReference(reference);
}

void Renderer::swapPostprocess() {
    endPass();
    std::swap(postprocess[0], postprocess[1]);
}

void Renderer::resolve(const Rml::LayerHandle layer) {
    endPass();
    commands.beginRender({.color = &layers[layer], .resolve = &postprocess[0], .clear = false});
    commands.endRender();
}

Renderer::Program Renderer::clipped(const Program unclipped) const {
    if (!clipMask)
        return unclipped;
    return static_cast<Program>(static_cast<std::uint8_t>(unclipped) + 1);
}

Rml::Rectanglei Renderer::getScissorOrViewport() const {
    return scissor.value_or(Rml::Rectanglei::FromSize(viewport));
}

NoorRHI::float4 Renderer::toClipRect(const Rml::Rectanglei region) const {
    const NoorRHI::float4 uv = toUvRect(region);
    return {uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, uv.z * 2.0f - 1.0f, 1.0f - uv.w * 2.0f};
}

NoorRHI::float4 Renderer::toUvRect(const Rml::Rectanglei region) const {
    const Rml::Vector2f size(viewport);
    return {static_cast<float>(region.Left()) / size.x, static_cast<float>(region.Top()) / size.y, static_cast<float>(region.Right()) / size.x,
            static_cast<float>(region.Bottom()) / size.y};
}

NoorRHI::float4 Renderer::toLimits(const Rml::Rectanglei region) const {
    const Rml::Vector2f size(viewport);
    const Rml::Vector2f min = (Rml::Vector2f(region.p0) + Rml::Vector2f(0.5f)) / size;
    const Rml::Vector2f max = (Rml::Vector2f(region.p1) - Rml::Vector2f(0.5f)) / size;
    return {min.x, min.y, max.x, max.y};
}

RenderShaders::PassArguments Renderer::fullscreen(const NoorRHI::Image& texture) const {
    return {.target = {-1.0f, 1.0f, 1.0f, -1.0f},
            .source = {0.0f, 0.0f, 1.0f, 1.0f},
            .texture = texture.getSampledHandle(),
            .sampler = clampSampler.getHandle(),
            .opacity = 1.0f};
}

std::array<NoorRHI::float4, 4> Renderer::toRows(const Rml::Matrix4f& matrix) {
    const Rml::RowMajorMatrix4f rows(matrix);
    const float* const values = rows.data();
    std::array<NoorRHI::float4, 4> result;
    for (std::size_t row = 0; row < result.size(); ++row)
        result[row] = {values[row * 4], values[row * 4 + 1], values[row * 4 + 2], values[row * 4 + 3]};
    return result;
}

void Renderer::draw(const NoorRHI::Image& target, const Program program, const RenderShaders::PassArguments& arguments) {
    beginPass(target);
    commands.draw(pipelines[static_cast<std::size_t>(program)], 6, 1, arguments);
}

RenderShaders::GeometryArguments Renderer::describeGeometry(const Rml::Vector2f translation) const {
    return {.transform = {transform[0], transform[1], transform[2], transform[3]},
            .translation = {translation.x, translation.y},
            .sampler = repeatSampler.getHandle()};
}

void Renderer::drawGeometry(const Program program, const Rml::CompiledGeometryHandle handle, RenderShaders::GeometryArguments arguments) {
    const Geometry& geometry = geometries.get(handle);
    if (geometry.indexCount == 0)
        return;
    beginPass(layers[layerCount - 1]);
    arguments.vertices = geometry.vertices.getPtr().as<RenderShaders::Vertex>();
    commands.draw(pipelines[static_cast<std::size_t>(program)], geometry.indices.getPtr().as<std::uint32_t>(), geometry.indexCount, 1, arguments);
}

Rml::CompiledGeometryHandle Renderer::CompileGeometry(const Rml::Span<const Rml::Vertex> vertices, const Rml::Span<const int> indices) {
    Geometry geometry;
    if (vertices.empty() || indices.empty())
        return geometries.add(std::move(geometry));
    if (auto opened = geometry.vertices.open(device, vertices.size()); !opened)
        fatal(std::format("The renderer cannot open a vertex buffer: {}", opened.error().message));
    if (auto opened = geometry.indices.open(device, indices.size()); !opened)
        fatal(std::format("The renderer cannot open an index buffer: {}", opened.error().message));
    if (auto uploaded = geometry.vertices.upload({vertices.data(), vertices.size()}); !uploaded)
        fatal(std::format("The renderer cannot upload vertices: {}", uploaded.error().message));
    if (auto uploaded = geometry.indices.upload({indices.data(), indices.size()}); !uploaded)
        fatal(std::format("The renderer cannot upload indices: {}", uploaded.error().message));
    geometry.indexCount = static_cast<std::uint32_t>(indices.size());
    return geometries.add(std::move(geometry));
}

void Renderer::RenderGeometry(const Rml::CompiledGeometryHandle geometry, const Rml::Vector2f translation, const Rml::TextureHandle texture) {
    RenderShaders::GeometryArguments arguments = describeGeometry(translation);
    if (!texture)
        return drawGeometry(clipped(Program::Color), geometry, arguments);
    arguments.texture = textures.get(texture).getSampledHandle();
    drawGeometry(clipped(Program::Texture), geometry, arguments);
}

void Renderer::ReleaseGeometry(const Rml::CompiledGeometryHandle geometry) {
    geometries.remove(geometry);
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
    textures.remove(handle);
}

void Renderer::failExternalTexture(const Rml::String& key) {
    if (!readingSources.erase(sourceOf(key)))
        fatal(std::format("The external texture '{}' failed without a read in flight.", key));
}

void Renderer::reloadExternalTextures() {
    readingSources.clear();
    for (const auto& [key, texture] : uploadedTextures)
        textures.remove(texture.handle);
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

Rml::TextureHandle Renderer::GenerateTexture(const Rml::Span<const Rml::byte> source, const Rml::Vector2i sourceDimensions) {
    NoorRHI::Image image = openImage(sourceDimensions, colorFormat, NoorRHI::ImageUsage::Sampled, 1);
    if (auto uploaded = image.upload(std::as_bytes(std::span(source.data(), source.size()))); !uploaded)
        fatal(std::format("The renderer cannot upload a texture: {}", uploaded.error().message));
    return textures.add(std::move(image));
}

void Renderer::ReleaseTexture(const Rml::TextureHandle texture) {
    if (texture == placeholder)
        return;
    textures.remove(texture);
}

void Renderer::EnableScissorRegion(const bool enable) {
    if (!enable)
        setScissor(std::nullopt);
}

void Renderer::SetScissorRegion(const Rml::Rectanglei region) {
    setScissor(region);
}

void Renderer::EnableClipMask(const bool enable) {
    clipMask = enable;
}

void Renderer::fillStencil(const std::uint32_t value) {
    setStencilReference(value);
    draw(layers[layerCount - 1], Program::StencilFill, fullscreen(postprocess[0]));
}

void Renderer::RenderToClipMask(const Rml::ClipMaskOperation operation, const Rml::CompiledGeometryHandle geometry, const Rml::Vector2f translation) {
    switch (operation) {
    case Rml::ClipMaskOperation::Set:
        fillStencil(0);
        setStencilReference(1);
        drawGeometry(Program::MaskReplace, geometry, describeGeometry(translation));
        stencilTest = 1;
        break;
    case Rml::ClipMaskOperation::SetInverse:
        fillStencil(1);
        setStencilReference(0);
        drawGeometry(Program::MaskReplace, geometry, describeGeometry(translation));
        stencilTest = 1;
        break;
    case Rml::ClipMaskOperation::Intersect:
        drawGeometry(Program::MaskIncrement, geometry, describeGeometry(translation));
        stencilTest += 1;
        break;
    }
    setStencilReference(stencilTest);
}

void Renderer::SetTransform(const Rml::Matrix4f* const newTransform) {
    transform = toRows(newTransform ? projection * *newTransform : projection);
}

Rml::LayerHandle Renderer::PushLayer() {
    if (layerCount == layers.size())
        layers.push_back(openImage(viewport, colorFormat, NoorRHI::ImageUsage::ColorAttachment, layerSamples));
    ++layerCount;
    RenderShaders::PassArguments arguments = fullscreen(postprocess[0]);
    draw(layers[layerCount - 1], Program::LayerClear, arguments);
    return layerCount - 1;
}

void Renderer::CompositeLayers(const Rml::LayerHandle source, const Rml::LayerHandle destination, const Rml::BlendMode blendMode,
                               const Rml::Span<const Rml::CompiledFilterHandle> filterHandles) {
    resolve(source);
    renderFilters(filterHandles);
    const Program program = blendMode == Rml::BlendMode::Replace ? Program::CompositeReplace : Program::Composite;
    draw(layers[destination], clipped(program), fullscreen(postprocess[0]));
}

void Renderer::PopLayer() {
    --layerCount;
}

Rml::TextureHandle Renderer::SaveLayerAsTexture() {
    const Rml::Rectanglei bounds = getScissorOrViewport().IntersectIfValid(Rml::Rectanglei::FromSize(viewport));
    if (!bounds.Valid() || bounds.Width() == 0 || bounds.Height() == 0) {
        Rml::Log::Message(Rml::Log::LT_ERROR, "The layer has no area to save as a texture.");
        return {};
    }
    NoorRHI::Image image = openImage(bounds.Size(), colorFormat, NoorRHI::ImageUsage::Sampled, 1);
    resolve(layerCount - 1);
    commands.copy(postprocess[0], image, toRect(bounds));
    return textures.add(std::move(image));
}

Rml::CompiledFilterHandle Renderer::SaveLayerAsMaskImage() {
    resolve(layerCount - 1);
    draw(blendMask, Program::Copy, fullscreen(postprocess[0]));
    return filters.add({.type = FilterType::MaskImage});
}

Rml::CompiledFilterHandle Renderer::CompileFilter(const Rml::String& name, const Rml::Dictionary& parameters) {
    Filter filter;
    Rml::Matrix4f matrix = Rml::Matrix4f::Identity();
    if (name == "opacity") {
        filter.type = FilterType::Opacity;
        filter.value = Rml::Get(parameters, "value", 1.0f);
        return filters.add(filter);
    }
    if (name == "blur") {
        filter.type = FilterType::Blur;
        filter.value = Rml::Get(parameters, "sigma", 1.0f);
        return filters.add(filter);
    }
    if (name == "drop-shadow") {
        filter.type = FilterType::DropShadow;
        filter.value = Rml::Get(parameters, "sigma", 0.0f);
        filter.color = toFloat4(Rml::Get(parameters, "color", Rml::Colourb()).ToPremultiplied());
        filter.offset = Rml::Get(parameters, "offset", Rml::Vector2f(0.0f));
        return filters.add(filter);
    }
    if (name == "brightness") {
        const float value = Rml::Get(parameters, "value", 1.0f);
        matrix = Rml::Matrix4f::Diag(value, value, value, 1.0f);
    } else if (name == "contrast") {
        const float value = Rml::Get(parameters, "value", 1.0f);
        const float grayness = 0.5f - 0.5f * value;
        matrix = Rml::Matrix4f::Diag(value, value, value, 1.0f);
        matrix.SetColumn(3, Rml::Vector4f(grayness, grayness, grayness, 1.0f));
    } else if (name == "invert") {
        const float value = std::clamp(Rml::Get(parameters, "value", 1.0f), 0.0f, 1.0f);
        const float inverted = 1.0f - 2.0f * value;
        matrix = Rml::Matrix4f::Diag(inverted, inverted, inverted, 1.0f);
        matrix.SetColumn(3, Rml::Vector4f(value, value, value, 1.0f));
    } else if (name == "grayscale") {
        const float value = Rml::Get(parameters, "value", 1.0f);
        const float reverse = 1.0f - value;
        const Rml::Vector3f gray = value * Rml::Vector3f(0.2126f, 0.7152f, 0.0722f);
        matrix = Rml::Matrix4f::FromRows({gray.x + reverse, gray.y, gray.z, 0.0f}, {gray.x, gray.y + reverse, gray.z, 0.0f},
                                         {gray.x, gray.y, gray.z + reverse, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
    } else if (name == "sepia") {
        const float value = Rml::Get(parameters, "value", 1.0f);
        const float reverse = 1.0f - value;
        const Rml::Vector3f red = value * Rml::Vector3f(0.393f, 0.769f, 0.189f);
        const Rml::Vector3f green = value * Rml::Vector3f(0.349f, 0.686f, 0.168f);
        const Rml::Vector3f blue = value * Rml::Vector3f(0.272f, 0.534f, 0.131f);
        matrix = Rml::Matrix4f::FromRows({red.x + reverse, red.y, red.z, 0.0f}, {green.x, green.y + reverse, green.z, 0.0f},
                                         {blue.x, blue.y, blue.z + reverse, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
    } else if (name == "hue-rotate") {
        const float value = Rml::Get(parameters, "value", 1.0f);
        const float s = std::sin(value);
        const float c = std::cos(value);
        matrix = Rml::Matrix4f::FromRows({0.213f + 0.787f * c - 0.213f * s, 0.715f - 0.715f * c - 0.715f * s, 0.072f - 0.072f * c + 0.928f * s, 0.0f},
                                         {0.213f - 0.213f * c + 0.143f * s, 0.715f + 0.285f * c + 0.140f * s, 0.072f - 0.072f * c - 0.283f * s, 0.0f},
                                         {0.213f - 0.213f * c - 0.787f * s, 0.715f - 0.715f * c + 0.715f * s, 0.072f + 0.928f * c + 0.072f * s, 0.0f},
                                         {0.0f, 0.0f, 0.0f, 1.0f});
    } else if (name == "saturate") {
        const float value = Rml::Get(parameters, "value", 1.0f);
        matrix = Rml::Matrix4f::FromRows({0.213f + 0.787f * value, 0.715f - 0.715f * value, 0.072f - 0.072f * value, 0.0f},
                                         {0.213f - 0.213f * value, 0.715f + 0.285f * value, 0.072f - 0.072f * value, 0.0f},
                                         {0.213f - 0.213f * value, 0.715f - 0.715f * value, 0.072f + 0.928f * value, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
    } else {
        Rml::Log::Message(Rml::Log::LT_WARNING, "Unsupported filter type '%s'.", name.c_str());
        return {};
    }
    filter.type = FilterType::ColorMatrix;
    filter.colorMatrix = toRows(matrix);
    return filters.add(filter);
}

void Renderer::ReleaseFilter(const Rml::CompiledFilterHandle filter) {
    filters.remove(filter);
}

Rml::CompiledShaderHandle Renderer::CompileShader(const Rml::String& name, const Rml::Dictionary& parameters) {
    Gradient gradient;
    const bool repeating = Rml::Get(parameters, "repeating", false);
    if (name == "linear-gradient") {
        gradient.function = repeating ? RenderShaders::repeatingLinearGradient : RenderShaders::linearGradient;
        gradient.start = Rml::Get(parameters, "p0", Rml::Vector2f(0.0f));
        gradient.vector = Rml::Get(parameters, "p1", Rml::Vector2f(0.0f)) - gradient.start;
    } else if (name == "radial-gradient") {
        gradient.function = repeating ? RenderShaders::repeatingRadialGradient : RenderShaders::radialGradient;
        gradient.start = Rml::Get(parameters, "center", Rml::Vector2f(0.0f));
        gradient.vector = Rml::Vector2f(1.0f) / Rml::Get(parameters, "radius", Rml::Vector2f(1.0f));
    } else if (name == "conic-gradient") {
        gradient.function = repeating ? RenderShaders::repeatingConicGradient : RenderShaders::conicGradient;
        gradient.start = Rml::Get(parameters, "center", Rml::Vector2f(0.0f));
        const float angle = Rml::Get(parameters, "angle", 0.0f);
        gradient.vector = {std::cos(angle), std::sin(angle)};
    } else {
        Rml::Log::Message(Rml::Log::LT_WARNING, "Unsupported shader type '%s'.", name.c_str());
        return {};
    }

    const auto found = parameters.find("color_stop_list");
    if (found == parameters.end() || found->second.GetType() != Rml::Variant::COLORSTOPLIST) {
        Rml::Log::Message(Rml::Log::LT_WARNING, "The shader '%s' has no color stops.", name.c_str());
        return {};
    }
    const Rml::ColorStopList& stops = found->second.GetReference<Rml::ColorStopList>();
    if (stops.empty()) {
        Rml::Log::Message(Rml::Log::LT_WARNING, "The shader '%s' has no color stops.", name.c_str());
        return {};
    }
    std::vector<NoorRHI::float4> colors;
    std::vector<float> positions;
    colors.reserve(stops.size());
    positions.reserve(stops.size());
    for (const Rml::ColorStop& stop : stops) {
        colors.push_back(toFloat4(stop.color));
        positions.push_back(stop.position.number);
    }
    if (auto opened = gradient.colors.open(device, colors.size()); !opened)
        fatal(std::format("The renderer cannot open a gradient buffer: {}", opened.error().message));
    if (auto opened = gradient.positions.open(device, positions.size()); !opened)
        fatal(std::format("The renderer cannot open a gradient buffer: {}", opened.error().message));
    if (auto uploaded = gradient.colors.upload(colors); !uploaded)
        fatal(std::format("The renderer cannot upload a gradient: {}", uploaded.error().message));
    if (auto uploaded = gradient.positions.upload(positions); !uploaded)
        fatal(std::format("The renderer cannot upload a gradient: {}", uploaded.error().message));
    gradient.stopCount = static_cast<std::uint32_t>(stops.size());
    return gradients.add(std::move(gradient));
}

void Renderer::RenderShader(const Rml::CompiledShaderHandle shader, const Rml::CompiledGeometryHandle geometry, const Rml::Vector2f translation,
                            const Rml::TextureHandle) {
    const Gradient& gradient = gradients.get(shader);
    RenderShaders::GeometryArguments arguments = describeGeometry(translation);
    arguments.stopColors = gradient.colors.getPtr();
    arguments.stopPositions = gradient.positions.getPtr();
    arguments.gradientStart = {gradient.start.x, gradient.start.y};
    arguments.gradientVector = {gradient.vector.x, gradient.vector.y};
    arguments.gradientFunction = gradient.function;
    arguments.stopCount = gradient.stopCount;
    drawGeometry(clipped(Program::Gradient), geometry, arguments);
}

void Renderer::ReleaseShader(const Rml::CompiledShaderHandle shader) {
    gradients.remove(shader);
}

void Renderer::renderFilters(const Rml::Span<const Rml::CompiledFilterHandle> filterHandles) {
    for (const Rml::CompiledFilterHandle handle : filterHandles) {
        const Filter& filter = filters.get(handle);
        RenderShaders::PassArguments arguments = fullscreen(postprocess[0]);
        switch (filter.type) {
        case FilterType::Opacity:
            arguments.opacity = filter.value;
            draw(postprocess[1], Program::Copy, arguments);
            swapPostprocess();
            break;
        case FilterType::Blur:
            blur(filter.value, postprocess[0], postprocess[1], getScissorOrViewport());
            break;
        case FilterType::DropShadow: {
            const Rml::Rectanglei window = getScissorOrViewport();
            const Rml::Vector2f offset = filter.offset / Rml::Vector2f(viewport);
            arguments.source = {-offset.x, -offset.y, 1.0f - offset.x, 1.0f - offset.y};
            arguments.limits = toLimits(window);
            arguments.color = filter.color;
            draw(postprocess[1], Program::DropShadow, arguments);
            if (filter.value >= 0.5f)
                blur(filter.value, postprocess[1], postprocess[2], window);
            draw(postprocess[1], Program::CopyBlended, fullscreen(postprocess[0]));
            swapPostprocess();
            break;
        }
        case FilterType::ColorMatrix:
            arguments.colorMatrix[0] = filter.colorMatrix[0];
            arguments.colorMatrix[1] = filter.colorMatrix[1];
            arguments.colorMatrix[2] = filter.colorMatrix[2];
            arguments.colorMatrix[3] = filter.colorMatrix[3];
            draw(postprocess[1], Program::ColorMatrix, arguments);
            swapPostprocess();
            break;
        case FilterType::MaskImage:
            arguments.mask = blendMask.getSampledHandle();
            draw(postprocess[1], Program::BlendMask, arguments);
            swapPostprocess();
            break;
        }
    }
}

void Renderer::blur(const float desiredSigma, const NoorRHI::Image& sourceDestination, const NoorRHI::Image& temporary, const Rml::Rectanglei window) {
    int passLevel = 0;
    float sigma = 0.0f;
    sigmaToParameters(desiredSigma, passLevel, sigma);
    const std::optional<Rml::Rectanglei> originalScissor = scissor;

    Rml::Rectanglei region = window;
    setScissor(region);
    const Rml::Rectanglei half = Rml::Rectanglei::FromSize(viewport / 2);
    const Rml::Vector2f uvScaling = {viewport.x % 2 == 1 ? 1.0f - 1.0f / static_cast<float>(viewport.x) : 1.0f,
                                     viewport.y % 2 == 1 ? 1.0f - 1.0f / static_cast<float>(viewport.y) : 1.0f};
    for (int level = 0; level < passLevel; ++level) {
        region.p0 = (region.p0 + Rml::Vector2i(1)) / 2;
        region.p1 = Rml::Math::Max(region.p1 / 2, region.p0);
        const bool fromSource = level % 2 == 0;
        RenderShaders::PassArguments arguments = fullscreen(fromSource ? sourceDestination : temporary);
        arguments.target = toClipRect(half);
        arguments.source = {0.0f, 0.0f, uvScaling.x, uvScaling.y};
        setScissor(region);
        draw(fromSource ? temporary : sourceDestination, Program::Copy, arguments);
    }
    if (passLevel % 2 == 0)
        draw(temporary, Program::Copy, fullscreen(sourceDestination));

    RenderShaders::PassArguments arguments = fullscreen(temporary);
    arguments.weights = blurWeights(sigma);
    arguments.limits = toLimits(region);
    arguments.texelOffset = {0.0f, 1.0f / static_cast<float>(viewport.y)};
    draw(sourceDestination, Program::Blur, arguments);

    setScissor(region.Extend(1));
    draw(temporary, Program::Clear, arguments);
    setScissor(region);
    arguments.texture = sourceDestination.getSampledHandle();
    arguments.texelOffset = {1.0f / static_cast<float>(viewport.x), 0.0f};
    draw(temporary, Program::Blur, arguments);

    setScissor(window);
    RenderShaders::PassArguments upscale = fullscreen(temporary);
    upscale.source = toUvRect(region);
    upscale.target = toClipRect(window);
    draw(sourceDestination, Program::Copy, upscale);
    const Rml::Rectanglei aligned = Rml::Rectanglei::FromCorners(region.p0 * (1 << passLevel), region.p1 * (1 << passLevel));
    if (aligned != window) {
        upscale.target = toClipRect(aligned);
        draw(sourceDestination, Program::Copy, upscale);
    }
    setScissor(originalScissor);
}

}
