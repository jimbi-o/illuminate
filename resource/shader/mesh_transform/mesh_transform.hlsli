#ifndef ILLUMINATE_RESOURCE_SHADER_MESH_TRANSFORM_HLSLI
#define ILLUMINATE_RESOURCE_SHADER_MESH_TRANSFORM_HLSLI
struct MeshTransformVsOutput {
  float4 position    : SV_POSITION;
  float3 position_vs : POSITION_VS;
  float3 normal      : NORMAL;
  float4 tangent     : TANGENT;
  float2 uv0         : TEXCOORD0;
};
struct ModelInfo {
  uint transform_offset;
  uint material_offset;
};
ConstantBuffer<ModelInfo>       model_info  : register(b0);
ConstantBuffer<SceneCameraData> camera_data : register(b1);
ByteAddressBuffer               transforms  : register(t0);
#endif
