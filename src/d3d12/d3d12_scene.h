#ifndef ILLUMINATE_D3D12_SCENE_H
#define ILLUMINATE_D3D12_SCENE_H
#include "d3d12_header_common.h"
namespace illuminate {
struct SceneData {
};
SceneData GetSceneFromTinyGltfText(const char* const gltf_text, const char* const base_dir = "");
}
#endif
