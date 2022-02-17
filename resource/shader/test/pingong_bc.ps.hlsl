#include "shader/include/postprocess.hlsli"
float4 cbv_color : register(b0);
Texture2D src : register(t0);
SamplerState tex_sampler : register(s0);
#define CopyFullscreenRootsig " \
DescriptorTable(CBV(b0),                                                \
                SRV(t0, numDescriptors=1),                              \
                visibility=SHADER_VISIBILITY_PIXEL),                    \
DescriptorTable(Sampler(s0), visibility=SHADER_VISIBILITY_PIXEL)        \
"
[RootSignature(CopyFullscreenRootsig)]
float4 MainPs(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  float4 color = src.Sample(tex_sampler, input.texcoord);
  return color * cbv_color;
}
