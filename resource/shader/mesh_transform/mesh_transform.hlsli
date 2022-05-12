#ifndef ILLUMINATE_RESOURCE_SHADER_MESH_TRANSFORM_HLSLI
#define ILLUMINATE_RESOURCE_SHADER_MESH_TRANSFORM_HLSLI
#include "shader/include/shader_defines.h"
#include "shader/include/mesh_deform_defines.hlsli"
#include "shader/include/material_defines.hlsli"
struct MeshTransformVsOutput {
  float4 position : SV_POSITION;
  float2 uv0      : TEXCOORD0;
};
struct ModelInfo {
  uint transform_offset;
  uint material_offset;
};
ConstantBuffer<ModelInfo>     model_info : register(b0);
ConstantBuffer<SceneCbvData>  scene_data : register(b1);
ByteAddressBuffer             transforms : register(t0);
#endif
