#include "VideoShaders.h"

struct Varyings {
    float4 position : SV_Position;
};

[shader("vertex")]
Varyings fullscreenMain(uint index : SV_VertexID) {
    Varyings output;
    output.position = float4(float((index << 1u) & 2u) * 2.0 - 1.0, float(index & 2u) * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 convertMain(Varyings input) : SV_Target0 {
    const VideoShaders::Arguments arguments = rootArguments<VideoShaders::Arguments>();
    const float2 place = input.position.xy / arguments.size;
    const float4 yuv = float4(arguments.luma.Load(int2(input.position.xy)).r, arguments.blueDifference.SampleLevel(arguments.sampler, place, 0.0).r,
                              arguments.redDifference.SampleLevel(arguments.sampler, place, 0.0).r, 1.0);
    const float3 rgb = float3(dot(arguments.colorMatrix[0], yuv), dot(arguments.colorMatrix[1], yuv), dot(arguments.colorMatrix[2], yuv));
    return float4(saturate(rgb), 1.0);
}
