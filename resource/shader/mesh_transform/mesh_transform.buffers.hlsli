#ifndef ILLUMINATE_RESOURCE_SHADER_MESH_TRANSFORM_BUFFERS_HLSLI
#define ILLUMINATE_RESOURCE_SHADER_MESH_TRANSFORM_BUFFERS_HLSLI
#include "shader/mesh_transform/mesh_transform.hlsli"
ConstantBuffer<ModelInfo>       model_info  : register(b0);
ConstantBuffer<SceneCameraData> camera_data : register(b1);
ByteAddressBuffer               transforms  : register(t0);
#endif
