#pragma once

#include "NoorRHI/Shaders/ShaderTypes.h"

namespace RenderShaders {

using namespace NoorRHI;

static const uint linearGradient = 0;
static const uint radialGradient = 1;
static const uint conicGradient = 2;
static const uint repeatingLinearGradient = 3;
static const uint repeatingRadialGradient = 4;
static const uint repeatingConicGradient = 5;
static const uint blurTaps = 7;
static const uint premultipliedTexture = 0;
static const uint straightTexture = 1;
static const uint maskTexture = 2;

struct Vertex {
    float x;
    float y;
    uint color;
    float u;
    float v;
};

struct GeometryArguments {
    float4 transform[4];
    Ptr<Vertex> vertices;
    float2 translation;
    TextureHandle texture;
    SamplerHandle sampler;
    Ptr<float4> stopColors;
    Ptr<float> stopPositions;
    float2 gradientStart;
    float2 gradientVector;
    uint gradientFunction;
    uint stopCount;
    uint textureMode;
};

struct PassArguments {
    float4 colorMatrix[4];
    float4 target;
    float4 source;
    float4 limits;
    float4 color;
    float4 weights;
    float2 texelOffset;
    TextureHandle texture;
    TextureHandle mask;
    SamplerHandle sampler;
    float opacity;
};

}
