#include "shader/include/postprocess.hlsli"
uint src_index : register(b0);
Texture2D src[] : register(t0);
SamplerState tex_sampler : register(s0);
#define CopyFullscreenRootsig " \
DescriptorTable(CBV(b0),                                                \
                SRV(t0, numDescriptors=16),                             \
                visibility=SHADER_VISIBILITY_PIXEL),                    \
DescriptorTable(Sampler(s0), visibility=SHADER_VISIBILITY_PIXEL)        \
"
[RootSignature(CopyFullscreenRootsig)]
float4 MainPs(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  float3 color = src[src_index].Sample(tex_sampler, input.texcoord).rgb;
  return float4(color, 1.0f);
}
