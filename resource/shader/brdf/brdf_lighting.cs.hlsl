#include "shader/brdf/brdf.hlsli"
#include "shader/include/shader_defines.h"
#include "shader/include/material_defines.hlsli"
#include "shader/include/material_functions.hlsli"
#include "shader/mesh_transform/mesh_transform.hlsli"
ConstantBuffer<SceneLightData> scene_light : register(b0);
Texture2D<float4> gbuffer[4] : register(t0);
RWTexture2D<float4> primary : register(u0);
#define BrdfLightingRootsig                    \
  "DescriptorTable(                            \
   CBV(b0),                                    \
   SRV(t0, numDescriptors=4),                  \
   UAV(u0),                                    \
   visibility=SHADER_VISIBILITY_ALL)"
[RootSignature(BrdfLightingRootsig)]
[numthreads(32,32,1)]
void main(uint3 thread_id: SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID) {
  uint3 location = uint3(thread_id.xy, 0);
  float4 gbuffer0 = gbuffer[0].Load(location);
  // TODO lighting
  primary[location.xy] = gbuffer0;
}
