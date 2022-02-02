#ifndef ILLUMINATE_D3D12_SCENE_H
#define ILLUMINATE_D3D12_SCENE_H
#include "d3d12_header_common.h"
namespace illuminate {
struct SceneData {
  uint32_t model_num{0};
  uint32_t* model_mesh_index{nullptr};
  uint32_t* mesh_index_buffer_len{nullptr};
  D3D12_INDEX_BUFFER_VIEW* mesh_index_buffer_view{nullptr};
  D3D12_VERTEX_BUFFER_VIEW* mesh_vertex_buffer_view_position{nullptr};
};
SceneData GetSceneFromTinyGltfText(const char* const gltf_text, const char* const base_dir = "");
}
#endif
