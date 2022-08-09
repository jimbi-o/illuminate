#ifndef ILLUMINATE_RESOURCE_SHADER_MESH_TRANSFORM_HLSLI
#define ILLUMINATE_RESOURCE_SHADER_MESH_TRANSFORM_HLSLI
#include "shader/include/material_defines.hlsli"
struct MeshTransformVsOutput {
  float4 position    : SV_POSITION;
#ifdef PREZ
#if OPACITY_TYPE==OPACITY_TYPE_ALPHA_MASK
  float2 uv0         : TEXCOORD0;
#endif
#else
  float3 position_vs : POSITION_VS;
  float3 normal      : NORMAL;
  float4 tangent     : TANGENT;
  float2 uv0         : TEXCOORD0;
#endif
};
struct ModelInfo {
  uint transform_offset;
  uint material_offset;
};
ConstantBuffer<ModelInfo>       model_info  : register(b0);
ConstantBuffer<SceneCameraData> camera_data : register(b1);
ByteAddressBuffer               transforms  : register(t0);
#endif
