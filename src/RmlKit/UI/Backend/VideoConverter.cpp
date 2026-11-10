#include "RmlKit/UI/Backend/VideoConverter.h"

namespace RmlKit::UI {

namespace {

constexpr const char* yuvBody = R"(
uniform sampler2D luma;
uniform sampler2D blueDifference;
uniform sampler2D redDifference;
uniform vec2 size;
uniform mat3 yuvMatrix;
uniform vec3 yuvOffset;
out vec4 color;
void main() {
    vec2 place = gl_FragCoord.xy / size;
    vec3 yuv = vec3(texelFetch(luma, ivec2(gl_FragCoord.xy), 0).r, texture(blueDifference, place).r,
                    texture(redDifference, place).r);
    color = vec4(clamp(yuvMatrix * (yuv - yuvOffset), 0.0, 1.0), 1.0);
}
)";

struct YuvColors {
    float matrix[9];
    float offset[3];
};

YuvColors yuvColors(const Media::VideoImage& frame) {
    const bool bt601 = frame.matrix == Media::YuvMatrix::Bt601;
    const float red = bt601 ? 0.299f : 0.2126f;
    const float blue = bt601 ? 0.114f : 0.0722f;
    const float green = 1.0f - red - blue;
    const float lumaScale = frame.fullRange ? 1.0f : 255.0f / 219.0f;
    const float chromaScale = frame.fullRange ? 1.0f : 255.0f / 224.0f;
    return {
        .matrix = {lumaScale, lumaScale, lumaScale, 0.0f, -2.0f * blue * (1.0f - blue) / green * chromaScale, 2.0f * (1.0f - blue) * chromaScale,
                   2.0f * (1.0f - red) * chromaScale, -2.0f * red * (1.0f - red) / green * chromaScale, 0.0f},
        .offset = {frame.fullRange ? 0.0f : 16.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f},
    };
}

}

VideoConverter::VideoConverter() : program(linkFullscreenProgram(yuvBody)) {
    size = glGetUniformLocation(program, "size");
    yuvMatrix = glGetUniformLocation(program, "yuvMatrix");
    yuvOffset = glGetUniformLocation(program, "yuvOffset");
    GLint current = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &current);
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "luma"), 0);
    glUniform1i(glGetUniformLocation(program, "blueDifference"), 1);
    glUniform1i(glGetUniformLocation(program, "redDifference"), 2);
    glUseProgram(static_cast<GLuint>(current));
}

VideoConverter::~VideoConverter() {
    glDeleteProgram(program);
}

void VideoConverter::convert(const Media::VideoImage& frame, Planes& planes, const GLuint target) {
    const SavedGlState saved;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (size_t plane = 0; plane < planes.textures.size(); ++plane) {
        const int width = plane == 0 ? frame.width : (frame.width + 1) / 2;
        const int height = plane == 0 ? frame.height : (frame.height + 1) / 2;
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(plane));
        if (planes.textures[plane] == 0 || planes.widths[plane] != width || planes.heights[plane] != height) {
            if (planes.textures[plane] == 0)
                planes.textures[plane] = newTexture();
            glBindTexture(GL_TEXTURE_2D, planes.textures[plane]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
            setSampling(plane == 0 ? GL_NEAREST : GL_LINEAR, plane == 0 ? GL_NEAREST : GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            planes.widths[plane] = width;
            planes.heights[plane] = height;
        }
        glBindTexture(GL_TEXTURE_2D, planes.textures[plane]);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame.strides[plane]);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, frame.planes[plane]);
    }
    glActiveTexture(GL_TEXTURE0);

    const YuvColors colors = yuvColors(frame);
    glUseProgram(program);
    glUniform2f(size, static_cast<float>(frame.width), static_cast<float>(frame.height));
    glUniformMatrix3fv(yuvMatrix, 1, GL_FALSE, colors.matrix);
    glUniform3fv(yuvOffset, 1, colors.offset);
    pass.draw(target, frame.width, frame.height);
}

void VideoConverter::deletePlanes(Planes& planes) {
    for (GLuint& texture : planes.textures) {
        if (texture)
            glDeleteTextures(1, &texture);
        texture = 0;
    }
}

}
