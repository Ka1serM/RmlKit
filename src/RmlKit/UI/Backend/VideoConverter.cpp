#include "RmlKit/UI/Backend/VideoConverter.h"

#include <cstring>
#include <format>

#include "RmlKit/UI/Backend/Fatal.h"
#include "RmlKit/UI/Backend/VideoShaders.h"
#include "VideoShaders.slang.h"

namespace RmlKit::UI {

namespace {

VideoShaders::Arguments describe(const Media::VideoImage& frame) {
    const bool bt601 = frame.matrix == Media::YuvMatrix::Bt601;
    const float red = bt601 ? 0.299f : 0.2126f;
    const float blue = bt601 ? 0.114f : 0.0722f;
    const float green = 1.0f - red - blue;
    const float lumaScale = frame.fullRange ? 1.0f : 255.0f / 219.0f;
    const float chromaScale = frame.fullRange ? 1.0f : 255.0f / 224.0f;
    const NoorRHI::float3 offset = {frame.fullRange ? 0.0f : 16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f};
    const auto row = [&offset](const float y, const float u, const float v) { return NoorRHI::float4{y, u, v, -(y * offset.x + u * offset.y + v * offset.z)}; };
    return {.colorMatrix = {row(lumaScale, 0.0f, 2.0f * (1.0f - red) * chromaScale),
                            row(lumaScale, -2.0f * blue * (1.0f - blue) / green * chromaScale, -2.0f * red * (1.0f - red) / green * chromaScale),
                            row(lumaScale, 2.0f * (1.0f - blue) * chromaScale, 0.0f)},
            .size = {static_cast<float>(frame.width), static_cast<float>(frame.height)}};
}

}

VideoConverter::VideoConverter(Renderer& renderer) : renderer(renderer) {
    NoorRHI::Device& device = renderer.getDevice();
    const NoorRHI::GraphicsPipelineDesc desc{.vertex = VideoShaders::fullscreenMain,
                                             .fragment = VideoShaders::convertMain,
                                             .state = {.cull = NoorRHI::CullMode::None},
                                             .colorFormat = NoorRHI::ImageFormat::Rgba8Unorm};
    if (auto opened = pipeline.open(device, desc); !opened)
        fatal(std::format("The video converter cannot open its pipeline: {}", opened.error().message));
    const NoorRHI::SamplerDesc clamp{.filter = NoorRHI::Filter::Linear,
                                     .addressU = NoorRHI::AddressMode::ClampToEdge,
                                     .addressV = NoorRHI::AddressMode::ClampToEdge,
                                     .addressW = NoorRHI::AddressMode::ClampToEdge};
    if (auto opened = sampler.open(device, clamp); !opened)
        fatal(std::format("The video converter cannot open its sampler: {}", opened.error().message));
}

void VideoConverter::upload(const Media::VideoImage& frame, const std::size_t plane, NoorRHI::Image& image) {
    const int width = plane == 0 ? frame.width : (frame.width + 1) / 2;
    const int height = plane == 0 ? frame.height : (frame.height + 1) / 2;
    if (!image || image.getWidth() != static_cast<std::uint32_t>(width) || image.getHeight() != static_cast<std::uint32_t>(height)) {
        const NoorRHI::ImageDesc desc{
            .width = static_cast<std::uint32_t>(width), .height = static_cast<std::uint32_t>(height), .format = NoorRHI::ImageFormat::R8Unorm};
        if (auto opened = image.open(renderer.getDevice(), desc); !opened)
            fatal(std::format("The video converter cannot open a plane: {}", opened.error().message));
    }
    const auto* const source = reinterpret_cast<const std::byte*>(frame.planes[plane]);
    const int stride = frame.strides[plane];
    if (stride == width) {
        if (auto uploaded = image.upload({source, static_cast<std::size_t>(width) * height}); !uploaded)
            fatal(std::format("The video converter cannot upload a plane: {}", uploaded.error().message));
        return;
    }
    packed.resize(static_cast<std::size_t>(width) * height);
    for (int y = 0; y < height; ++y)
        std::memcpy(packed.data() + static_cast<std::size_t>(y) * width, source + static_cast<std::ptrdiff_t>(y) * stride, static_cast<std::size_t>(width));
    if (auto uploaded = image.upload(packed); !uploaded)
        fatal(std::format("The video converter cannot upload a plane: {}", uploaded.error().message));
}

void VideoConverter::convert(const Media::VideoImage& frame, Planes& planes, const NoorRHI::Image& target) {
    for (std::size_t plane = 0; plane < planes.size(); ++plane)
        upload(frame, plane, planes[plane]);
    VideoShaders::Arguments arguments = describe(frame);
    arguments.luma = planes[0].getSampledHandle();
    arguments.blueDifference = planes[1].getSampledHandle();
    arguments.redDifference = planes[2].getSampledHandle();
    arguments.sampler = sampler.getHandle();
    NoorRHI::CommandList& commands = renderer.record();
    commands.beginRender({.color = &target, .clear = false});
    commands.draw(pipeline, 3, 1, arguments);
    commands.endRender();
}

}
