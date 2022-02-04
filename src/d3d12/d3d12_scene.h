#ifndef ILLUMINATE_D3D12_SCENE_H
#define ILLUMINATE_D3D12_SCENE_H
#include "d3d12_header_common.h"
#include "d3d12_gpu_buffer_allocator.h"
namespace illuminate {
static const uint32_t kMeshBufferTypeNum = 2; // index, position
struct SceneData {
  uint32_t model_num{0};
  uint32_t* model_mesh_index{nullptr};
  uint32_t* mesh_index_buffer_len{nullptr};
  D3D12_INDEX_BUFFER_VIEW* mesh_index_buffer_view{nullptr};
  D3D12_VERTEX_BUFFER_VIEW* mesh_vertex_buffer_view_position{nullptr};
  uint32_t buffer_allocation_num{0};
  BufferAllocation* buffer_allocation_upload{nullptr};
  BufferAllocation* buffer_allocation_default{nullptr};
};
class MemoryAllocationJanitor;
SceneData GetSceneFromTinyGltfText(const char* const gltf_text, const char* const base_dir, D3D12MA::Allocator* gpu_buffer_allocator, MemoryAllocationJanitor* allocator);
void ReleaseSceneData(SceneData* scene_data);
}
#endif
