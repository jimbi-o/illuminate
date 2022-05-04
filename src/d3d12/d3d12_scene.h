#ifndef ILLUMINATE_D3D12_SCENE_H
#define ILLUMINATE_D3D12_SCENE_H
#include "d3d12_header_common.h"
#include "d3d12_gpu_buffer_allocator.h"
#include "shader_defines.h"
namespace illuminate {
enum VertexBufferType : uint8_t {
  kVertexBufferTypePosition = 0,
  kVertexBufferTypeNormal,
  kVertexBufferTypeTangent,
  kVertexBufferTypeNum,
};
struct SceneData {
  uint32_t model_num{0};
  // per model data
  uint32_t* model_instance_num{nullptr};
  uint32_t* model_submesh_num{nullptr};
  uint32_t** model_submesh_index{nullptr};
  uint32_t* transform_offset{nullptr};
  // per submesh data
  uint32_t* submesh_index_buffer_len{nullptr};
  D3D12_INDEX_BUFFER_VIEW*  submesh_index_buffer_view{nullptr};
  D3D12_VERTEX_BUFFER_VIEW* submesh_vertex_buffer_view[kVertexBufferTypeNum]{};
  StrHash* submesh_material_variation_hash{nullptr};
  uint32_t* submesh_material_index{nullptr};
};
enum SceneBuffer {
  kSceneBufferTransform = 0,
  kSceneBufferMaterialIndices,
  kSceneBufferColors,
  kSceneBufferAlphaCutoff,
  kSceneBufferNum,
};
class MemoryAllocationJanitor;
struct MaterialList;
struct SceneResources {
  ID3D12Resource*  resource[kSceneBufferNum]{};
  ID3D12Resource** mesh_resources_upload{};
  ID3D12Resource** mesh_resources_default{};
  uint32_t mesh_resource_num{};
  uint32_t per_mesh_resource_size_in_bytes{};
};
SceneData GetSceneFromTinyGltfBinary(const char* const binary_filename, MemoryAllocationJanitor* allocator, SceneResources* scene_resources, uint32_t* used_resource_num);
}
#endif
