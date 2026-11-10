#pragma once

#include <RmlUi/Core/Types.h>

#include "RmlKit/Media/VideoPlayer.h"
#include "RmlKit/UI/Backend/VideoConverter.h"

namespace RmlKit::UI {

class VideoTexture {
public:
    VideoTexture(VideoConverter& converter, int width, int height);
    ~VideoTexture();
    VideoTexture(const VideoTexture&) = delete;
    VideoTexture& operator=(const VideoTexture&) = delete;

    Rml::TextureHandle getHandle() const { return handle; }
    void convert(const Media::VideoImage& frame);

private:
    VideoConverter& converter;
    int width;
    int height;
    VideoConverter::Planes planes;
    Rml::TextureHandle handle = {};
};

}
