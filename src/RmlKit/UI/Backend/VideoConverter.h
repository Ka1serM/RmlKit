#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include <NoorRHI/NoorRHI.h>

#include "RmlKit/Media/VideoPlayer.h"
#include "RmlKit/UI/Backend/Renderer.h"

namespace RmlKit::UI {

class VideoConverter {
public:
    explicit VideoConverter(Renderer& renderer);
    VideoConverter(const VideoConverter&) = delete;
    VideoConverter& operator=(const VideoConverter&) = delete;

    using Planes = std::array<NoorRHI::Image, 3>;
    Renderer& getRenderer() const { return renderer; }
    void convert(const Media::VideoImage& frame, Planes& planes, const NoorRHI::Image& target);

private:
    void upload(const Media::VideoImage& frame, std::size_t plane, NoorRHI::Image& image);

    Renderer& renderer;
    NoorRHI::GraphicsPipeline pipeline;
    NoorRHI::Sampler sampler;
    std::vector<std::byte> packed;
};

}
