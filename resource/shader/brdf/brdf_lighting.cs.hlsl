#include "shader/brdf/brdf.hlsli"
#include "shader/brdf/brdf_lighting.cs.h"
#include "shader/include/shader_defines.h"
#include "shader/include/shader_functions.h"
#include "shader/include/material_defines.hlsli"
#include "shader/include/material_functions.hlsli"
#include "shader/mesh_transform/mesh_transform.hlsli"
ConstantBuffer<BrdfLightingCBuffer> params : register(b0);
Texture2D<float4> gbuffer[4] : register(t0);
Texture2D<float>  depth      : register(t4);
RWTexture2D<float4> primary  : register(u0);
#define BrdfLightingRootsig                    \
  "DescriptorTable(                            \
   CBV(b0),                                    \
   SRV(t0, numDescriptors=5),                  \
   UAV(u0),                                    \
   visibility=SHADER_VISIBILITY_ALL)"
[RootSignature(BrdfLightingRootsig)]
[numthreads(16,16,1)]
void main(uint3 thread_id: SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID) {
  uint3 location = uint3(thread_id.xy, 0);
  float d = depth.Load(location).r;
  float4 gbuffer1 = gbuffer[1].Load(location);
  float4 gbuffer0 = gbuffer[0].Load(location);
  float4 gbuffer2 = gbuffer[2].Load(location);
  float4 gbuffer3 = gbuffer[3].Load(location);
  float linear_depth = GetLinearDepth(d, params.zbuffer_to_linear_params);
  float3 v = normalize(RecoverViewSpacePosition(thread_id.xy, d, params.zbuffer_to_linear_params));
  float3 l = params.light_direction_vs;
  float3 h = normalize(l + v); // half vector
  float vh = clamp(dot(v, h), 0.0f, 1.0f);
  float3 n = normalize(gbuffer1.rgb);
  float nl = clamp(dot(n, l), 0.0f, 1.0f);
  float nv = clamp(dot(n, v), 0.0f, 1.0f);
  float nh = clamp(dot(n, h), 0.0f, 1.0f);
  float3 brdf = BrdfDefault(gbuffer0.rgb, gbuffer2.r, gbuffer2.g, vh, nl, nv, nh);
  primary[location.xy] = float4((brdf * nl * params.light_color.xyz * params.light_color.w) * params.exposure_rate + gbuffer3.rgb, 1.0f);
}
