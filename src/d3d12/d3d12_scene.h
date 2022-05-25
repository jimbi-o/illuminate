#ifndef ILLUMINATE_D3D12_SCENE_H
#define ILLUMINATE_D3D12_SCENE_H
#include "D3D12MemAlloc.h"
#include "d3d12_header_common.h"
#include "d3d12_gpu_buffer_allocator.h"
#include "shader_defines.h"
namespace illuminate {
class MemoryAllocationJanitor;
struct ResourceTransfer;
enum SceneDescriptorHandle {
  kSceneDescriptorTransform = 0,
  kSceneDescriptorMaterialCommonSettings,
  kSceneDescriptorMaterialIndices,
  kSceneDescriptorTexture,
  kSceneDescriptorSampler,
  kSceneDescriptorHandleTypeNum,
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
  D3D12_INDEX_BUFFER_VIEW* submesh_index_buffer_view{nullptr};
  uint32_t* submesh_vertex_buffer_view_index[kVertexBufferTypeNum]{nullptr};
  D3D12_VERTEX_BUFFER_VIEW* submesh_vertex_buffer_view{nullptr};
  StrHash* submesh_material_variation_hash{nullptr};
  uint32_t* submesh_material_index{nullptr};
  // scene resources
  D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[kSceneDescriptorHandleTypeNum]{};
  uint32_t texture_num{};
  uint32_t sampler_num{};
  uint32_t resource_num{};
  ID3D12Resource** resources{};
  D3D12MA::Allocation** allocations{};
  ID3D12DescriptorHeap* descriptor_heap{nullptr};
  ID3D12DescriptorHeap* sampler_descriptor_heap{nullptr};
};
static const uint32_t kTextureNumPerMaterial = 4;
static const char kSceneSamplerName[] = "scene";
static const uint32_t kSceneSamplerId = 0xFFFF0000;
SceneData GetSceneFromTinyGltf(const char* const filename, const uint32_t frame_index, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, MemoryAllocationJanitor* allocator, ResourceTransfer* resource_transfer);
void ReleaseSceneData(SceneData* scene_data);
bool IsSceneBuffer(const char* const name);
uint32_t EncodeSceneBufferIndex(const char* const name);
bool IsSceneBuffer(const uint32_t buffer_index);
uint32_t GetDecodedSceneBufferIndex(const uint32_t buffer_index);
}
#endif
