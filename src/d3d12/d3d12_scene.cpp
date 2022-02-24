#include "d3d12_scene.h"
#include "illuminate/math/math.h"
#include "d3d12_src_common.h"
namespace illuminate {
namespace {
bool CheckError(const bool ret, const std::string& err, const std::string& warn) {
  if (!warn.empty()) {
    logwarn("tinygltf:{}", warn.c_str());
  }
  if (!err.empty()) {
    logerror("tinygltf:{}", err.c_str());
  }
  return ret;
}
bool GetTinyGltfModel(const char* const gltf_text, const char* const base_dir, tinygltf::Model* model) {
  using namespace tinygltf;
  TinyGLTF loader;
  std::string err;
  std::string warn;
  bool result = loader.LoadASCIIFromString(model, &err, &warn, gltf_text, static_cast<uint32_t>(strlen(gltf_text)), base_dir);
  return CheckError(result, err, warn);
}
bool GetTinyGltfModelFromBinaryFile(const char* const binary_filename, tinygltf::Model* model) {
  using namespace tinygltf;
  TinyGLTF loader;
  std::string err;
  std::string warn;
  bool result = loader.LoadBinaryFromFile(model, &err, &warn, binary_filename);
  return CheckError(result, err, warn);
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
void FillResourceData(const tinygltf::Buffer& buffer, const tinygltf::Accessor& accessor, const tinygltf::BufferView& buffer_view, const uint32_t size_in_bytes, ID3D12Resource* resource) {
  auto dst = MapResource(resource, size_in_bytes);
  if (dst == nullptr) {
    logerror("MapResource failed. {}", size_in_bytes);
    assert(false);
  }
  memcpy(dst, &buffer.data[accessor.byteOffset + buffer_view.byteOffset], size_in_bytes);
  UnmapResource(resource);
}
auto GetBufferDesc(const uint32_t buffer_size) {
  return D3D12_RESOURCE_DESC1{
    .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
    .Alignment = 0,
    .Width = buffer_size,
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
}
void FillResourceData(const tinygltf::Model& model, const uint32_t accessor_index, SceneData* scene_data, uint32_t* buffer_allocation_index, D3D12_GPU_VIRTUAL_ADDRESS* buffer_location, uint32_t* index, uint32_t* size, DXGI_FORMAT* format, uint32_t* stride, D3D12MA::Allocator* gpu_buffer_allocator, const char* buffer_name) {
  const auto& accessor = model.accessors[accessor_index];
  const auto& buffer_view = model.bufferViews[accessor.bufferView];
  const auto buffer_size = GetUint32(buffer_view.byteLength);
  auto resource_desc  = GetBufferDesc(buffer_size);
  auto& upload_buffer = scene_data->buffer_allocation_upload[*buffer_allocation_index];
  upload_buffer = CreateBuffer(D3D12_HEAP_TYPE_UPLOAD,  D3D12_RESOURCE_STATE_COMMON, resource_desc, nullptr, gpu_buffer_allocator);
  auto& default_buffer = scene_data->buffer_allocation_default[*buffer_allocation_index];
  default_buffer = CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, resource_desc, nullptr, gpu_buffer_allocator);
  SetD3d12Name(upload_buffer.resource, buffer_name);
  FillResourceData(model.buffers[buffer_view.buffer], accessor, buffer_view, buffer_size, upload_buffer.resource);
  *buffer_location = default_buffer.resource->GetGPUVirtualAddress();
  if (index) {
    *index = GetUint32(accessor.count);
  }
  if (size) {
    *size = buffer_size;
  }
  if (format) {
    *format = GetIndexBufferDxgiFormat(accessor.type, accessor.componentType);
  }
  if (stride) {
    *stride = GetVertexBufferStrideInBytes(accessor.type, accessor.componentType);
  }
  (*buffer_allocation_index)++;
}
void CountModelInstanceNum(const tinygltf::Model& model, const uint32_t node_index, uint32_t* model_instance_num) {
  const auto& node = model.nodes[node_index];
  if (node.mesh != -1) {
    model_instance_num[node.mesh]++;
  }
  for (auto& child : node.children) {
    CountModelInstanceNum(model, child, model_instance_num);
  }
}
void SetTransform(const tinygltf::Model& model, const uint32_t node_index, const matrix& parent_transform, const uint32_t* transform_offset, uint32_t* transform_offset_index, matrix* transform_buffer) {
  const auto& node = model.nodes[node_index];
  if (node.mesh != -1) {
    const auto index = transform_offset[node.mesh] + transform_offset_index[node.mesh];
    memcpy(&(transform_buffer[index]), &parent_transform, sizeof(transform_buffer[index]));
    transform_offset_index[node.mesh]++;
  }
  matrix transform{};
  if (!node.matrix.empty()) {
    matrix t{};
    FillMatrix(node.matrix.data(), t);
    MultiplyMatrix(parent_transform, t, transform);
  } else {
    CopyMatrix(parent_transform, transform);
  }
  for (auto& child : node.children) {
    SetTransform(model, child, transform, transform_offset, transform_offset_index, transform_buffer);
  }
}
auto ParseTinyGltfScene(const tinygltf::Model& model, D3D12MA::Allocator* gpu_buffer_allocator, MemoryAllocationJanitor* allocator) {
  SceneData scene_data{};
  scene_data.model_num = GetUint32(model.meshes.size());
  scene_data.model_instance_num = AllocateArray<uint32_t>(allocator, scene_data.model_num);
  scene_data.model_submesh_num = AllocateArray<uint32_t>(allocator, scene_data.model_num);
  scene_data.model_submesh_index = AllocateArray<uint32_t*>(allocator, scene_data.model_num);
  scene_data.transform_offset = AllocateArray<uint32_t>(allocator, scene_data.model_num);
  uint32_t mesh_num = 0;
  for (uint32_t i = 0; i < scene_data.model_num; i++) {
    scene_data.model_instance_num[i] = 0;
    scene_data.model_submesh_num[i] = GetUint32(model.meshes[i].primitives.size());
    scene_data.model_submesh_index[i] = AllocateArray<uint32_t>(allocator, scene_data.model_submesh_num[i]);
    for (uint32_t j = 0; j < scene_data.model_submesh_num[i]; j++) {
      scene_data.model_submesh_index[i][j] = mesh_num;
      mesh_num++;
    }
  }
  scene_data.submesh_index_buffer_len = AllocateArray<uint32_t>(allocator, mesh_num);
  scene_data.submesh_index_buffer_view = AllocateArray<D3D12_INDEX_BUFFER_VIEW>(allocator, mesh_num);
  for (uint32_t i = 0; i < kVertexBufferTypeNum; i++) {
    scene_data.submesh_vertex_buffer_view[i] = AllocateArray<D3D12_VERTEX_BUFFER_VIEW>(allocator, mesh_num);
  }
  scene_data.buffer_allocation_num = mesh_num * kVertexBufferTypeNum + 1/*transform buffer*/;
  scene_data.buffer_allocation_upload  = AllocateArray<BufferAllocation>(allocator, scene_data.buffer_allocation_num);
  scene_data.buffer_allocation_default = AllocateArray<BufferAllocation>(allocator, scene_data.buffer_allocation_num);
  uint32_t buffer_allocation_index = 0;
  for (uint32_t i = 0; i < scene_data.model_num; i++) {
    const auto& primitives = model.meshes[i].primitives;
    for (uint32_t j = 0; j < scene_data.model_submesh_num[i]; j++) {
      const auto& primitive = primitives[j];
      const auto mesh_index = scene_data.model_submesh_index[i][j];
      {
        // index buffer
        auto& view = scene_data.submesh_index_buffer_view[mesh_index];
        FillResourceData(model, primitive.indices, &scene_data, &buffer_allocation_index, &view.BufferLocation, &scene_data.submesh_index_buffer_len[mesh_index], &view.SizeInBytes, &view.Format, nullptr, gpu_buffer_allocator, ("mesh_ib" + std::to_string(mesh_index)).data());
      }
      if (primitive.attributes.contains("POSITION")) {
        // position buffer
        auto& view = scene_data.submesh_vertex_buffer_view[kVertexBufferTypePosition][mesh_index];
        FillResourceData(model, primitive.attributes.at("POSITION"), &scene_data, &buffer_allocation_index, &view.BufferLocation, nullptr, &view.SizeInBytes, nullptr, &view.StrideInBytes, gpu_buffer_allocator, ("mesh_vbpos" + std::to_string(mesh_index)).data());
      } else {
        logwarn("missing POSITION. {} {} {}", i, j, mesh_index);
      }
      if (primitive.attributes.contains("NORMAL")) {
        // normal buffer
        auto& view = scene_data.submesh_vertex_buffer_view[kVertexBufferTypeNormal][mesh_index];
        FillResourceData(model, primitive.attributes.at("NORMAL"), &scene_data, &buffer_allocation_index, &view.BufferLocation, nullptr, &view.SizeInBytes, nullptr, &view.StrideInBytes, gpu_buffer_allocator, ("mesh_vbpos" + std::to_string(mesh_index)).data());
      } else {
        logwarn("missing NORMAL. {} {} {}", i, j, mesh_index);
      }
      if (primitive.attributes.contains("TANGENT")) {
        // tangent buffer
        auto& view = scene_data.submesh_vertex_buffer_view[kVertexBufferTypeTangent][mesh_index];
        FillResourceData(model, primitive.attributes.at("TANGENT"), &scene_data, &buffer_allocation_index, &view.BufferLocation, nullptr, &view.SizeInBytes, nullptr, &view.StrideInBytes, gpu_buffer_allocator, ("mesh_vbpos" + std::to_string(mesh_index)).data());
      } else {
        logwarn("missing TANGENT. {} {} {}", i, j, mesh_index);
      }
    }
  }
  for (const auto& node : model.scenes[0].nodes) {
    CountModelInstanceNum(model, node, scene_data.model_instance_num);
  }
  scene_data.transform_element_num = 0;
  for (uint32_t i = 0; i < scene_data.model_num; i++) {
    scene_data.transform_offset[i] = scene_data.transform_element_num;
    scene_data.transform_element_num += scene_data.model_instance_num[i];
  }
  scene_data.transform_buffer_allocation_index = buffer_allocation_index;
  buffer_allocation_index++;
  void* transform_buffer = nullptr;
  {
    scene_data.transform_buffer_stride_size = GetUint32(sizeof(matrix));
    const auto transform_buffer_size = scene_data.transform_buffer_stride_size * scene_data.transform_element_num;
    auto resource_desc = GetBufferDesc(transform_buffer_size);
    auto& upload_buffer = scene_data.buffer_allocation_upload[scene_data.transform_buffer_allocation_index];
    upload_buffer = CreateBuffer(D3D12_HEAP_TYPE_UPLOAD,  D3D12_RESOURCE_STATE_COMMON, resource_desc, nullptr, gpu_buffer_allocator);
    auto& default_buffer = scene_data.buffer_allocation_default[scene_data.transform_buffer_allocation_index];
    default_buffer = CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, resource_desc, nullptr, gpu_buffer_allocator);
    transform_buffer = MapResource(upload_buffer.resource, transform_buffer_size);
  }
  // TODO fill transform_buffer
  auto tmp_allocator = GetTemporalMemoryAllocator();
  auto transform_offset_index = AllocateArray<uint32_t>(&tmp_allocator, scene_data.model_num);
  for (uint32_t i = 0; i < scene_data.model_num; i++) {
    transform_offset_index[i] = 0;
  }
  for (const auto& node : model.scenes[0].nodes) {
    matrix m{};
    GetIdentityMatrix(m);
    SetTransform(model, node, m, scene_data.transform_offset, transform_offset_index, static_cast<matrix*>(transform_buffer));
  }
  UnmapResource(scene_data.buffer_allocation_upload[scene_data.transform_buffer_allocation_index].resource);
  assert(scene_data.buffer_allocation_num == buffer_allocation_index);
  return scene_data;
}
} // namespace anonymous
SceneData GetSceneFromTinyGltfText(const char* const gltf_text, const char* const base_dir, D3D12MA::Allocator* gpu_buffer_allocator, MemoryAllocationJanitor* allocator) {
  tinygltf::Model model;
  if (!GetTinyGltfModel(gltf_text, base_dir, &model)) {
    logerror("gltf load failed. {} {}", gltf_text, base_dir);
    return {};
  }
  return ParseTinyGltfScene(model, gpu_buffer_allocator, allocator);
}
SceneData GetSceneFromTinyGltfBinary(const char* const binary_filename, D3D12MA::Allocator* gpu_buffer_allocator, MemoryAllocationJanitor* allocator) {
  tinygltf::Model model;
  if (!GetTinyGltfModelFromBinaryFile(binary_filename, &model)) {
    logerror("gltf load failed. {}", binary_filename);
    return {};
  }
  return ParseTinyGltfScene(model, gpu_buffer_allocator, allocator);
}
void ReleaseSceneData(SceneData* scene_data) {
  for (uint32_t i = 0; i < scene_data->buffer_allocation_num; i++) {
    ReleaseBufferAllocation(&scene_data->buffer_allocation_upload[i]);
    ReleaseBufferAllocation(&scene_data->buffer_allocation_default[i]);
  }
}
} // namespace illuminate
