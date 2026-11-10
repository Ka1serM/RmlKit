#pragma once

#include <cstddef>
#include <iterator>

#include "RmlKit/UI/Backend/GL.h"

namespace RmlKit::UI {

//! RmlUi sets its draw state in each frame: the conversions run between frames and restore what they change.
class SavedGlState {
public:
    SavedGlState() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertexArray);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        for (GLint unit = 0; unit < units; ++unit) {
            glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &textures[unit]);
        }
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpackRowLength);
        for (size_t i = 0; i < std::size(capabilities); ++i)
            enabled[i] = glIsEnabled(capabilities[i]);
        for (const GLenum capability : capabilities)
            glDisable(capability);
        glActiveTexture(GL_TEXTURE0);
    }

    ~SavedGlState() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFramebuffer));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFramebuffer));
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glUseProgram(static_cast<GLuint>(program));
        glBindVertexArray(static_cast<GLuint>(vertexArray));
        for (GLint unit = 0; unit < units; ++unit) {
            glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(textures[unit]));
        }
        glActiveTexture(static_cast<GLenum>(activeTexture));
        glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, unpackRowLength);
        for (size_t i = 0; i < std::size(capabilities); ++i)
            if (enabled[i])
                glEnable(capabilities[i]);
    }

    SavedGlState(const SavedGlState&) = delete;
    SavedGlState& operator=(const SavedGlState&) = delete;

private:
    static constexpr GLint units = 3;
    static constexpr GLenum capabilities[] = {GL_BLEND, GL_SCISSOR_TEST, GL_STENCIL_TEST, GL_DEPTH_TEST, GL_CULL_FACE};

    GLint drawFramebuffer = 0;
    GLint readFramebuffer = 0;
    GLint viewport[4] = {};
    GLint program = 0;
    GLint vertexArray = 0;
    GLint activeTexture = 0;
    GLint textures[units] = {};
    GLint unpackAlignment = 4;
    GLint unpackRowLength = 0;
    GLboolean enabled[std::size(capabilities)] = {};
};

void setSampling(GLint minFilter, GLint magFilter);
GLuint newTexture();
GLuint linkFullscreenProgram(const char* fragmentBody);

class GlPass {
public:
    GlPass();
    ~GlPass();
    GlPass(const GlPass&) = delete;
    GlPass& operator=(const GlPass&) = delete;

    void draw(GLuint target, int width, int height);

private:
    GLuint framebuffer = 0;
    GLuint emptyVertexArray = 0;
};

}
