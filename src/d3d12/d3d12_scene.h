#ifndef ILLUMINATE_D3D12_SCENE_H
#define ILLUMINATE_D3D12_SCENE_H
#include "d3d12_header_common.h"
#include "d3d12_gpu_buffer_allocator.h"
namespace illuminate {
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
  D3D12_VERTEX_BUFFER_VIEW* submesh_vertex_buffer_view_position{nullptr};
  // buffer allocations
  uint32_t buffer_allocation_num{0};
  BufferAllocation* buffer_allocation_upload{nullptr};
  BufferAllocation* buffer_allocation_default{nullptr};
  // transform buffer info
  uint32_t transform_element_num{0};
  uint32_t transform_buffer_stride_size{0};
  uint32_t transform_buffer_allocation_index{0};
};
class MemoryAllocationJanitor;
SceneData GetSceneFromTinyGltfText(const char* const gltf_text, const char* const base_dir, D3D12MA::Allocator* gpu_buffer_allocator, MemoryAllocationJanitor* allocator);
SceneData GetSceneFromTinyGltfBinary(const char* const binary_filename, D3D12MA::Allocator* gpu_buffer_allocator, MemoryAllocationJanitor* allocator);
void ReleaseSceneData(SceneData* scene_data);
}
#endif
