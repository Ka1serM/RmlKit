#pragma once

#if PLATFORM_WEB
#include <GLES3/gl3.h>
inline constexpr const char* glslVersion = "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n";
#else
#include <RmlUi_Include_GL3.h>
inline constexpr const char* glslVersion = "#version 330\n";
#endif
