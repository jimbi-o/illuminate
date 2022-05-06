#include "d3d12_scene.h"
#include "illuminate/math/math.h"
#include "d3d12_shader_compiler.h"
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
bool GetTinyGltfModel(const char* const filename, tinygltf::Model* model) {
  using namespace tinygltf;
  TinyGLTF loader;
  std::string err;
  std::string warn;
  bool result = loader.LoadASCIIFromFile(model, &err, &warn, filename);
  return CheckError(result, err, warn);
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
constexpr auto GetIndexBufferDxgiFormat(const uint32_t tinygltf_type, const uint32_t tinygltf_component_type) {
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
constexpr auto GetVertexBufferStrideInBytes(const uint32_t tinygltf_type, const uint32_t tinygltf_component_type) {
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
auto GetBufferSize(const size_t& count, const uint32_t component_type, const uint32_t type) {
  return GetUint32(count) * tinygltf::GetComponentSizeInBytes(component_type) * tinygltf::GetNumComponentsInType(type);
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
auto GetModelMaterialVariationHash(const tinygltf::Material& material) {
  StrHash hash{};
  hash = CombineHash(CalcStrHash("MESH_DEFORM_TYPE"), hash);
  hash = CombineHash(CalcStrHash("MESH_DEFORM_TYPE_STATIC"), hash);
  hash = CombineHash(CalcStrHash("OPACITY_TYPE"), hash);
  if (material.alphaMode.compare("MASK") == 0) {
    hash = CombineHash(CalcStrHash("OPACITY_TYPE_ALPHA_MASK"), hash);
  } else {
    assert(material.alphaMode.compare("OPAQUE") == 0);
    hash = CombineHash(CalcStrHash("OPACITY_TYPE_OPAQUE"), hash);
  }
  return hash;
}
enum { kIndiceMode, kVertexMode, };
template <uint32_t mode, typename F>
void SetMeshBuffers(const tinygltf::Model& model, const std::string& attribute_name, const SceneData& scene_data, SceneResources* scene_resources, uint32_t* used_resource_num, const std::string& buffer_name_prefix, F&& set_mesh_info) {
  uint32_t resource_index = *used_resource_num;
  uint32_t used_size_in_bytes = 0;
  auto dst = MapResource(scene_resources->mesh_resources_upload[resource_index], scene_resources->per_mesh_resource_size_in_bytes);
  auto head_addr = scene_resources->mesh_resources_default[resource_index]->GetGPUVirtualAddress();
  SetD3d12Name(scene_resources->mesh_resources_default[resource_index], buffer_name_prefix + "0_0");
  for (uint32_t i = 0; i < scene_data.model_num; i++) {
    const auto& primitives = model.meshes[i].primitives;
    for (uint32_t j = 0; j < scene_data.model_submesh_num[i]; j++) {
      const auto& primitive = primitives[j];
      if constexpr(mode == kVertexMode) {
        if (!primitive.attributes.contains(attribute_name)) {
          logwarn("missing {}. {} {} {}", attribute_name, i, j, scene_data.model_submesh_index[i][j]);
          continue;
        }
      }
      const auto accessor_index = (mode == kIndiceMode) ? primitive.indices : primitive.attributes.at(attribute_name);
      const auto& accessor = model.accessors[accessor_index];
      const auto buffer_size = GetBufferSize(accessor.count, accessor.componentType, accessor.type);
      if (used_size_in_bytes + buffer_size > scene_resources->per_mesh_resource_size_in_bytes) {
        UnmapResource(scene_resources->mesh_resources_upload[resource_index]);
        resource_index++;
        used_size_in_bytes = 0;
        dst = MapResource(scene_resources->mesh_resources_upload[resource_index], scene_resources->per_mesh_resource_size_in_bytes);
        head_addr = scene_resources->mesh_resources_default[resource_index]->GetGPUVirtualAddress();
        SetD3d12Name(scene_resources->mesh_resources_default[resource_index], buffer_name_prefix + std::to_string(i) + "_" + std::to_string(j));
      }
      assert(used_size_in_bytes + buffer_size <= scene_resources->per_mesh_resource_size_in_bytes && "implement buffer split");
      const auto& buffer_view = model.bufferViews[accessor.bufferView];
      memcpy(dst, &model.buffers[buffer_view.buffer].data[accessor.byteOffset + buffer_view.byteOffset], buffer_size);
      set_mesh_info(head_addr + used_size_in_bytes, buffer_size, accessor.type, accessor.componentType, accessor.count, scene_data.model_submesh_index[i][j]);
      used_size_in_bytes += buffer_size;
      dst = reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(dst) + buffer_size);
    }
  }
  *used_resource_num = resource_index + 1;
}
void SetSubmeshMaterials(const tinygltf::Model& model, SceneData* scene_data) {
  for (uint32_t i = 0; i < scene_data->model_num; i++) {
    const auto& primitives = model.meshes[i].primitives;
    for (uint32_t j = 0; j < scene_data->model_submesh_num[i]; j++) {
      const auto& primitive = primitives[j];
      const auto mesh_index = scene_data->model_submesh_index[i][j];
      if (primitive.material >= 0 && primitive.material < model.materials.size()) {
        scene_data->submesh_material_variation_hash[mesh_index] = GetModelMaterialVariationHash(model.materials[primitive.material]);
        scene_data->submesh_material_index[mesh_index] = primitive.material;
      } else {
        logwarn("invalid material i:{} j:{} mesh_index:{} m:{} msize:{}", i, j, mesh_index, primitive.material, model.materials.size());
        scene_data->submesh_material_variation_hash[mesh_index] = MaterialList::kInvalidIndex;
        scene_data->submesh_material_index[mesh_index] = 0;
      }
    }
  }
}
void SetTransformValues(const tinygltf::Model& model, SceneData* scene_data, const uint32_t mesh_num, ID3D12Resource* resource) {
  uint32_t transform_element_num = 0;
  for (uint32_t i = 0; i < scene_data->model_num; i++) {
    scene_data->transform_offset[i] = transform_element_num;
    transform_element_num += scene_data->model_instance_num[i];
  }
  auto tmp_allocator = GetTemporalMemoryAllocator();
  auto transform_offset_index = AllocateArray<uint32_t>(&tmp_allocator, scene_data->model_num);
  for (uint32_t i = 0; i < scene_data->model_num; i++) {
    transform_offset_index[i] = 0;
  }
  auto transform_buffer = MapResource(resource, sizeof(matrix) * mesh_num);
  for (const auto& node : model.scenes[0].nodes) {
    matrix m{};
    GetIdentityMatrix(m);
    SetTransform(model, node, m, scene_data->transform_offset, transform_offset_index, static_cast<matrix*>(transform_buffer));
  }
  UnmapResource(resource);
}
auto IsColorSame(const float* a, const double* b) {
  const auto epsilon = static_cast<double>(std::numeric_limits<float>::epsilon());
  for (uint32_t i = 0; i < 4; i++) {
    if (std::abs(a[i] - b[i]) > epsilon) { return false; }
  }
  return true;
}
void SetColor(const double* src, float* dst) {
  for (uint32_t i = 0; i < 4; i++) {
    dst[i] = static_cast<float>(src[i]);
  }
}
auto FindAlbedoFactorIndex(const std::array<float, 4>* colors, const uint32_t num, const std::vector<double>& finding_color) {
  for (uint32_t i = 0; i < num; i++) {
    if (IsColorSame(colors[i].data(), finding_color.data())) { return i; }
  }
  return ~0U;
}
auto FindFloat(const float* list, const uint32_t num, const double finding_value) {
  const auto epsilon = static_cast<double>(std::numeric_limits<float>::epsilon());
  for (uint32_t i = 0; i < num; i++) {
    if (std::abs(list[i] - finding_value <= epsilon)) { return i; }
  }
  return ~0U;
}
void SetMaterialValues(const tinygltf::Model& model, SceneResources* scene_resources) {
  auto tmp_allocator = GetTemporalMemoryAllocator();
  const auto material_num = GetUint32(model.materials.size());
  auto material_indices = AllocateArray<shader::MaterialIndexList>(&tmp_allocator, material_num);
  auto material_colors = AllocateArray<std::array<float, 4>>(&tmp_allocator, material_num);
  auto alpha_cutoffs = AllocateArray<float>(&tmp_allocator, material_num);
  uint32_t next_color_index = 0;
  uint32_t next_alpha_cutoff_index = 0;
  const auto white_texture_index = GetUint32(model.images.size()); // TODO
  const auto bilinear_sampler_index = GetUint32(model.samplers.size()); // TODO
  for (uint32_t i = 0; i < material_num; i++) {
    const auto& src_material = model.materials[i];
    auto& material_index = material_indices[i];
    {
      // albedo factor
      auto albedo_factor_index = FindAlbedoFactorIndex(material_colors, next_color_index, src_material.pbrMetallicRoughness.baseColorFactor);
      if (albedo_factor_index > next_color_index) {
        SetColor(src_material.pbrMetallicRoughness.baseColorFactor.data(), material_colors[next_color_index].data());
        albedo_factor_index = next_color_index;
        next_color_index++;
      }
      material_index.albedo_factor = albedo_factor_index;
    }
    {
      // albedo tex, sampler
      const auto& texture_index = src_material.pbrMetallicRoughness.baseColorTexture.index;
      if (texture_index == -1) {
        material_index.albedo_tex = white_texture_index;
        material_index.albedo_sampler = bilinear_sampler_index;
      } else {
        const auto& texture_info = model.textures[texture_index];
        material_index.albedo_tex = texture_info.source;
        material_index.albedo_sampler = texture_info.sampler;
      }
    }
    {
      // alpha cutoff
      auto alpha_cutoff_index = FindFloat(alpha_cutoffs, next_alpha_cutoff_index, src_material.alphaCutoff);
      if (alpha_cutoff_index > next_alpha_cutoff_index) {
        alpha_cutoffs[next_alpha_cutoff_index] = static_cast<float>(src_material.alphaCutoff);
        next_alpha_cutoff_index++;
      }
      material_index.alpha_cutoff = alpha_cutoff_index;
    }
  }
  {
    const auto indice_copy_size = GetUint32(sizeof(shader::MaterialIndexList)) * material_num;
    memcpy(MapResource(scene_resources->resource[kSceneBufferMaterialIndices], indice_copy_size), material_indices, indice_copy_size);
    UnmapResource(scene_resources->resource[kSceneBufferMaterialIndices]);
  }
  if (next_color_index > 0) {
    auto color_dst = MapResource(scene_resources->resource[kSceneBufferColors], next_color_index * sizeof(float) * 4);
    for (uint32_t i = 0; i < next_color_index; i++) {
      memcpy(color_dst, material_colors[i].data(), sizeof(float) * 4);
      color_dst = SucceedPtrWithStructSize<float[4]>(color_dst);
    }
    UnmapResource(scene_resources->resource[kSceneBufferColors]);
  }
  if (next_alpha_cutoff_index > 0) {
    const auto copy_size = GetUint32(sizeof(float)) * next_alpha_cutoff_index;
    memcpy(MapResource(scene_resources->resource[kSceneBufferAlphaCutoff], copy_size), alpha_cutoffs, copy_size);
    UnmapResource(scene_resources->resource[kSceneBufferAlphaCutoff]);
  }
}
auto ParseTinyGltfScene(const tinygltf::Model& model, MemoryAllocationJanitor* allocator, SceneResources* scene_resources, uint32_t* used_resource_num) {
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
  *used_resource_num = 0;
  SetMeshBuffers<kIndiceMode>(model, "", scene_data, scene_resources, used_resource_num, "idx",
                              [submesh_index_buffer_len = scene_data.submesh_index_buffer_len,
                               submesh_index_buffer_view = scene_data.submesh_index_buffer_view]
                              (const uint64_t& addr, const uint32_t size, const uint32_t tinygltf_type, const uint32_t tinygltf_component_type, const uint64_t& accessor_count, const uint32_t index) {
                                submesh_index_buffer_len[index] = GetUint32(accessor_count);
                                submesh_index_buffer_view[index] = {
                                  .BufferLocation = addr,
                                  .SizeInBytes    = size,
                                  .Format         = GetIndexBufferDxgiFormat(tinygltf_type, tinygltf_component_type),
                                };
                              });
  const std::string attributes[] = {"POSITION", "NORMAL", "TANGENT",};
  static_assert(std::size(attributes) == kVertexBufferTypeNum);
  const std::string vbname[] = {"pos", "normal", "tangent",};
  static_assert(std::size(vbname) == kVertexBufferTypeNum);
  for (uint32_t i = 0; i < kVertexBufferTypeNum; i++) {
    scene_data.submesh_vertex_buffer_view[i] = AllocateArray<D3D12_VERTEX_BUFFER_VIEW>(allocator, mesh_num);
    SetMeshBuffers<kVertexMode>(model, attributes[i], scene_data, scene_resources, used_resource_num, std::move(vbname[i]),
                                [submesh_vertex_buffer_view = scene_data.submesh_vertex_buffer_view[i]]
                                (const uint64_t& addr, const uint32_t size, const uint32_t tinygltf_type, const uint32_t tinygltf_component_type, [[maybe_unused]]const uint64_t& accessor_count, const uint32_t index) {
                                  submesh_vertex_buffer_view[index] = {
                                    .BufferLocation = addr,
                                    .SizeInBytes    = size,
                                    .StrideInBytes  = GetVertexBufferStrideInBytes(tinygltf_type, tinygltf_component_type),
                                  };
                                });
  }
  scene_data.submesh_material_variation_hash = AllocateArray<StrHash>(allocator, mesh_num);
  scene_data.submesh_material_index = AllocateArray<uint32_t>(allocator, mesh_num);
  SetSubmeshMaterials(model, &scene_data);
  for (const auto& node : model.scenes[0].nodes) {
    CountModelInstanceNum(model, node, scene_data.model_instance_num);
  }
  SetTransformValues(model, &scene_data, mesh_num, scene_resources->resource[kSceneBufferTransform]);
  SetMaterialValues(model, scene_resources);
  return scene_data;
}
} // namespace anonymous
SceneData GetSceneFromTinyGltf(const char* const filename, MemoryAllocationJanitor* allocator, SceneResources* scene_resources, uint32_t* used_resource_num) {
  tinygltf::Model model;
  if (!GetTinyGltfModel(filename, &model)) {
    logerror("gltf load failed. {}", filename);
    return {};
  }
  return ParseTinyGltfScene(model, allocator, scene_resources, used_resource_num);
}
D3D12_RESOURCE_DESC1 GetModelBufferDesc(const uint32_t size_in_bytes) {
  return {
    .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
    .Alignment = 0,
    .Width = size_in_bytes,
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
} // namespace illuminate
