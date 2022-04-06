#include "shader/postprocess/postprocess.hlsli"
float4 cbv_color : register(b0);
#define CopyFullscreenRootsig "\
DescriptorTable(CBV(b0), visibility=SHADER_VISIBILITY_PIXEL),    \
"
[RootSignature(CopyFullscreenRootsig)]
float4 MainPs(FullscreenTriangleVSOutput input) : SV_TARGET0 {
  return cbv_color;
}
