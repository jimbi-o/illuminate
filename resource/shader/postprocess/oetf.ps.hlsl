#include "shader/postprocess/postprocess.hlsli"
#define INV_GAMMA 1.0f / 2.2f
Texture2D src : register(t0);
SamplerState tex_sampler : register(s0);
#define CopyFullscreenRootsig " \
DescriptorTable(SRV(t0, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL), \
DescriptorTable(Sampler(s0), visibility=SHADER_VISIBILITY_PIXEL) \
"
[RootSignature(CopyFullscreenRootsig)]
float4 main(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  float4 color = src.Sample(tex_sampler, input.texcoord);
  return pow(color, INV_GAMMA);
}
