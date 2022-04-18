#include "shader/test/prez.hlsli"
[RootSignature(PrezRootsig)]
float4 main(PrezVsOutput input) : SV_TARGET0 {
  return float4(1.0f, 0.0f, 1.0f, 1.0f);
}
