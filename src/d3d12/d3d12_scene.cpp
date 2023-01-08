#include "d3d12_scene.h"
#include <filesystem>
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
void SetTransform(const tinygltf::Model& model, const uint32_t node_index, const gfxminimath::matrix& parent_transform, const uint32_t* transform_offset, uint32_t* transform_offset_index, gfxminimath::matrix* transform_buffer) {
  using namespace gfxminimath;
  auto parent = parent_transform;
  auto transform = matrix::identity();
  const auto& node = model.nodes[node_index];
  if (!node.matrix.empty()) {
    float m[16]{};
    for (uint32_t i = 0; i < 3; i++) {
      m[i] = static_cast<float>(node.matrix[i]);
    }
    set_matrix_with_row_major_array(m, &transform);
  } else {
    if (!node.scale.empty()) {
      float scale[3]{};
      for (uint32_t i = 0; i < 3; i++) {
        scale[i] = static_cast<float>(node.scale[i]);
      }
      transform *= matrix::scale(scale);
    }
    if (!node.rotation.empty()) {
      float rotation[4]{};
      for (uint32_t i = 0; i < 4; i++) {
        rotation[i] = static_cast<float>(node.rotation[i]);
      }
      transform *= matrix::rotation(rotation);
    }
    if (!node.translation.empty()) {
      float translation[3]{};
      for (uint32_t i = 0; i < 3; i++) {
        translation[i] = static_cast<float>(node.translation[i]);
      }
      transform *= matrix::translation(translation);
    }
    transform *= parent;
  }
  if (node.mesh != -1) {
    const auto index = transform_offset[node.mesh] + transform_offset_index[node.mesh];
    transform_buffer[index] = transform;
    transform_offset_index[node.mesh]++;
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
            for (uint32_t k = 0; k < accessor.count; k++) {
              const float src[4] = {1.0f,0.0f,0.0f,1.0f,}; // assuming tangent
              const auto stride_size = (accessor.componentType == TINYGLTF_TYPE_VEC3) ? 12U : 16U;
              memcpy(dst, src, stride_size);
              dst = SucceedPtr(dst, stride_size);
            }
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
auto GetTransformList(const tinygltf::Model& model, SceneData* scene_data, const uint32_t mesh_num) {
  uint32_t transform_element_num = 0;
  for (uint32_t i = 0; i < scene_data->model_num; i++) {
    scene_data->transform_offset[i] = transform_element_num;
    transform_element_num += scene_data->model_instance_num[i];
  }
  auto transform_offset_index = AllocateArrayFrame<uint32_t>(scene_data->model_num);
  for (uint32_t i = 0; i < scene_data->model_num; i++) {
    transform_offset_index[i] = 0;
  }
  auto transform_list = AllocateArrayFrame<gfxminimath::matrix>(mesh_num);
  for (const auto& node : model.scenes[0].nodes) {
    SetTransform(model, node, gfxminimath::matrix::identity(), scene_data->transform_offset, transform_offset_index, transform_list);
  }
  return transform_list;
}
auto ConvertMatrixToFloatArray(const gfxminimath::matrix* m, const uint32_t mesh_num) {
  auto array = AllocateArrayFrame<float>(mesh_num * 16);
  for (uint32_t i = 0; i < mesh_num; i++) {
    to_array_column_major(m[i], &array[i * 16]);
  }
  return array;
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
auto GetRawBufferDesc(const uint32_t size_in_bytes) {
  return D3D12_SHADER_RESOURCE_VIEW_DESC {
    .Format = DXGI_FORMAT_R32_TYPELESS,
    .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    .Buffer = {
      .FirstElement = 0,
      .NumElements = size_in_bytes / 4, // 4=R32
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
auto SetTextureSamplerIndex(const std::vector<tinygltf::Texture>& textures, const uint32_t texture_index, const uint32_t tex_default, const uint32_t sampler_default, const uint32_t texture_offset, const uint32_t sampler_offset, uint32_t* tex, uint32_t* sampler) {
  if (texture_index == -1) {
    *tex = tex_default;
    *sampler = sampler_default;
    return;
  }
  const auto& texture_info = textures[texture_index];
  *tex = (texture_info.source == -1) ? tex_default : texture_info.source + texture_offset;
  *sampler = (texture_info.source == -1) ? sampler_default : texture_info.sampler + sampler_offset;
}
static const uint32_t kWhiteTextureIndex = 0;
static const uint32_t kBlackTextureIndex = 1;
static const uint32_t kDefaultNormalTextureIndex = 2;
static const uint32_t kDefaultTextureNum = 3;
auto SetMaterialValues(const tinygltf::Model& model, const uint32_t frame_index, SceneData* scene_data, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, const D3D12_CPU_DESCRIPTOR_HANDLE descriptor_heap_head_addr, const uint32_t descriptor_handle_increment_size, const uint32_t descriptor_index_offset, ResourceTransfer* resource_transfer) {
  const auto material_num = GetMaterialNum(model);
  shader::MaterialCommonSettings material_common_settings{};
  material_common_settings.misc_offset = static_cast<uint32_t>(sizeof(shader::AlbedoInfo)) * material_num;
  const auto material_index_list_size = material_common_settings.misc_offset + static_cast<uint32_t>(sizeof(shader::MaterialMiscInfo)) * material_num;
  auto material_index_list = AllocateFrame(material_index_list_size);
  auto albedo_infos = static_cast<shader::AlbedoInfo*>(material_index_list);
  auto misc_infos = static_cast<shader::MaterialMiscInfo*>(SucceedPtr(material_index_list, material_common_settings.misc_offset));
  const uint32_t texture_offset = kDefaultTextureNum; // white, black & default normal texture at texture array head
  const uint32_t sampler_offset = 1; // trilinear texture at sampler array head
  for (uint32_t i = 0; i < material_num; i++) {
    const auto& src_material = model.materials[i];
    auto& albedo_info = albedo_infos[i];
    albedo_info.factor.x = static_cast<float>(src_material.pbrMetallicRoughness.baseColorFactor[0]);
    albedo_info.factor.y = static_cast<float>(src_material.pbrMetallicRoughness.baseColorFactor[1]);
    albedo_info.factor.z = static_cast<float>(src_material.pbrMetallicRoughness.baseColorFactor[2]);
    albedo_info.factor.w = static_cast<float>(src_material.pbrMetallicRoughness.baseColorFactor[3]);
    SetTextureSamplerIndex(model.textures, src_material.pbrMetallicRoughness.baseColorTexture.index, kWhiteTextureIndex, 0, texture_offset, sampler_offset, &albedo_info.tex, &albedo_info.sampler);
    albedo_info.alpha_cutoff = static_cast<float>(src_material.alphaCutoff);
    auto& misc_info = misc_infos[i];
    misc_info.metallic_factor = static_cast<float>(src_material.pbrMetallicRoughness.metallicFactor);
    misc_info.roughness_factor = static_cast<float>(src_material.pbrMetallicRoughness.roughnessFactor);
    SetTextureSamplerIndex(model.textures, src_material.pbrMetallicRoughness.metallicRoughnessTexture.index, kWhiteTextureIndex, 0, texture_offset, sampler_offset, &misc_info.metallic_roughness_tex, &misc_info.metallic_roughness_sampler);
    misc_info.normal_scale = static_cast<float>(src_material.normalTexture.scale);
    SetTextureSamplerIndex(model.textures, src_material.normalTexture.index, kDefaultNormalTextureIndex, 0, texture_offset, sampler_offset, &misc_info.normal_tex, &misc_info.normal_sampler);
    misc_info.occlusion_strength = static_cast<float>(src_material.occlusionTexture.strength);
    SetTextureSamplerIndex(model.textures, src_material.occlusionTexture.index, kWhiteTextureIndex, 0, texture_offset, sampler_offset, &misc_info.occlusion_tex, &misc_info.occlusion_sampler);
    SetTextureSamplerIndex(model.textures, src_material.emissiveTexture.index, kBlackTextureIndex, 0, texture_offset, sampler_offset, &misc_info.emissive_tex, &misc_info.emissive_sampler);
    misc_info.emissive_factor.x = static_cast<float>(src_material.emissiveFactor[0]);
    misc_info.emissive_factor.y = static_cast<float>(src_material.emissiveFactor[1]);
    misc_info.emissive_factor.z = static_cast<float>(src_material.emissiveFactor[2]);
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
    auto [resource_upload, resource_default] = PrepareSingleBufferTransfer(material_index_list_size, "material_index_list_U", "material_index_list",
                                                                           frame_index, scene_data, buffer_allocator, resource_transfer);
    memcpy(MapResource(resource_upload, material_index_list_size), material_index_list, material_index_list_size);
    UnmapResource(resource_upload);
    const auto resource_desc = GetRawBufferDesc(material_index_list_size);
    const auto handle = GetDescriptorHandle(descriptor_heap_head_addr, descriptor_handle_increment_size, handle_index);
    device->CreateShaderResourceView(resource_default, &resource_desc, handle);
    scene_data->cpu_handles[kSceneDescriptorMaterialIndices].ptr = handle.ptr;
    logtrace("material index handle index:{} resource:{:x} handle:{:x}", handle_index, reinterpret_cast<std::uintptr_t>(resource_default), scene_data->cpu_handles[kSceneDescriptorMaterialIndices].ptr);
    handle_index++;
  }
  return handle_index - descriptor_index_offset;
}
auto GetTextureNum(const tinygltf::Model& model) {
  return GetUint32(model.images.size()) + kDefaultTextureNum;
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
    std::filesystem::path path;
    if (i == kWhiteTextureIndex) {
      path = "textures/white.dds";
    } else if (i == kBlackTextureIndex) {
      path = "textures/black.dds";
    } else if (i == kDefaultNormalTextureIndex) {
      path = "textures/normal.dds";
    } else {
      path = gltf_path;
      path.replace_filename(model.images[i-kDefaultTextureNum].uri);
      path.replace_extension(".dds");
    }
    auto texture_creation_info = GatherTextureCreationInfo(device, path.wstring().c_str());
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
auto ParseTinyGltfScene(const tinygltf::Model& model, const char* const gltf_path, const uint32_t frame_index, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
  SceneData scene_data{};
  scene_data.model_num = GetUint32(model.meshes.size());
  scene_data.model_instance_num = AllocateArrayScene<uint32_t>(scene_data.model_num);
  scene_data.model_submesh_num = AllocateArrayScene<uint32_t>(scene_data.model_num);
  scene_data.model_submesh_index = AllocateArrayScene<uint32_t*>(scene_data.model_num);
  scene_data.transform_offset = AllocateArrayScene<uint32_t>(scene_data.model_num);
  uint32_t mesh_num = 0;
  for (uint32_t i = 0; i < scene_data.model_num; i++) {
    scene_data.model_instance_num[i] = 0;
    scene_data.model_submesh_num[i] = GetUint32(model.meshes[i].primitives.size());
    scene_data.model_submesh_index[i] = AllocateArrayScene<uint32_t>(scene_data.model_submesh_num[i]);
    for (uint32_t j = 0; j < scene_data.model_submesh_num[i]; j++) {
      scene_data.model_submesh_index[i][j] = mesh_num;
      mesh_num++;
    }
  }
  scene_data.texture_num = GetTextureNum(model);
  const auto resource_num = mesh_num * (kVertexBufferTypeNum + 1/*index buffer*/) + scene_data.texture_num + kSceneDescriptorHandleTypeNum - 2/*texture,sampler*/;
  scene_data.resource_num = 0;
  scene_data.resources = AllocateArrayScene<ID3D12Resource*>(resource_num);
  scene_data.allocations = AllocateArrayScene<D3D12MA::Allocation*>(resource_num);
  scene_data.submesh_index_buffer_len = AllocateArrayScene<uint32_t>(mesh_num);
  scene_data.submesh_index_buffer_view = AllocateArrayScene<D3D12_INDEX_BUFFER_VIEW>(mesh_num);
  scene_data.submesh_vertex_buffer_view = AllocateArrayScene<D3D12_VERTEX_BUFFER_VIEW>(mesh_num * kVertexBufferTypeNum);
  for (uint32_t i = 0; i < kVertexBufferTypeNum; i++) {
    scene_data.submesh_vertex_buffer_view_index[i] = AllocateArrayScene<uint32_t>(mesh_num);
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
  scene_data.submesh_material_variation_hash = AllocateArrayScene<StrHash>(mesh_num);
  scene_data.submesh_material_index = AllocateArrayScene<uint32_t>(mesh_num);
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
    const auto stride_size = GetUint32(sizeof(float) * 16);
    const auto resource_size = stride_size * mesh_num;
    auto [resource_upload, resource_default] = PrepareSingleBufferTransfer(resource_size, "transform_U", "transform", frame_index, &scene_data, buffer_allocator, resource_transfer);
    const auto resource_desc = GetRawBufferDesc(mesh_num, stride_size);
    const auto handle = GetDescriptorHandle(descriptor_heap_head_addr, handle_increment_size_cbv_srv_uav, used_descriptor_num);
    device->CreateShaderResourceView(resource_default, &resource_desc, handle);
    scene_data.cpu_handles[kSceneDescriptorTransform].ptr = handle.ptr;
    auto transform_list = GetTransformList(model, &scene_data, mesh_num);
    auto transform_list_to_array = ConvertMatrixToFloatArray(transform_list, mesh_num);
    auto transform_buffer = MapResource(resource_upload, resource_size);
    memcpy(transform_buffer, transform_list_to_array, resource_size);
    UnmapResource(resource_upload);
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
SceneData GetSceneFromTinyGltf(const char* const filename, const uint32_t frame_index, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
  loginfo("loading {}", filename);
  tinygltf::Model model;
  if (!GetTinyGltfModel(filename, &model)) {
    logerror("gltf load failed. {}", filename);
    return {};
  }
  return ParseTinyGltfScene(model, filename, frame_index, device, buffer_allocator, resource_transfer);
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
static const StrHash scene_buffer_names[] = {
  SID("transforms"),
  SID("material_common_settings"),
  SID("material_indices"),
  SID("textures"),
  SID("samplers"),
};
}
bool IsSceneBufferName(const StrHash& hash) {
  static_assert(std::size(scene_buffer_names) == kSceneDescriptorHandleTypeNum);
  for (uint32_t i = 0; i < kSceneDescriptorHandleTypeNum; i++) {
    if (hash == scene_buffer_names[i]) {
      return true;
    }
  }
  return false;
}
uint32_t EncodeSceneBufferIndex(const StrHash& hash) {
  uint32_t index = 0;
  for (uint32_t i = 0; i < kSceneDescriptorHandleTypeNum; i++) {
    if (hash == scene_buffer_names[i]) {
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
#include "d3d12_gpu_buffer_allocation.h"
TEST_CASE("gltf scene") { // NOLINT
  using namespace illuminate; // NOLINT
  DxgiCore dxgi_core;
  dxgi_core.Init(); // NOLINT
  Device device;
  device.Init(dxgi_core.GetAdapter()); // NOLINT
  auto buffer_allocator = GetBufferAllocator(dxgi_core.GetAdapter(), device.Get());
  const uint32_t frame_num = 2;
  uint32_t frame_index = 0;
  auto resource_transfer = PrepareResourceTransferer(frame_num, 1024, 11);
  auto scene_data = GetSceneFromTinyGltf(TEST_MODEL_PATH, frame_index, device.Get(), buffer_allocator, &resource_transfer);
  ClearResourceTransfer(2, &resource_transfer);
  ReleaseSceneData(&scene_data);
  buffer_allocator->Release();
  device.Term();
  dxgi_core.Term();
  ClearAllAllocations();
}
