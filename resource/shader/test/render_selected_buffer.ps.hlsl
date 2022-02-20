#include "shader/include/postprocess.hlsli"
uint src_index : register(b0);
Texture2D src[] : register(t0);
SamplerState tex_sampler : register(s0);
#define CopyFullscreenRootsig " \
RootConstants(num32BitConstants=1, b0, visibility=SHADER_VISIBILITY_PIXEL), \
DescriptorTable(SRV(t0, numDescriptors=unbounded,                       \
                    flags = DESCRIPTORS_VOLATILE),                      \
                visibility=SHADER_VISIBILITY_PIXEL),                    \
DescriptorTable(Sampler(s0), visibility=SHADER_VISIBILITY_PIXEL)        \
"
[RootSignature(CopyFullscreenRootsig)]
float4 MainPs(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  float3 color = src[src_index].Sample(tex_sampler, input.texcoord).rgb;
  return float4(color, 1.0f);
}
