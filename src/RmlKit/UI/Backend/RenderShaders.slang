#include "RenderShaders.h"

struct GeometryVaryings {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float2 texCoord : TEXCOORD0;
};

struct PassVaryings {
    float4 position : SV_Position;
    float2 texCoord : TEXCOORD0;
};

float4 unpackColor(uint color) {
    return float4(float(color & 0xffu), float((color >> 8u) & 0xffu), float((color >> 16u) & 0xffu), float(color >> 24u)) / 255.0;
}

float wrap(float value, float period) {
    return value - period * floor(value / period);
}

[shader("vertex")]
GeometryVaryings geometryMain(uint index : SV_VertexID) {
    const RenderShaders::GeometryArguments arguments = rootArguments<RenderShaders::GeometryArguments>();
    const RenderShaders::Vertex vertex = arguments.vertices[index];
    const float4 position = float4(vertex.x + arguments.translation.x, vertex.y + arguments.translation.y, 0.0, 1.0);
    const float w = dot(arguments.transform[3], position);
    GeometryVaryings output;
    output.position = float4(dot(arguments.transform[0], position), dot(arguments.transform[1], position), 0.5 * w, w);
    output.color = unpackColor(vertex.color);
    output.texCoord = float2(vertex.u, vertex.v);
    return output;
}

[shader("fragment")]
float4 colorMain(GeometryVaryings input) : SV_Target0 {
    return input.color;
}

[shader("fragment")]
float4 textureMain(GeometryVaryings input) : SV_Target0 {
    const RenderShaders::GeometryArguments arguments = rootArguments<RenderShaders::GeometryArguments>();
    return input.color * arguments.texture.SampleLevel(arguments.sampler, input.texCoord, 0.0);
}

[shader("fragment")]
float4 gradientMain(GeometryVaryings input) : SV_Target0 {
    const RenderShaders::GeometryArguments arguments = rootArguments<RenderShaders::GeometryArguments>();
    const uint function = arguments.gradientFunction;
    const float2 offset = input.texCoord - arguments.gradientStart;
    const float2 v = arguments.gradientVector;
    float t;
    if (function == RenderShaders::linearGradient || function == RenderShaders::repeatingLinearGradient)
        t = dot(v, offset) / dot(v, v);
    else if (function == RenderShaders::radialGradient || function == RenderShaders::repeatingRadialGradient)
        t = length(v * offset);
    else
        t = 0.5 + atan2(-(v.x * offset.x + v.y * offset.y), v.x * offset.y - v.y * offset.x) / (2.0 * 3.14159265);
    if (function >= RenderShaders::repeatingLinearGradient) {
        const float first = arguments.stopPositions[0];
        const float last = arguments.stopPositions[arguments.stopCount - 1];
        t = first + wrap(t - first, last - first);
    }
    float4 color = arguments.stopColors[0];
    for (uint stop = 1; stop < arguments.stopCount; ++stop)
        color = lerp(color, arguments.stopColors[stop], smoothstep(arguments.stopPositions[stop - 1], arguments.stopPositions[stop], t));
    return input.color * color;
}

[shader("vertex")]
PassVaryings passMain(uint index : SV_VertexID) {
    const RenderShaders::PassArguments arguments = rootArguments<RenderShaders::PassArguments>();
    const float2 corners[6] = {float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0), float2(0.0, 1.0), float2(1.0, 0.0), float2(1.0, 1.0)};
    const float2 corner = corners[index];
    PassVaryings output;
    output.position = float4(lerp(arguments.target.xy, arguments.target.zw, corner), 0.0, 1.0);
    output.texCoord = lerp(arguments.source.xy, arguments.source.zw, corner);
    return output;
}

[shader("fragment")]
float4 clearMain(PassVaryings input) : SV_Target0 {
    return float4(0.0);
}

[shader("fragment")]
float4 copyMain(PassVaryings input) : SV_Target0 {
    const RenderShaders::PassArguments arguments = rootArguments<RenderShaders::PassArguments>();
    return arguments.texture.SampleLevel(arguments.sampler, input.texCoord, 0.0) * arguments.opacity;
}

[shader("fragment")]
float4 colorMatrixMain(PassVaryings input) : SV_Target0 {
    const RenderShaders::PassArguments arguments = rootArguments<RenderShaders::PassArguments>();
    const float4 color = arguments.texture.SampleLevel(arguments.sampler, input.texCoord, 0.0);
    return float4(dot(arguments.colorMatrix[0], color), dot(arguments.colorMatrix[1], color), dot(arguments.colorMatrix[2], color), color.a);
}

[shader("fragment")]
float4 blendMaskMain(PassVaryings input) : SV_Target0 {
    const RenderShaders::PassArguments arguments = rootArguments<RenderShaders::PassArguments>();
    const float4 color = arguments.texture.SampleLevel(arguments.sampler, input.texCoord, 0.0);
    return color * arguments.mask.SampleLevel(arguments.sampler, input.texCoord, 0.0).a;
}

float insideLimits(float4 limits, float2 texCoord) {
    const float2 inside = step(limits.xy, texCoord) * step(texCoord, limits.zw);
    return inside.x * inside.y;
}

[shader("fragment")]
float4 blurMain(PassVaryings input) : SV_Target0 {
    const RenderShaders::PassArguments arguments = rootArguments<RenderShaders::PassArguments>();
    const int center = int(RenderShaders::blurTaps / 2u);
    float4 color = float4(0.0);
    for (int tap = 0; tap < int(RenderShaders::blurTaps); ++tap) {
        const float2 texCoord = input.texCoord - float(tap - center) * arguments.texelOffset;
        const float weight = arguments.weights[abs(tap - center)] * insideLimits(arguments.limits, texCoord);
        color += arguments.texture.SampleLevel(arguments.sampler, texCoord, 0.0) * weight;
    }
    return color;
}

[shader("fragment")]
float4 dropShadowMain(PassVaryings input) : SV_Target0 {
    const RenderShaders::PassArguments arguments = rootArguments<RenderShaders::PassArguments>();
    const float alpha = arguments.texture.SampleLevel(arguments.sampler, input.texCoord, 0.0).a;
    return alpha * insideLimits(arguments.limits, input.texCoord) * arguments.color;
}
