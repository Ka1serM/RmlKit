#pragma once

#include <array>

#include "RmlKit/Media/VideoPlayer.h"
#include "RmlKit/UI/Backend/GlPass.h"

namespace RmlKit::UI {

class VideoConverter {
public:
    VideoConverter();
    ~VideoConverter();
    VideoConverter(const VideoConverter&) = delete;
    VideoConverter& operator=(const VideoConverter&) = delete;

    struct Planes {
        std::array<GLuint, 3> textures = {};
        std::array<int, 3> widths = {};
        std::array<int, 3> heights = {};
    };
    void convert(const Media::VideoImage& frame, Planes& planes, GLuint target);
    static void deletePlanes(Planes& planes);

private:
    GlPass pass;
    GLuint program = 0;
    GLint size = -1;
    GLint yuvMatrix = -1;
    GLint yuvOffset = -1;
};

}
