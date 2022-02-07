#include "d3d12_scene.h"
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
auto IsPrimitiveMeshIdentical(const tinygltf::Primitive& a, const tinygltf::Primitive& b) {
  if (a.indices != b.indices) { return false; }
  if (a.attributes.size() != b.attributes.size()) { return false; }
  for (const auto& [key, val] : a.attributes) {
    if (!b.attributes.contains(key)) { return false; }
    if (val != b.attributes.at(key)) { return false; }
  }
  return true;
}
auto IsIdenticalMeshExists(const std::vector<tinygltf::Mesh>& meshes, const uint32_t m_target, const uint32_t p_target) {
  const auto& target_primitive = meshes[m_target].primitives[p_target];
  for (uint32_t m = 0; m <= m_target; m++) {
    const auto p_max = m == m_target ? p_target : static_cast<uint32_t>(meshes[m].primitives.size());
    for (uint32_t p = 0; p < p_max; p++) {
      if (IsPrimitiveMeshIdentical(meshes[m].primitives[p], target_primitive)) {
        return true;
      }
    }
  }
  return false;
}
auto GetModelNum(const std::vector<tinygltf::Mesh>& meshes) {
  uint32_t model_num = 0;
  for (const auto& mesh : meshes) {
    model_num += static_cast<uint32_t>(mesh.primitives.size());
  }
  return model_num;
}
auto GetMeshNum(const std::vector<tinygltf::Mesh>& meshes) {
  uint32_t mesh_num = 0;
  for (uint32_t m = 0; m < meshes.size(); m++) {
    for (uint32_t p = 0; p < meshes[m].primitives.size(); p++) {
      if (!IsIdenticalMeshExists(meshes, m, p)) {
        mesh_num++;
      }
    }
  }
  return mesh_num;
}
auto GetIndexBufferDxgiFormat(const uint32_t tinygltf_type, const uint32_t tinygltf_component_type) {
  assert(tinygltf_type == TINYGLTF_TYPE_SCALAR);
  if (tinygltf_type != TINYGLTF_TYPE_SCALAR) {
    return DXGI_FORMAT_UNKNOWN;
  }
  if (tinygltf_component_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
    return DXGI_FORMAT_R16_UINT;
  }
  assert(tinygltf_component_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT);
  return DXGI_FORMAT_R32_UINT;
}
auto GetVertexBufferStrideInBytes(const uint32_t tinygltf_type, const uint32_t tinygltf_component_type) {
  if (tinygltf_component_type != TINYGLTF_COMPONENT_TYPE_FLOAT) {
    logerror("currently only float is accepted for vertex buffer {}", tinygltf_component_type);
    assert(false && "currently only float is accepted for vertex buffer");
    return 0U;
  }
  switch (tinygltf_type) {
    case TINYGLTF_TYPE_SCALAR: return 4U;
    case TINYGLTF_TYPE_VEC2:   return 8U;
    case TINYGLTF_TYPE_VEC3:   return 12U;
    case TINYGLTF_TYPE_VEC4:   return 16U;
  }
  logerror("invalid vertex buffer setup. {} {}", tinygltf_type, tinygltf_component_type);
  assert(false && "invalid vertex buffer setup");
  return 0U;
}
auto FillResourceData(const tinygltf::Buffer& buffer, const tinygltf::Accessor& accessor, const tinygltf::BufferView& buffer_view, const uint32_t size_in_bytes, ID3D12Resource* resource) {
  auto dst = MapResource(resource, size_in_bytes);
  if (dst == nullptr) { return false; }
  memcpy(dst, &buffer.data[accessor.byteOffset + buffer_view.byteOffset], size_in_bytes);
  UnmapResource(resource);
  return true;
}
} // namespace anonymous
SceneData GetSceneFromTinyGltfText(const char* const gltf_text, const char* const base_dir, D3D12MA::Allocator* gpu_buffer_allocator, MemoryAllocationJanitor* allocator) {
  tinygltf::Model model;
  if (!GetTinyGltfModel(gltf_text, base_dir, &model)) {
    logerror("gltf load failed.", gltf_text, base_dir);
    return {};
  }
  SceneData scene_data{};
  scene_data.model_num = GetModelNum(model.meshes);
  const auto mesh_num = GetMeshNum(model.meshes);
  scene_data.model_mesh_index = AllocateArray<uint32_t>(allocator, scene_data.model_num);
  scene_data.mesh_index_buffer_len = AllocateArray<uint32_t>(allocator, mesh_num);
  scene_data.mesh_index_buffer_view = AllocateArray<D3D12_INDEX_BUFFER_VIEW>(allocator, mesh_num);
  scene_data.mesh_vertex_buffer_view_position = AllocateArray<D3D12_VERTEX_BUFFER_VIEW>(allocator, mesh_num);
  scene_data.buffer_allocation_num = mesh_num * kMeshBufferTypeNum;
  scene_data.buffer_allocation_upload  = AllocateArray<BufferAllocation>(allocator, scene_data.buffer_allocation_num);
  scene_data.buffer_allocation_default = AllocateArray<BufferAllocation>(allocator, scene_data.buffer_allocation_num);
  uint32_t model_index = 0;
  uint32_t mesh_index = 0;
  for (uint32_t m = 0; m < model.meshes.size(); m++) {
    for (uint32_t p = 0; p < model.meshes[m].primitives.size(); p++) {
      const auto& target_primitive = model.meshes[m].primitives[p];
      bool identical_mesh_exists = false;
      uint32_t identical_mesh_index = 0;
      for (uint32_t m_processed = 0; m_processed <= m; m_processed++) {
        const auto p_max = m_processed == m ? p : static_cast<uint32_t>(model.meshes[m].primitives.size());
        for (uint32_t p_processed = 0; p_processed < p_max; p_processed++) {
          if (IsPrimitiveMeshIdentical(model.meshes[m_processed].primitives[p_processed], target_primitive)) {
            identical_mesh_exists = true;
            scene_data.model_mesh_index[model_index] = scene_data.model_mesh_index[identical_mesh_index];
            model_index++;
            break;
          } // if IsPrimitiveMeshIdentical
          identical_mesh_index++;
        } // p_processed
        if (identical_mesh_exists) { break; }
      } // m_processed
      if (identical_mesh_exists) { continue; }
      scene_data.model_mesh_index[model_index] = mesh_index;
      model_index++;
      const auto buffer_allocation_index_base = mesh_index * kMeshBufferTypeNum;
      {
        // index buffer
        const auto& accessor = model.accessors[target_primitive.indices];
        const auto& buffer_view = model.bufferViews[accessor.bufferView];
        scene_data.mesh_index_buffer_len[mesh_index] = static_cast<uint32_t>(accessor.count);
        D3D12_RESOURCE_DESC1 resource_desc{
          .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
          .Alignment = 0,
          .Width = buffer_view.byteLength,
          .Height = 1,
          .DepthOrArraySize = 1,
          .MipLevels = 1,
          .Format = DXGI_FORMAT_UNKNOWN,
          .SampleDesc = {
            .Count = 1,
            .Quality = 0,
          },
          .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
          .Flags = D3D12_RESOURCE_FLAG_NONE,
          .SamplerFeedbackMipRegion = {
            .Width = 0,
            .Height = 0,
            .Depth = 0,
          },
        };
        auto buffer_allocation_upload  = &scene_data.buffer_allocation_upload[buffer_allocation_index_base];
        *buffer_allocation_upload = CreateBuffer(D3D12_HEAP_TYPE_UPLOAD,  D3D12_RESOURCE_STATE_COMMON, resource_desc, nullptr, gpu_buffer_allocator);
        auto buffer_allocation_default = &scene_data.buffer_allocation_default[buffer_allocation_index_base];
        *buffer_allocation_default = CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, resource_desc, nullptr, gpu_buffer_allocator);
        SetD3d12Name(buffer_allocation_default->resource, "mesh_ib" + std::to_string(mesh_index));
        auto& d3d12_buffer_view = scene_data.mesh_index_buffer_view[mesh_index];
        d3d12_buffer_view.BufferLocation = buffer_allocation_default->resource->GetGPUVirtualAddress();
        d3d12_buffer_view.SizeInBytes = static_cast<uint32_t>(resource_desc.Width);
        d3d12_buffer_view.Format = GetIndexBufferDxgiFormat(accessor.type, accessor.componentType);
        if (!FillResourceData(model.buffers[buffer_view.buffer], accessor, buffer_view, d3d12_buffer_view.SizeInBytes, buffer_allocation_upload->resource)) {
          logerror("resource map failed for index buffer {} {} {}", m, p, d3d12_buffer_view.SizeInBytes);
          assert(false && "resource map failed for index buffer");
        }
      }
      if (target_primitive.attributes.contains("POSITION")) {
        // vertex buffer POSITION
        const auto& accessor = model.accessors[target_primitive.attributes.at("POSITION")];
        const auto& buffer_view = model.bufferViews[accessor.bufferView];
        auto& d3d12_buffer_view = scene_data.mesh_vertex_buffer_view_position[mesh_index];
        D3D12_RESOURCE_DESC1 resource_desc{
          .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
          .Alignment = 0,
          .Width = buffer_view.byteLength,
          .Height = 1,
          .DepthOrArraySize = 1,
          .MipLevels = 1,
          .Format = DXGI_FORMAT_UNKNOWN,
          .SampleDesc = {
            .Count = 1,
            .Quality = 0,
          },
          .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
          .Flags = D3D12_RESOURCE_FLAG_NONE,
          .SamplerFeedbackMipRegion = {
            .Width = 0,
            .Height = 0,
            .Depth = 0,
          },
        };
        auto buffer_allocation_upload  = &scene_data.buffer_allocation_upload[buffer_allocation_index_base + 1];
        *buffer_allocation_upload = CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_COMMON, resource_desc, nullptr, gpu_buffer_allocator);
        auto buffer_allocation_default = &scene_data.buffer_allocation_default[buffer_allocation_index_base + 1];
        *buffer_allocation_default = CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, resource_desc, nullptr, gpu_buffer_allocator);
        SetD3d12Name(buffer_allocation_default->resource, "mesh_pos" + std::to_string(mesh_index));
        d3d12_buffer_view.BufferLocation = buffer_allocation_default->resource->GetGPUVirtualAddress();
        d3d12_buffer_view.SizeInBytes = static_cast<uint32_t>(resource_desc.Width);
        d3d12_buffer_view.StrideInBytes = GetVertexBufferStrideInBytes(accessor.type, accessor.componentType);
        if (!FillResourceData(model.buffers[buffer_view.buffer], accessor, buffer_view, d3d12_buffer_view.SizeInBytes, buffer_allocation_upload->resource)) {
          logerror("resource map failed for vertex buffer position {} {} {}", m, p, d3d12_buffer_view.SizeInBytes);
          assert(false && "resource map failed for vertex buffer position");
        }
      } else {
        logwarn("no position buffer. m:{} p:{} model:{} mesh:{}", m, p, model_index, mesh_index);
      }
      mesh_index++;
    } // p
  } // m
  return scene_data;
}
void ReleaseSceneData(SceneData* scene_data) {
  for (uint32_t i = 0; i < scene_data->buffer_allocation_num; i++) {
    if (scene_data->buffer_allocation_upload[i].resource) {
      ReleaseBufferAllocation(&scene_data->buffer_allocation_upload[i]);
    }
    ReleaseBufferAllocation(&scene_data->buffer_allocation_default[i]);
  }
}
} // namespace illuminate
#include "doctest/doctest.h"
#include "d3d12_device.h"
#include "d3d12_dxgi_core.h"
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
  DxgiCore dxgi_core;
  CHECK_UNARY(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK_UNARY(device.Init(dxgi_core.GetAdapter())); // NOLINT
  auto gpu_buffer_allocator = GetBufferAllocator(dxgi_core.GetAdapter(), device.Get());
  auto tmp_allocator = GetTemporalMemoryAllocator();
  auto scene_data = GetSceneFromTinyGltfText(gltf_text, "", gpu_buffer_allocator, &tmp_allocator);
  CHECK_EQ(scene_data.model_num, 1);
  CHECK_EQ(scene_data.model_mesh_index[0], 0);
  CHECK_EQ(scene_data.mesh_index_buffer_len[0], 3);
  CHECK_NE(scene_data.mesh_index_buffer_view[0].BufferLocation, 0);
  CHECK_EQ(scene_data.mesh_index_buffer_view[0].SizeInBytes, 6);
  CHECK_EQ(scene_data.mesh_index_buffer_view[0].Format, DXGI_FORMAT_R16_UINT);
  CHECK_NE(scene_data.mesh_vertex_buffer_view_position[0].BufferLocation, 0);
  CHECK_EQ(scene_data.mesh_vertex_buffer_view_position[0].SizeInBytes, 12 * 3);
  CHECK_EQ(scene_data.mesh_vertex_buffer_view_position[0].StrideInBytes, 12);
  ReleaseSceneData(&scene_data);
  gpu_buffer_allocator->Release();
  device.Term();
  dxgi_core.Term();
}
TEST_CASE("tinygltf model and mesh count") {
  using namespace illuminate;
  std::vector<tinygltf::Mesh> meshes;
  CHECK_EQ(GetModelNum(meshes),  0);
  CHECK_EQ(GetMeshNum(meshes), 0);
  meshes.push_back({});
  CHECK_EQ(GetModelNum(meshes),  0);
  CHECK_EQ(GetMeshNum(meshes), 0);
  meshes.back().primitives.push_back({});
  meshes.back().primitives.back().indices = 0;
  CHECK_EQ(GetModelNum(meshes),  1);
  CHECK_EQ(GetMeshNum(meshes), 1);
  meshes.back().primitives.push_back({});
  meshes.back().primitives.back().indices = 1;
  CHECK_EQ(GetModelNum(meshes),  2);
  CHECK_EQ(GetMeshNum(meshes), 2);
  meshes.push_back({});
  meshes.back().primitives.push_back({});
  meshes.back().primitives.back().indices = 2;
  CHECK_EQ(GetModelNum(meshes),  3);
  CHECK_EQ(GetMeshNum(meshes), 3);
  meshes.back().primitives.back().indices = 1;
  CHECK_EQ(GetModelNum(meshes),  3);
  CHECK_EQ(GetMeshNum(meshes), 2);
  meshes.front().primitives.front().indices = 1;
  CHECK_EQ(GetModelNum(meshes),  3);
  CHECK_EQ(GetMeshNum(meshes), 1);
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
  CHECK_EQ(GetModelNum(meshes),  8);
  CHECK_EQ(GetMeshNum(meshes), 4);
}
