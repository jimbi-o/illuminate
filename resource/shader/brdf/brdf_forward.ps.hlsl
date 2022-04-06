#include "shader/brdf/brdf.hlsli"
#include "shader/mesh_transform/mesh_transform.hlsli"
[RootSignature(MeshTransformRootsig)]
float4 main(MeshTransformVsOutput input) : SV_TARGET0 {
  return float4(0.0f, 1.0f, 1.0f, 1.0f);
}
