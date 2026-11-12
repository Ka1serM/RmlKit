#include "RmlKit/UI/Backend/VideoTexture.h"

#include <cstddef>
#include <format>
#include <vector>

#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

VideoTexture::VideoTexture(VideoConverter& converter, const int width, const int height) : converter(converter) {
    const NoorRHI::ImageDesc desc{.width = static_cast<std::uint32_t>(width),
                                  .height = static_cast<std::uint32_t>(height),
                                  .format = NoorRHI::ImageFormat::Rgba8Unorm,
                                  .usage = NoorRHI::ImageUsage::Sampled | NoorRHI::ImageUsage::ColorAttachment};
    if (auto opened = image.open(converter.getRenderer().getDevice(), desc); !opened)
        fatal(std::format("The video texture cannot open: {}", opened.error().message));
    const std::vector<std::byte> black(image.getByteSize(), std::byte{0});
    if (auto uploaded = image.upload(black); !uploaded)
        fatal(std::format("The video texture cannot clear: {}", uploaded.error().message));
    //! RmlUi releases the handle of a callback texture through the render interface.
    handle = converter.getRenderer().addTexture(image, TextureMode::Premultiplied);
}

void VideoTexture::convert(const Media::VideoImage& frame) {
    if (frame.width != static_cast<int>(image.getWidth()) || frame.height != static_cast<int>(image.getHeight()))
        fatal(std::format("A video frame is {} x {}, not {} x {}.", frame.width, frame.height, image.getWidth(), image.getHeight()));
    converter.convert(frame, planes, image);
}

}
