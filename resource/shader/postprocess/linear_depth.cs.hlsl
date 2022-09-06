#include "shader/include/shader_functions.h"
#include "shader/postprocess/linear_depth.cs.h"
ConstantBuffer<LinearDepthCBuffer> params : register(b0);
Texture2D<float> depth                    : register(t0);
RWTexture2D<float> linear_depth           : register(u0);
#define LinearDepthRootsig                     \
  "DescriptorTable(                            \
   CBV(b0),                                    \
   SRV(t0),                                    \
   UAV(u0),                                    \
   visibility=SHADER_VISIBILITY_ALL)"
[RootSignature(LinearDepthRootsig)]
[numthreads(16,16,1)]
void main(uint3 thread_id: SV_DispatchThreadID, uint3 group_thread_id : SV_GroupThreadID) {
  uint3 location = uint3(thread_id.xy, 0);
  float d = depth.Load(location).r;
  linear_depth[location.xy] = GetLinearDepth(d, params.compact_projection_param);
}
