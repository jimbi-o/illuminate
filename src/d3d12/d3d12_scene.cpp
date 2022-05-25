#include "d3d12_scene.h"
#include <filesystem>
#include "SimpleMath.h"
#include "illuminate/math/math.h"
#include "illuminate/util/util_functions.h"
#include "d3d12_descriptors.h"
#include "d3d12_resource_transfer.h"
#include "d3d12_shader_compiler.h"
#include "d3d12_src_common.h"
#include "d3d12_texture_util.h"
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
  using namespace DirectX::SimpleMath;
  Matrix parent;
  CopyMatrix(parent_transform, parent.m);
  Matrix transform{};
  const auto& node = model.nodes[node_index];
  if (!node.matrix.empty()) {
    FillMatrix(node.matrix.data(), transform.m);
  } else {
    if (!node.scale.empty()) {
      float scale[3]{};
      for (uint32_t i = 0; i < 3; i++) {
        scale[i] = static_cast<float>(node.scale[i]);
      }
      transform *= Matrix::CreateScale(scale[0], scale[1], scale[2]);
    }
    if (!node.rotation.empty()) {
      float quat[4]{};
      for (uint32_t i = 0; i < 4; i++) {
        quat[i] = static_cast<float>(node.rotation[i]);
      }
      transform *= Matrix::CreateFromQuaternion(Quaternion(&quat[0]));
    }
    if (!node.translation.empty()) {
      float translation[3]{};
      for (uint32_t i = 0; i < 3; i++) {
        translation[i] = static_cast<float>(node.translation[i]);
      }
      transform *= Matrix::CreateTranslation(translation[0], translation[1], translation[2]);
    }
    transform = transform * parent;
  }
  if (node.mesh != -1) {
    const auto index = transform_offset[node.mesh] + transform_offset_index[node.mesh];
    memcpy(&(transform_buffer[index]), &transform.m[0][0], sizeof(transform_buffer[index]));
    logtrace("transform index{} node{} mesh{} val{}", transform_offset_index[node.mesh], node_index, index, transform_buffer[index][0][0]);
    transform_offset_index[node.mesh]++;
  }
  for (auto& child : node.children) {
    SetTransform(model, child, transform.m, transform_offset, transform_offset_index, transform_buffer);
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
void CreateUploadAndDefaultBuffers(const uint32_t buffer_size, D3D12MA::Allocator* buffer_allocator,
                                   const char* const name_upload, ID3D12Resource** resource_upload, D3D12MA::Allocation** allocation_upload,
                                   const char* const name_default, ID3D12Resource** resource_default, D3D12MA::Allocation** allocation_default) {
  const auto desc = GetBufferDesc(buffer_size);
  CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, desc, nullptr, buffer_allocator, allocation_upload, resource_upload);
  CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, desc, nullptr, buffer_allocator, allocation_default, resource_default);
  SetD3d12Name(*resource_upload, name_upload);
  SetD3d12Name(*resource_default, name_default);
  logtrace("buffer created size:{} {}:{:x} {}:{:x}", buffer_size, name_upload, reinterpret_cast<std::uintptr_t>(resource_upload), name_default, reinterpret_cast<std::uintptr_t>(resource_default));
}
auto PrepareSingleBufferTransfer(const uint32_t size_in_bytes, const char* const name_u, const char* const name_d, const uint32_t frame_index, SceneData* scene_data, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
  ID3D12Resource* resource_upload{nullptr};
  D3D12MA::Allocation* allocation_upload{nullptr};
  const auto index = scene_data->resource_num;
  scene_data->resource_num++;
  CreateUploadAndDefaultBuffers(size_in_bytes, buffer_allocator,
                                name_u, &resource_upload, &allocation_upload,
                                name_d, &scene_data->resources[index], &scene_data->allocations[index]);
  if (!ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, scene_data->resources[index], resource_transfer)) {
    logwarn("ReserveResourceTransfer failed. {}", name_d);
  }
  return std::make_pair(resource_upload, scene_data->resources[index]);
}
enum { kIndiceMode, kVertexMode, };
template <uint32_t mode, typename F>
auto SetMeshBuffers(const tinygltf::Model& model, const std::string& attribute_name, const SceneData& scene_data, const uint32_t frame_index, ID3D12Resource** resource_default, D3D12MA::Allocation** allocation_default, const std::string& buffer_name_prefix, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer, F&& set_mesh_info) {
  uint32_t resource_index = 0;
  for (uint32_t i = 0; i < scene_data.model_num; i++) {
    const auto& primitives = model.meshes[i].primitives;
    for (uint32_t j = 0; j < scene_data.model_submesh_num[i]; j++) {
      const auto& primitive = primitives[j];
      if constexpr(mode == kVertexMode) {
        if (!primitive.attributes.contains(attribute_name)) {
          if (!primitive.attributes.empty()) {
            const auto accessor_index = primitive.attributes.begin()->second;
            const auto& accessor = model.accessors[accessor_index];
            const auto buffer_size = GetBufferSize(accessor.count, accessor.componentType, accessor.type);
            ID3D12Resource* resource_upload{nullptr};
            D3D12MA::Allocation* allocation_upload{nullptr};
            CreateUploadAndDefaultBuffers(buffer_size, buffer_allocator,
                                          (buffer_name_prefix + "U" + std::to_string(i) + "_" + std::to_string(j)).c_str(), &resource_upload, &allocation_upload,
                                          (buffer_name_prefix + std::to_string(i) + "_" + std::to_string(j)).c_str(), &resource_default[resource_index], &allocation_default[resource_index]);
            auto dst = MapResource(resource_upload, buffer_size);
            auto addr = resource_default[resource_index]->GetGPUVirtualAddress();
            memset(dst, 0, buffer_size);
            set_mesh_info(addr, buffer_size, accessor.type, accessor.componentType, accessor.count, scene_data.model_submesh_index[i][j]);
            UnmapResource(resource_upload);
            ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, resource_default[resource_index], resource_transfer);
            logwarn("missing {}. {} {} {} {}. default buffer assigned", attribute_name, i, j, scene_data.model_submesh_index[i][j], resource_index);
            resource_index++;
          } else {
            logwarn("missing {}. {} {} {} {}. no attributes found", attribute_name, i, j, scene_data.model_submesh_index[i][j], resource_index);
          }
          continue;
        }
      }
      const auto accessor_index = (mode == kIndiceMode) ? primitive.indices : primitive.attributes.at(attribute_name);
      const auto& accessor = model.accessors[accessor_index];
      const auto buffer_size = GetBufferSize(accessor.count, accessor.componentType, accessor.type);
      ID3D12Resource* resource_upload{nullptr};
      D3D12MA::Allocation* allocation_upload{nullptr};
      {
        // create resources
        CreateUploadAndDefaultBuffers(buffer_size, buffer_allocator,
                                      (buffer_name_prefix + "U" + std::to_string(i) + "_" + std::to_string(j)).c_str(), &resource_upload, &allocation_upload,
                                      (buffer_name_prefix + std::to_string(i) + "_" + std::to_string(j)).c_str(), &resource_default[resource_index], &allocation_default[resource_index]);
      }
      {
        // fill upload buffer
        auto dst = MapResource(resource_upload, buffer_size);
        auto addr = resource_default[resource_index]->GetGPUVirtualAddress();
        const auto& buffer_view = model.bufferViews[accessor.bufferView];
        memcpy(dst, &model.buffers[buffer_view.buffer].data[accessor.byteOffset + buffer_view.byteOffset], buffer_size);
        set_mesh_info(addr, buffer_size, accessor.type, accessor.componentType, accessor.count, scene_data.model_submesh_index[i][j]);
        UnmapResource(resource_upload);
      }
      ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, resource_default[resource_index], resource_transfer);
      resource_index++;
    }
  }
  return resource_index;
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
        logtrace("submesh:{}-{}({}), variation:{:x} primitive.material:{}=submesh_material_index:{}", i, j, mesh_index, scene_data->submesh_material_variation_hash[mesh_index], primitive.material, scene_data->submesh_material_index[mesh_index]);
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
    SetTransform(model, node, DirectX::SimpleMath::Matrix::Identity.m, scene_data->transform_offset, transform_offset_index, static_cast<matrix*>(transform_buffer));
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
auto GetRawBufferDesc(const uint32_t num, const uint32_t stride) {
  return D3D12_SHADER_RESOURCE_VIEW_DESC {
    .Format = DXGI_FORMAT_R32_TYPELESS,
    .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    .Buffer = {
      .FirstElement = 0,
      .NumElements = num * stride / 4, // 4=R32
      .StructureByteStride = 0,
      .Flags = D3D12_BUFFER_SRV_FLAG_RAW,
    }
  };
}
auto GetArrayBufferDesc(const uint32_t element_num, const uint32_t stride_size) {
  return D3D12_SHADER_RESOURCE_VIEW_DESC {
    .Format = DXGI_FORMAT_UNKNOWN,
    .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    .Buffer = {
      .FirstElement = 0,
      .NumElements = element_num,
      .StructureByteStride = stride_size,
      .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
    }
  };
}
auto GetConstantBufferDesc(const D3D12_GPU_VIRTUAL_ADDRESS&  addr, const uint32_t size) {
  return D3D12_CONSTANT_BUFFER_VIEW_DESC{
    .BufferLocation = addr,
    .SizeInBytes = size,
  };
}
auto GetMaterialNum(const tinygltf::Model& model) {
  return GetUint32(model.materials.size());
}
auto SetMaterialValues(const tinygltf::Model& model, const uint32_t frame_index, SceneData* scene_data, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, const D3D12_CPU_DESCRIPTOR_HANDLE descriptor_heap_head_addr, const uint32_t descriptor_handle_increment_size, const uint32_t descriptor_index_offset, ResourceTransfer* resource_transfer) {
  shader::MaterialCommonSettings material_common_settings{}; // TODO fill
  const auto material_num = GetMaterialNum(model);
  auto tmp_allocator = GetTemporalMemoryAllocator();
  auto albedo_indices = AllocateArray<shader::AlbedoIndexList>(&tmp_allocator, material_num);
  auto material_colors = AllocateArray<std::array<float, 4>>(&tmp_allocator, material_num);
  auto alpha_cutoffs = AllocateArray<float>(&tmp_allocator, material_num);
  uint32_t next_color_index = 0;
  uint32_t next_alpha_cutoff_index = 0;
  const uint32_t texture_offset = 1; // white texture at texture array head
  const uint32_t sampler_offset = 1; // trilinear texture at sampler array head
  for (uint32_t i = 0; i < material_num; i++) {
    const auto& src_material = model.materials[i];
    auto& albedo_index = albedo_indices[i];
    {
      // albedo factor
      auto albedo_factor_index = FindAlbedoFactorIndex(material_colors, next_color_index, src_material.pbrMetallicRoughness.baseColorFactor);
      if (albedo_factor_index == ~0U) {
        SetColor(src_material.pbrMetallicRoughness.baseColorFactor.data(), material_colors[next_color_index].data());
        albedo_factor_index = next_color_index;
        next_color_index++;
        logtrace("material:{} albedo index:{} color:[{},{},{}]", i, albedo_factor_index, material_colors[next_color_index][0], material_colors[next_color_index][1], material_colors[next_color_index][2]);
      }
      albedo_index.factor = albedo_factor_index;
      logtrace("material:{} albedo:{}", i, albedo_factor_index);
    }
    {
      // albedo tex, sampler
      albedo_index.tex = 0;
      albedo_index.sampler = 0;
      const auto& texture_index = src_material.pbrMetallicRoughness.baseColorTexture.index;
      if (texture_index != -1) {
        const auto& texture_info = model.textures[texture_index];
        if (texture_info.source != -1) {
          albedo_index.tex = texture_info.source + texture_offset;
        }
        if (texture_info.sampler != -1) {
          albedo_index.sampler = texture_info.sampler + sampler_offset;
        }
        logtrace("material:{} texture:{} sampler:{}", i, texture_info.source, texture_info.sampler);
      }
    }
    {
      // alpha cutoff
      auto alpha_cutoff_index = FindFloat(alpha_cutoffs, next_alpha_cutoff_index, src_material.alphaCutoff);
      if (alpha_cutoff_index > next_alpha_cutoff_index) {
        alpha_cutoffs[next_alpha_cutoff_index] = static_cast<float>(src_material.alphaCutoff);
        alpha_cutoff_index = next_alpha_cutoff_index;
        next_alpha_cutoff_index++;
      }
      assert(src_material.alphaMode.compare("MASK") != 0 || alpha_cutoff_index < material_num);
      albedo_index.alpha_cutoff = alpha_cutoff_index;
      logtrace("material:{} alpha_cutoff:{}", i, alpha_cutoff_index);
    }
  }
  auto handle_index = descriptor_index_offset;
  {
    const auto size_of_material_common_settings = GetUint32(sizeof(material_common_settings));
    const auto size_of_material_common_settings_aligned = AlignAddress(size_of_material_common_settings, 256); // cbv size must be multiple of 256
    auto [resource_upload, resource_default] = PrepareSingleBufferTransfer(size_of_material_common_settings_aligned,
                                                                           "material_common_settings_U", "material_common_settings",
                                                                           frame_index, scene_data, buffer_allocator, resource_transfer);
    memcpy(MapResource(resource_upload, size_of_material_common_settings), &material_common_settings, size_of_material_common_settings);
    UnmapResource(resource_upload);
    const auto resource_desc = GetConstantBufferDesc(resource_default->GetGPUVirtualAddress(), size_of_material_common_settings_aligned);
    const auto handle = GetDescriptorHandle(descriptor_heap_head_addr, descriptor_handle_increment_size, handle_index);
    device->CreateConstantBufferView(&resource_desc, handle);
    scene_data->cpu_handles[kSceneDescriptorMaterialCommonSettings].ptr = handle.ptr;
    logtrace("material common settings handle index:{} resource:{:x} handle:{:x}", handle_index, reinterpret_cast<std::uintptr_t>(resource_default), scene_data->cpu_handles[kSceneDescriptorMaterialCommonSettings].ptr);
    handle_index++;
  }
  {
    const auto albedo_index_stride = GetUint32(sizeof(shader::AlbedoIndexList));
    const auto size = albedo_index_stride * material_num;
    auto [resource_upload, resource_default] = PrepareSingleBufferTransfer(size, "material_index_list_U", "material_index_list",
                                                                           frame_index, scene_data, buffer_allocator, resource_transfer);
    memcpy(MapResource(resource_upload, size), albedo_indices, size);
    UnmapResource(resource_upload);
    const auto resource_desc = GetRawBufferDesc(material_num, albedo_index_stride);
    const auto handle = GetDescriptorHandle(descriptor_heap_head_addr, descriptor_handle_increment_size, handle_index);
    device->CreateShaderResourceView(resource_default, &resource_desc, handle);
    scene_data->cpu_handles[kSceneDescriptorMaterialIndices].ptr = handle.ptr;
    logtrace("material index handle index:{} resource:{:x} handle:{:x}", handle_index, reinterpret_cast<std::uintptr_t>(resource_default), scene_data->cpu_handles[kSceneDescriptorMaterialIndices].ptr);
    handle_index++;
  }
  {
    const auto color_stride_size = GetUint32(sizeof(float)) * 4;
    const auto size = color_stride_size * next_color_index;
    auto [resource_upload, resource_default] = PrepareSingleBufferTransfer(size, "color_U", "color", frame_index, scene_data, buffer_allocator, resource_transfer);
    auto color_dst = MapResource(resource_upload, size);
    for (uint32_t i = 0; i < next_color_index; i++) {
      memcpy(color_dst, material_colors[i].data(), sizeof(float) * 4);
      color_dst = SucceedPtr(color_dst, color_stride_size);
    }
    UnmapResource(resource_upload);
    const auto resource_desc = GetArrayBufferDesc(next_color_index, color_stride_size);
    const auto handle = GetDescriptorHandle(descriptor_heap_head_addr, descriptor_handle_increment_size, handle_index);
    device->CreateShaderResourceView(resource_default, &resource_desc, handle);
    scene_data->cpu_handles[kSceneDescriptorColors].ptr = handle.ptr;
    logtrace("color handle index:{} resource:{:x} handle:{:x}", handle_index, reinterpret_cast<std::uintptr_t>(resource_default), scene_data->cpu_handles[kSceneDescriptorColors].ptr);
    handle_index++;
  }
  {
    const auto alpha_cutoff_stride_size = GetUint32(sizeof(float));
    const auto size = alpha_cutoff_stride_size * std::max(next_alpha_cutoff_index, 1U);
    auto [resource_upload, resource_default] = PrepareSingleBufferTransfer(size, "alpha_cutoffs_U", "alpha_cutoffs", frame_index, scene_data, buffer_allocator, resource_transfer);
    if (next_alpha_cutoff_index > 0) {
      memcpy(MapResource(resource_upload, size), alpha_cutoffs, size);
      UnmapResource(resource_upload);
    }
    const auto resource_desc = GetArrayBufferDesc(std::max(next_alpha_cutoff_index, 1U), alpha_cutoff_stride_size);
    const auto handle = GetDescriptorHandle(descriptor_heap_head_addr, descriptor_handle_increment_size, handle_index);
    device->CreateShaderResourceView(resource_default, &resource_desc, handle);
    scene_data->cpu_handles[kSceneDescriptorAlphaCutoff].ptr = handle.ptr;
    logtrace("alpha cutoff handle index:{} resource:{:x} handle:{:x}", handle_index, reinterpret_cast<std::uintptr_t>(resource_default), scene_data->cpu_handles[kSceneDescriptorAlphaCutoff].ptr);
    handle_index++;
  }
  return handle_index - descriptor_index_offset;
}
auto GetTextureNum(const tinygltf::Model& model) {
  return GetUint32(model.images.size()) + 1;
}
D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDescTexture2D(const D3D12_RESOURCE_DESC1& desc) {
  assert(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D);
  return {
    .Format = desc.Format,
    .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    .Texture2D = {
      .MostDetailedMip = 0,
      .MipLevels = desc.MipLevels,
      .PlaneSlice = 0,
      .ResourceMinLODClamp = 0.0f,
    },
  };
}
auto PrepareSceneTextureUpload(const tinygltf::Model& model, const char* const gltf_path, const uint32_t frame_index, SceneData* scene_data, D3d12Device* device, const D3D12_CPU_DESCRIPTOR_HANDLE descriptor_heap_head_addr, const uint32_t descriptor_handle_increment_size, const uint32_t descriptor_index_offset, ResourceTransfer* resource_transfer, D3D12MA::Allocator* buffer_allocator) {
  const auto texture_num = scene_data->texture_num;
  auto handle_index = descriptor_index_offset;
  for (uint32_t i = 0; i < texture_num; i++) {
    auto tmp_allocator = GetTemporalMemoryAllocator();
    std::filesystem::path path;
    if (i == 0) {
      // default texture
      path = "textures/white.dds";
    } else {
      path = gltf_path;
      path.replace_filename(model.images[i-1].uri);
      path.replace_extension(".dds");
    }
    auto texture_creation_info = GatherTextureCreationInfo(device, path.wstring().c_str(), &tmp_allocator);
    if (texture_creation_info.resource_desc.Width == 0) {
      logwarn("invalid texture {}", path.string());
      continue;
    }
    if (texture_creation_info.resource_desc.Flags) {
      logwarn("texture flag:{}", texture_creation_info.resource_desc.Flags);
    }
    ID3D12Resource* resource_upload{nullptr};
    D3D12MA::Allocation* allocation_upload{nullptr};
    CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ,
                 GetBufferDesc(texture_creation_info.total_size_in_bytes),
                 nullptr, buffer_allocator,
                 &allocation_upload, &resource_upload);
    const auto default_buffer_index = scene_data->resource_num;
    scene_data->resource_num++;
    CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON,
                 texture_creation_info.resource_desc,
                 nullptr, buffer_allocator,
                 &scene_data->allocations[default_buffer_index], &scene_data->resources[default_buffer_index]);
    SetD3d12Name(resource_upload, path.string() + "U");
    SetD3d12Name(scene_data->resources[default_buffer_index], path.string());
    FillUploadResource(texture_creation_info, resource_upload);
    if (!ReserveResourceTransfer(frame_index, texture_creation_info.subresource_num, texture_creation_info.layout,
                                 resource_upload, allocation_upload,
                                 scene_data->resources[default_buffer_index], resource_transfer)) {
      logwarn("texture ReserveResourceTransfer failed. {} {}", path.string(), i);
    }
    const auto handle = GetDescriptorHandle(descriptor_heap_head_addr, descriptor_handle_increment_size, handle_index);
    auto view_desc = GetSrvDescTexture2D(texture_creation_info.resource_desc);
    device->CreateShaderResourceView(scene_data->resources[default_buffer_index], &view_desc, handle);
    logtrace("texture index:{} {} handle_index:{} resource:{:x} ptr:{:x}", i, path.string(), handle_index, reinterpret_cast<std::uintptr_t>(scene_data->resources[default_buffer_index]), handle.ptr);
    handle_index++;
  }
}
auto ConvertToD3d12Filter(const int32_t minFilter, const int32_t magFilter) {
  // https://www.khronos.org/opengl/wiki/Sampler_Object
  switch (minFilter) {
    case -1: {
      switch (magFilter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST: {
          return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        }
        default:
        case -1:
        case TINYGLTF_TEXTURE_FILTER_LINEAR: {
          return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        }
      }
      break;
    }
    case TINYGLTF_TEXTURE_FILTER_NEAREST: {
      switch (magFilter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST: {
          return D3D12_FILTER_MIN_MAG_MIP_POINT;
        }
        default:
        case -1:
        case TINYGLTF_TEXTURE_FILTER_LINEAR: {
          return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        }
      }
      break;
    }
    case TINYGLTF_TEXTURE_FILTER_LINEAR: {
      switch (magFilter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST: {
          return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        }
        default:
        case -1:
        case TINYGLTF_TEXTURE_FILTER_LINEAR: {
          return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        }
      }
      break;
    }
    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST: {
      switch (magFilter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST: {
          return D3D12_FILTER_MIN_MAG_MIP_POINT;
        }
        default:
        case -1:
        case TINYGLTF_TEXTURE_FILTER_LINEAR: {
          return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        }
      }
      break;
    }
    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST: {
      switch (magFilter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST: {
          return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
        }
        default:
        case -1:
        case TINYGLTF_TEXTURE_FILTER_LINEAR: {
          return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        }
      }
      break;
    }
    case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR: {
      switch (magFilter) {
        default:
        case -1:
        case TINYGLTF_TEXTURE_FILTER_NEAREST: {
          return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
        }
        case TINYGLTF_TEXTURE_FILTER_LINEAR: {
          logwarn("invalid sampler filter type combination. {} {}", minFilter, magFilter);
          return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        }
      }
      break;
    }
    case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR: {
      switch (magFilter) {
        case TINYGLTF_TEXTURE_FILTER_NEAREST: {
          return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
        }
        default:
        case -1:
        case TINYGLTF_TEXTURE_FILTER_LINEAR: {
          return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        }
      }
      break;
    }
    default: {
      logwarn("no filter selected {} {}", minFilter, magFilter);
      return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    }
  }
}
auto ConvertToD3d12TextureAddressMode(const int32_t wrap) {
  switch (wrap) {
    case TINYGLTF_TEXTURE_WRAP_REPEAT:          return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:   return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
  }
  return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
}
auto CreateSamplers(const std::vector<tinygltf::Sampler>& samplers, D3d12Device* device, const D3D12_CPU_DESCRIPTOR_HANDLE descriptor_heap_head_addr, const uint32_t descriptor_handle_increment_size) {
  const auto sampler_num = GetUint32(samplers.size()) + 1;
  // not very precise conversion from gltf.
  D3D12_SAMPLER_DESC sampler_desc{};
  sampler_desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler_desc.MipLODBias = 0.0f;
  sampler_desc.MaxAnisotropy = 16; // there is no anisotropy in standard gltf yet.
  sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
  sampler_desc.MinLOD = 0.0f;
  sampler_desc.MaxLOD = 65535.0f;
  for (uint32_t i = 0; i < sampler_num; i++) {
    if (i == 0) {
      // default trilinear sampler
      sampler_desc.Filter   = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
      sampler_desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
      sampler_desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    } else {
      sampler_desc.Filter   = ConvertToD3d12Filter(samplers[i-1].minFilter, samplers[i-1].magFilter);
      sampler_desc.AddressU = ConvertToD3d12TextureAddressMode(samplers[i-1].wrapS);
      sampler_desc.AddressV = ConvertToD3d12TextureAddressMode(samplers[i-1].wrapT);
    }
    const auto handle = GetDescriptorHandle(descriptor_heap_head_addr, descriptor_handle_increment_size, i);
    device->CreateSampler(&sampler_desc, handle);
  }
}
auto ParseTinyGltfScene(const tinygltf::Model& model, const char* const gltf_path, MemoryAllocationJanitor* allocator, const uint32_t frame_index, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
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
  scene_data.texture_num = GetTextureNum(model);
  const auto resource_num = mesh_num * (kVertexBufferTypeNum + 1/*index buffer*/) + scene_data.texture_num + kSceneDescriptorHandleTypeNum - 2/*texture,sampler*/;
  scene_data.resource_num = 0;
  scene_data.resources = AllocateArray<ID3D12Resource*>(allocator, resource_num);
  scene_data.allocations = AllocateArray<D3D12MA::Allocation*>(allocator, resource_num);
  scene_data.submesh_index_buffer_len = AllocateArray<uint32_t>(allocator, mesh_num);
  scene_data.submesh_index_buffer_view = AllocateArray<D3D12_INDEX_BUFFER_VIEW>(allocator, mesh_num);
  scene_data.submesh_vertex_buffer_view = AllocateArray<D3D12_VERTEX_BUFFER_VIEW>(allocator, mesh_num * kVertexBufferTypeNum);
  for (uint32_t i = 0; i < kVertexBufferTypeNum; i++) {
    scene_data.submesh_vertex_buffer_view_index[i] = AllocateArray<uint32_t>(allocator, mesh_num);
  }
  {
    const auto default_buffer_index = scene_data.resource_num;
    const auto num = SetMeshBuffers<kIndiceMode>(model, "", scene_data, frame_index,
                                                 &scene_data.resources[default_buffer_index], &scene_data.allocations[default_buffer_index],
                                                 "idx", buffer_allocator, resource_transfer,
                                                 [submesh_index_buffer_len = scene_data.submesh_index_buffer_len,
                                                  submesh_index_buffer_view = scene_data.submesh_index_buffer_view]
                                                 (const uint64_t& addr, const uint32_t size,
                                                  const uint32_t tinygltf_type, const uint32_t tinygltf_component_type, const uint64_t& accessor_count,
                                                  const uint32_t index) {
                                                   submesh_index_buffer_len[index] = GetUint32(accessor_count);
                                                   submesh_index_buffer_view[index] = {
                                                     .BufferLocation = addr,
                                                     .SizeInBytes    = size,
                                                     .Format         = GetIndexBufferDxgiFormat(tinygltf_type, tinygltf_component_type),
                                                   };
                                                   logtrace("index buffer index:{} len:{} addr:{:x} size:{} format:{}", index, submesh_index_buffer_len[index], addr, size, GetIndexBufferDxgiFormat(tinygltf_type, tinygltf_component_type));
                                                 });
    assert(num == mesh_num);
    scene_data.resource_num += num;
  }
  const std::string attributes[] = {"POSITION", "NORMAL", "TANGENT", "TEXCOORD_0"};
  static_assert(std::size(attributes) == kVertexBufferTypeNum);
  uint32_t vertex_buffer_index_offset = 0;
  for (uint32_t i = 0; i < kVertexBufferTypeNum; i++) {
    const auto default_buffer_index = scene_data.resource_num;
    const auto num = SetMeshBuffers<kVertexMode>(model, attributes[i], scene_data, frame_index,
                                                 &scene_data.resources[default_buffer_index], &scene_data.allocations[default_buffer_index],
                                                 attributes[i], buffer_allocator, resource_transfer,
                                                 [submesh_vertex_buffer_view_index = scene_data.submesh_vertex_buffer_view_index[i],
                                                  submesh_vertex_buffer_view = scene_data.submesh_vertex_buffer_view,
                                                  vertex_buffer_index_offset = vertex_buffer_index_offset]
                                                 (const uint64_t& addr, const uint32_t size,
                                                  const uint32_t tinygltf_type, const uint32_t tinygltf_component_type, [[maybe_unused]]const uint64_t& accessor_count,
                                                  const uint32_t index) {
                                                   submesh_vertex_buffer_view_index[index] = index + vertex_buffer_index_offset;
                                                   submesh_vertex_buffer_view[index + vertex_buffer_index_offset] = {
                                                     .BufferLocation = addr,
                                                     .SizeInBytes    = size,
                                                     .StrideInBytes  = GetVertexBufferStrideInBytes(tinygltf_type, tinygltf_component_type),
                                                   };
                                                   logtrace("vertex buffer index:{} addr:{:x} size:{} strid:{}", index, addr, size, submesh_vertex_buffer_view[index].StrideInBytes);
                                                 });
    assert(num <= mesh_num);
    scene_data.resource_num += num;
    vertex_buffer_index_offset += num;
  }
  scene_data.submesh_material_variation_hash = AllocateArray<StrHash>(allocator, mesh_num);
  scene_data.submesh_material_index = AllocateArray<uint32_t>(allocator, mesh_num);
  SetSubmeshMaterials(model, &scene_data);
  for (const auto& node : model.scenes[0].nodes) {
    CountModelInstanceNum(model, node, scene_data.model_instance_num);
  }
  const auto cbv_srv_uav_num = kSceneDescriptorHandleTypeNum - 2/*texture,sampler*/ + scene_data.texture_num;
  const auto handle_increment_size_cbv_srv_uav = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  scene_data.descriptor_heap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, cbv_srv_uav_num);
  SetD3d12Name(scene_data.descriptor_heap, "scene_descriptor_heap");
  const auto descriptor_heap_head_addr = scene_data.descriptor_heap->GetCPUDescriptorHandleForHeapStart();
  uint32_t used_descriptor_num = 0;
  {
    const auto stride_size = GetUint32(sizeof(matrix));
    auto [resource_upload, resource_default] = PrepareSingleBufferTransfer(stride_size * mesh_num, "transform_U", "transform", frame_index, &scene_data, buffer_allocator, resource_transfer);
    SetTransformValues(model, &scene_data, mesh_num, resource_upload);
    const auto resource_desc = GetRawBufferDesc(mesh_num, stride_size);
    const auto handle = GetDescriptorHandle(descriptor_heap_head_addr, handle_increment_size_cbv_srv_uav, used_descriptor_num);
    device->CreateShaderResourceView(resource_default, &resource_desc, handle);
    scene_data.cpu_handles[kSceneDescriptorTransform].ptr = handle.ptr;
    used_descriptor_num++;
  }
  {
    used_descriptor_num += SetMaterialValues(model, frame_index, &scene_data, device, buffer_allocator, descriptor_heap_head_addr, handle_increment_size_cbv_srv_uav, used_descriptor_num, resource_transfer);
    scene_data.cpu_handles[kSceneDescriptorTexture].ptr = GetDescriptorHandle(descriptor_heap_head_addr, handle_increment_size_cbv_srv_uav, used_descriptor_num).ptr;
    PrepareSceneTextureUpload(model, gltf_path, frame_index, &scene_data, device, descriptor_heap_head_addr, handle_increment_size_cbv_srv_uav, used_descriptor_num, resource_transfer, buffer_allocator);
  }
  {
    scene_data.sampler_num = GetUint32(model.samplers.size()) + 1;
    scene_data.sampler_descriptor_heap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, scene_data.sampler_num);
    SetD3d12Name(scene_data.sampler_descriptor_heap, "scene_sampler_descriptor_heap");
    scene_data.cpu_handles[kSceneDescriptorSampler].ptr = scene_data.sampler_descriptor_heap->GetCPUDescriptorHandleForHeapStart().ptr;
    const auto handle_increment_size_sampler = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    CreateSamplers(model.samplers, device, scene_data.cpu_handles[kSceneDescriptorSampler], handle_increment_size_sampler);
  }
  return scene_data;
}
} // namespace anonymous
SceneData GetSceneFromTinyGltf(const char* const filename, const uint32_t frame_index, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, MemoryAllocationJanitor* allocator, ResourceTransfer* resource_transfer) {
  loginfo("loading {}", filename);
  tinygltf::Model model;
  if (!GetTinyGltfModel(filename, &model)) {
    logerror("gltf load failed. {}", filename);
    return {};
  }
  return ParseTinyGltfScene(model, filename, allocator, frame_index, device, buffer_allocator, resource_transfer);
}
void ReleaseSceneData(SceneData* scene_data) {
  for (uint32_t i = 0; i < scene_data->resource_num; i++) {
    scene_data->resources[i]->Release();
    scene_data->allocations[i]->Release();
  }
  scene_data->descriptor_heap->Release();
  scene_data->sampler_descriptor_heap->Release();
}
namespace {
static const char* scene_buffer_names[] = {
  "transforms",
  "material_indices",
  "material_common_settings",
  "colors",
  "alpha_cutoffs",
  "textures",
  "samplers",
};
}
bool IsSceneBuffer(const char* const name) {
  static_assert(std::size(scene_buffer_names) == kSceneDescriptorHandleTypeNum);
  for (uint32_t i = 0; i < kSceneDescriptorHandleTypeNum; i++) {
    if (strcmp(name, scene_buffer_names[i]) == 0) {
      return true;
    }
  }
  return false;
}
uint32_t EncodeSceneBufferIndex(const char* const name) {
  uint32_t index = 0;
  for (uint32_t i = 0; i < kSceneDescriptorHandleTypeNum; i++) {
    if (strcmp(name, scene_buffer_names[i]) == 0) {
      index = i;
      break;
    }
  }
  return 0xFFFF0000 | index;
}
bool IsSceneBuffer(const uint32_t buffer_index) {
  return (0xFFFF0000 & buffer_index) == 0xFFFF0000;
}
uint32_t GetDecodedSceneBufferIndex(const uint32_t buffer_index) {
  return (~0xFFFF0000) & buffer_index;
}
} // namespace illuminate
#include "doctest/doctest.h"
#include "d3d12_dxgi_core.h"
#include "d3d12_device.h"
#include "d3d12_gpu_buffer_allocator.h"
TEST_CASE("gltf scene") { // NOLINT
  using namespace illuminate; // NOLINT
  DxgiCore dxgi_core;
  dxgi_core.Init(); // NOLINT
  Device device;
  device.Init(dxgi_core.GetAdapter()); // NOLINT
  auto buffer_allocator = GetBufferAllocator(dxgi_core.GetAdapter(), device.Get());
  auto tmp_allocator = GetTemporalMemoryAllocator();
  const uint32_t frame_num = 2;
  uint32_t frame_index = 0;
  auto resource_transfer = PrepareResourceTransferer(frame_num, 1024, 11);
  auto scene_data = GetSceneFromTinyGltf(TEST_MODEL_PATH, frame_index, device.Get(), buffer_allocator, &tmp_allocator, &resource_transfer);
  ClearResourceTransfer(2, &resource_transfer);
  ReleaseSceneData(&scene_data);
  buffer_allocator->Release();
  device.Term();
  dxgi_core.Term();
  gSystemMemoryAllocator->Reset();
}
