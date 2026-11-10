#include "RmlKit/UI/Backend/GlPass.h"

#include <format>
#include <string>

#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

namespace {

constexpr const char* vertexBody = R"(
void main() {
    vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
)";

GLuint compile(const GLenum type, const char* body) {
    const GLuint shader = glCreateShader(type);
    const char* sources[] = {glslVersion, body};
    glShaderSource(shader, 2, sources, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled)
        return shader;
    std::string log(1024, '\0');
    GLsizei length = 0;
    glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), &length, log.data());
    glDeleteShader(shader);
    log.resize(static_cast<size_t>(length));
    fatal(std::format("A texture conversion shader did not compile: {}", log));
}

}

void setSampling(const GLint minFilter, const GLint magFilter) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

GLuint newTexture() {
    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0)
        fatal("glGenTextures gave no texture");
    return texture;
}

GLuint linkFullscreenProgram(const char* fragmentBody) {
    const GLuint vertex = compile(GL_VERTEX_SHADER, vertexBody);
    const GLuint fragment = compile(GL_FRAGMENT_SHADER, fragmentBody);
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint done = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &done);
    if (done)
        return program;
    std::string log(1024, '\0');
    GLsizei length = 0;
    glGetProgramInfoLog(program, static_cast<GLsizei>(log.size()), &length, log.data());
    glDeleteProgram(program);
    log.resize(static_cast<size_t>(length));
    fatal(std::format("A texture conversion shader did not link: {}", log));
}

GlPass::GlPass() {
    glGenFramebuffers(1, &framebuffer);
    glGenVertexArrays(1, &emptyVertexArray);
}

GlPass::~GlPass() {
    glDeleteVertexArrays(1, &emptyVertexArray);
    glDeleteFramebuffers(1, &framebuffer);
}

void GlPass::draw(const GLuint target, const int width, const int height) {
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        fatal("The GPU cannot draw into an RGBA8 texture.");
    }
    glViewport(0, 0, width, height);
    glBindVertexArray(emptyVertexArray);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
}

}
