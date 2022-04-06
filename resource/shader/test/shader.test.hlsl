#include "shader/postprocess/postprocess.hlsli"
FullscreenTriangleVSOutput MainVs(uint id : SV_VERTEXID) {
  // https://www.reddit.com/r/gamedev/comments/2j17wk/a_slightly_faster_bufferless_vertex_shader_trick/
  FullscreenTriangleVSOutput output;
  output.texcoord.x = (id == 2) ?  2.0 :  0.0;
  output.texcoord.y = (id == 1) ?  2.0 :  0.0;
  output.position = float4(output.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
  return output;
}
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
float4 main(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  float4 color = src.Sample(tex_sampler, input.texcoord);
  return color * cbv_color;
}
