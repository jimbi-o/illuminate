#include "d3d12_scene.h"
#include "gl.h"
#include "d3d12_src_common.h"
namespace illuminate {
namespace {
bool GetTinyGltfModel(const char* const gltf_text, const char* const base_dir, tinygltf::Model* model) {
  using namespace tinygltf;
  TinyGLTF loader;
  std::string err;
  std::string warn;
  bool ret = loader.LoadASCIIFromString(model, &err, &warn, gltf_text, static_cast<uint32_t>(strlen(gltf_text)), base_dir);
  if (!warn.empty()) {
    logwarn("tinygltf:{}", warn.c_str());
  }
  if (!err.empty()) {
    logerror("tinygltf:{}", err.c_str());
  }
  return ret;
}
void GetModelNumMeshNum(const std::vector<tinygltf::Mesh>& meshes, uint32_t* model_num, uint32_t* mesh_num) {
  *model_num = 0;
  for (const auto& mesh : meshes) {
    *model_num += static_cast<uint32_t>(mesh.primitives.size());
  }
  *mesh_num = 0;
  for (uint32_t m = 0; m < meshes.size(); m++) {
    for (uint32_t p = 0; p < meshes[m].primitives.size(); p++) {
      const auto current_accessor_index = meshes[m].primitives[p].indices;
      bool processed = false;
      for (uint32_t m_processed = 0; m_processed <= m; m_processed++) {
        const auto p_max = m_processed == m ? p : static_cast<uint32_t>(meshes[m_processed].primitives.size());
        for (uint32_t p_processed = 0; p_processed < p_max; p_processed++) {
          if (meshes[m_processed].primitives[p_processed].indices == current_accessor_index) {
            processed = true;
            break;
          }
        }
        if (processed) { break; }
      }
      if (!processed) {
        (*mesh_num)++;
      }
    }
  }
}
} // namespace anonymous
SceneData GetSceneFromTinyGltfText(const char* const gltf_text, const char* const base_dir) {
  tinygltf::Model model;
  if (!GetTinyGltfModel(gltf_text, base_dir, &model)) {
    logerror("gltf load failed.", gltf_text, base_dir);
    return {};
  }
  uint32_t model_num = 0, mesh_num = 0;
  GetModelNumMeshNum(model.meshes, &model_num, &mesh_num);
  SceneData scene_data{};
  return scene_data;
}
} // namespace illuminate
#include "doctest/doctest.h"
TEST_CASE("load scene/tiny gltf") {
  auto gltf_text = R"(
{
  "scene": 0,
  "scenes" : [
    {
      "nodes" : [ 0 ]
    }
  ],
  "nodes" : [
    {
      "mesh" : 0
    }
  ],
  "meshes" : [
    {
      "primitives" : [ {
        "attributes" : {
          "POSITION" : 1
        },
        "indices" : 0
      } ]
    }
  ],
  "buffers" : [
    {
      "uri" : "data:application/octet-stream;base64,AAABAAIAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAACAPwAAAAA=",
      "byteLength" : 44
    }
  ],
  "bufferViews" : [
    {
      "buffer" : 0,
      "byteOffset" : 0,
      "byteLength" : 6,
      "target" : 34963
    },
    {
      "buffer" : 0,
      "byteOffset" : 8,
      "byteLength" : 36,
      "target" : 34962
    }
  ],
  "accessors" : [
    {
      "bufferView" : 0,
      "byteOffset" : 0,
      "componentType" : 5123,
      "count" : 3,
      "type" : "SCALAR",
      "max" : [ 2 ],
      "min" : [ 0 ]
    },
    {
      "bufferView" : 1,
      "byteOffset" : 0,
      "componentType" : 5126,
      "count" : 3,
      "type" : "VEC3",
      "max" : [ 1.0, 1.0, 0.0 ],
      "min" : [ 0.0, 0.0, 0.0 ]
    }
  ],
  "asset" : {
    "version" : "2.0"
  }
}
)";
  using namespace illuminate;
  tinygltf::Model model;
  CHECK_UNARY(GetTinyGltfModel(gltf_text, "", &model));
}
TEST_CASE("tinygltf model and mesh count") {
  using namespace illuminate;
  std::vector<tinygltf::Mesh> meshes;
  uint32_t model_num = 0, mesh_num = 0;
  GetModelNumMeshNum(meshes, &model_num, &mesh_num);
  CHECK_EQ(model_num, 0);
  CHECK_EQ(mesh_num, 0);
  meshes.push_back({});
  GetModelNumMeshNum(meshes, &model_num, &mesh_num);
  CHECK_EQ(model_num, 0);
  CHECK_EQ(mesh_num, 0);
  meshes.back().primitives.push_back({});
  meshes.back().primitives.back().indices = 0;
  GetModelNumMeshNum(meshes, &model_num, &mesh_num);
  CHECK_EQ(model_num, 1);
  CHECK_EQ(mesh_num, 1);
  meshes.back().primitives.push_back({});
  meshes.back().primitives.back().indices = 1;
  GetModelNumMeshNum(meshes, &model_num, &mesh_num);
  CHECK_EQ(model_num, 2);
  CHECK_EQ(mesh_num, 2);
  meshes.push_back({});
  meshes.back().primitives.push_back({});
  meshes.back().primitives.back().indices = 2;
  GetModelNumMeshNum(meshes, &model_num, &mesh_num);
  CHECK_EQ(model_num, 3);
  CHECK_EQ(mesh_num, 3);
  meshes.back().primitives.back().indices = 1;
  GetModelNumMeshNum(meshes, &model_num, &mesh_num);
  CHECK_EQ(model_num, 3);
  CHECK_EQ(mesh_num, 2);
  meshes.front().primitives.front().indices = 1;
  GetModelNumMeshNum(meshes, &model_num, &mesh_num);
  CHECK_EQ(model_num, 3);
  CHECK_EQ(mesh_num, 1);
  meshes.push_back({});
  meshes.push_back({});
  meshes.push_back({});
  meshes.back().primitives.push_back({});
  meshes.back().primitives.back().indices = 2;
  meshes.back().primitives.push_back({});
  meshes.back().primitives.back().indices = 1;
  meshes.back().primitives.push_back({});
  meshes.back().primitives.back().indices = 4;
  meshes.back().primitives.push_back({});
  meshes.back().primitives.back().indices = 2;
  meshes.back().primitives.push_back({});
  meshes.back().primitives.back().indices = 5;
  GetModelNumMeshNum(meshes, &model_num, &mesh_num);
  CHECK_EQ(model_num, 8);
  CHECK_EQ(mesh_num, 4);
}
