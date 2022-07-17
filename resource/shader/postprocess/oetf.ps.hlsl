#include "shader/postprocess/postprocess.hlsli"
Texture2D src : register(t0);
SamplerState tex_sampler : register(s0);
#define CopyFullscreenRootsig " \
DescriptorTable(SRV(t0, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL), \
DescriptorTable(Sampler(s0), visibility=SHADER_VISIBILITY_PIXEL) \
"
[RootSignature(CopyFullscreenRootsig)]
float4 main(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  float4 color = src.Sample(tex_sampler, input.texcoord);
  if (all(isfinite(color))) {
    return color;
  }
  return float4(1.0f, 0.0f, 1.0f, 1.0f);
}
