#include "RmlKit/UI/Backend/VideoTexture.h"

#include <format>
#include <vector>

#include "RmlKit/UI/Backend/Fatal.h"
#include "RmlKit/UI/Backend/GL.h"

namespace RmlKit::UI {

VideoTexture::VideoTexture(VideoConverter& converter, const int width, const int height) : converter(converter), width(width), height(height) {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0)
        fatal("glGenTextures gave no texture");
    GLint bound = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    glBindTexture(GL_TEXTURE_2D, texture);
    const std::vector<std::byte> black(static_cast<size_t>(width) * height * 4, std::byte{0});
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, black.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(bound));
    //! RmlUi's GL3 renderer uses the GL texture name as the handle and deletes it in ReleaseTexture.
    handle = static_cast<Rml::TextureHandle>(texture);
}

VideoTexture::~VideoTexture() {
    VideoConverter::deletePlanes(planes);
}

void VideoTexture::convert(const Media::VideoImage& frame) {
    if (frame.width != width || frame.height != height)
        fatal(std::format("A video frame is {} x {}, not {} x {}.", frame.width, frame.height, width, height));
    converter.convert(frame, planes, static_cast<GLuint>(handle));
}

}
