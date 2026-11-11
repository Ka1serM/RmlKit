#pragma once

#include "NoorRHI/Shaders/ShaderTypes.h"

namespace VideoShaders {

using namespace NoorRHI;

struct Arguments {
    float4 colorMatrix[3];
    float2 size;
    TextureHandle luma;
    TextureHandle blueDifference;
    TextureHandle redDifference;
    SamplerHandle sampler;
};

}
