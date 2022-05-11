#ifndef ILLUMINATE_D3D12_SCENE_H
#define ILLUMINATE_D3D12_SCENE_H
#include "D3D12MemAlloc.h"
#include "d3d12_header_common.h"
#include "d3d12_gpu_buffer_allocator.h"
#include "shader_defines.h"
namespace illuminate {
class MemoryAllocationJanitor;
struct ResourceTransfer;
enum VertexBufferType : uint8_t {
  kVertexBufferTypePosition = 0,
  kVertexBufferTypeNormal,
  kVertexBufferTypeTangent,
  kVertexBufferTypeNum,
};
enum SceneBufferType {
  kSceneBufferTransform = 0,
  kSceneBufferMaterialIndices,
  kSceneBufferColors,
  kSceneBufferAlphaCutoff,
  kSceneBufferTextures,
  kSceneBufferMesh,
  kSceneBufferNum,
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
  // scene resources
  uint32_t resource_num[kSceneBufferNum]{};
  ID3D12Resource** resources[kSceneBufferNum]{};
  D3D12MA::Allocation** allocations[kSceneBufferNum]{};
  D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[kSceneBufferNum]{};
  ID3D12DescriptorHeap* descriptor_heap{nullptr};
};
SceneData GetSceneFromTinyGltf(const char* const filename, const uint32_t frame_index, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, MemoryAllocationJanitor* allocator, ResourceTransfer* resource_transfer);
void ReleaseSceneData(SceneData* scene_data);
bool IsSceneBuffer(const char* const name);
uint32_t EncodeSceneBufferIndex(const char* const name);
bool IsSceneBuffer(const uint32_t buffer_index);
const D3D12_CPU_DESCRIPTOR_HANDLE& GetSceneBufferHandle(const uint32_t buffer_index, const SceneData& scene_data);
}
#endif
