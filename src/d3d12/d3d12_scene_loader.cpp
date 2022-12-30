#include "d3d12_scene_loader.h"
#include <fstream>
#include "d3d12_descriptors.h"
#include "d3d12_gpu_buffer_allocator.h"
#include "d3d12_memory_allocators.h"
#include "d3d12_resource_transfer.h"
#include "d3d12_src_common.h"
#include "d3d12_texture_util.h"
#include "shader/include/shader_defines.h"
namespace illuminate {
enum MeshResourceType : uint32_t {
  kMeshResourceTypeTransform,
  kMeshResourceTypeIndex,
  kMeshResourceTypePosition,
  kMeshResourceTypeNormal,
  kMeshResourceTypeTangent,
  kMeshResourceTypeTexcoord,
  kMeshResourceTypeNum, 
};
struct ModelData {
  uint32_t  mesh_num{0};
  uint32_t* instance_num{nullptr};
  uint32_t** transform_index{nullptr};
  uint32_t* index_buffer_len{nullptr};
  uint32_t* index_buffer_offset{nullptr};
  uint32_t* vertex_buffer_offset{nullptr};
  uint32_t* material_setting_index{nullptr};
};
struct MeshViews {
  D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
  D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view[kMeshResourceTypeNum]{};
};
struct MaterialData {
  uint32_t material_num{};
  StrHash* material_variation_hash{};
};
struct MaterialViews {
  DescriptorHeapSet material_descriptor_heap_set{};
  DescriptorHeapSet texture_descriptor_heap_set{};
  DescriptorHeapSet sampler_descriptor_heap_set{};
};
struct ResourceSet {
  uint32_t resource_num{};
  D3D12MA::Allocation** allocations{};
  ID3D12Resource** resources{};
};
struct ModelDataSet {
  ModelData model_data{};
  MeshViews mesh_views{};
  MaterialData material_data{};
  MaterialViews material_views{};
  ResourceSet resource_set{};
};
enum DefaultTextureIndex : uint32_t {
  kDefaultTextureIndexBlack = 0,
  kDefaultTextureIndexWhite,
  kDefaultTextureIndexYellow,
  kDefaultTextureIndexNormal,
  kDefaultTextureIndexNum,
};
struct DefaultTextureSet {
  ResourceSet resource_set{};
  DescriptorHeapSet descriptor_heap_set{};
};
const char kDefaultTexturePath[] = "textures/";
namespace {
nlohmann::json LoadJson(const char* const filename) {
  std::ifstream file(filename);
  nlohmann::json json;
  file >> json;
  return json;
}
void SwapFilename(const char* const filepath, const char* const new_filename, const uint32_t dst_buffer_len, char* const dst_buffer) {
  const auto last_slash_pos = strrchr(filepath, '/');
  if (last_slash_pos == nullptr) {
    strcpy_s(dst_buffer, dst_buffer_len, new_filename);
    return;
  }
  const auto last_slash_index = last_slash_pos - &filepath[0];
  strcpy_s(dst_buffer, dst_buffer_len, filepath);
  snprintf(&dst_buffer[last_slash_index], dst_buffer_len - last_slash_index, "/%s", new_filename);
}
char* ReadAllBytesFromFileToTemporaryBuffer(const char* const filepath) {
  std::ifstream filestream(filepath, std::ios::binary | std::ios::ate);
  if (!filestream) { return nullptr; }
  const auto filelen = filestream.tellg();
  filestream.seekg(0, std::ios::beg);
  auto buffer = AllocateFrame<char>(filelen);
  filestream.read(buffer, filelen);
  return buffer;
}
constexpr auto ConvertToVertexBufferType(const MeshResourceType type) {
  switch (type) {
    case kMeshResourceTypeTransform: return kVertexBufferTypeNum;
    case kMeshResourceTypeIndex    : return kVertexBufferTypeNum;
    case kMeshResourceTypePosition : return kVertexBufferTypePosition;
    case kMeshResourceTypeNormal   : return kVertexBufferTypeNormal;
    case kMeshResourceTypeTangent  : return kVertexBufferTypeTangent;
    case kMeshResourceTypeTexcoord : return kVertexBufferTypeTexCoord0;
  }
  return kVertexBufferTypeNum;
}
auto ReserveBufferTransfer(const uint32_t buffer_size, const void* buffer_data, const char* const buffer_name, const uint32_t frame_index, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
  D3D12MA::Allocation *allocation_default{nullptr}, *allocation_upload{nullptr}; 
  ID3D12Resource *resource_default{nullptr}, *resource_upload{nullptr};
  {
    // create resources
    const auto desc = GetBufferDesc(buffer_size);
    CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, desc, nullptr, buffer_allocator, &allocation_default, &resource_default);
    CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, desc, nullptr, buffer_allocator, &allocation_upload, &resource_upload);
  }
  {
    // set names
    SetD3d12Name(resource_default, buffer_name);
    const uint32_t buffer_name_len = 128;
    char u_buffer_name[buffer_name_len];
    snprintf(u_buffer_name, buffer_name_len, "U%s", buffer_name);
    SetD3d12Name(resource_upload, buffer_name);
  }
  {
    // fill upload buffer
    auto dst = MapResource(resource_upload, buffer_size);
    memcpy(dst, buffer_data, buffer_size);
    UnmapResource(resource_upload);
  }
  ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, resource_default, resource_transfer);
  return std::make_pair(allocation_default, resource_default);
}
auto ParseMeshViews(const nlohmann::json& json, const char* const filename, const uint32_t frame_index, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
  MeshViews mesh_views{};
  auto mesh_allocations = AllocateArrayFrame<D3D12MA::Allocation*>(kMeshResourceTypeNum);
  auto mesh_resources = AllocateArrayFrame<ID3D12Resource*>(kMeshResourceTypeNum);
  const uint32_t binary_path_len = 128;
  char binary_path[binary_path_len];
  SwapFilename(filename, json.at("binary_filename").get<std::string>().c_str(), binary_path_len, binary_path);
  auto mesh_binary_buffer = ReadAllBytesFromFileToTemporaryBuffer(binary_path);
  // TODO check transform matrix column/row-major
  const auto& json_binary_info = json.at("binary_info");
  const char* mesh_type_name[] = {
    "transform",
    "index",
    "position",
    "normal",
    "tangent",
    "texcoord",
  };
  static_assert(countof(mesh_type_name) == kMeshResourceTypeNum);
  for (uint32_t mesh_resource_type = 0; mesh_resource_type < kMeshResourceTypeNum; mesh_resource_type++) {
    const auto& mesh_binary_info = json_binary_info.at(mesh_type_name[mesh_resource_type]);
    const auto buffer_size = mesh_binary_info.at("size_in_bytes").get<uint32_t>();
    const uint32_t buffer_name_len = 128;
    char buffer_name[buffer_name_len];
    snprintf(buffer_name, buffer_name_len, "%s_%s", filename, mesh_type_name[mesh_resource_type]);
    const uint32_t offset_in_bytes = mesh_binary_info.at("offset_in_bytes");
    auto [allocation_default, resource_default] = ReserveBufferTransfer(buffer_size, &mesh_binary_buffer[offset_in_bytes], buffer_name, frame_index, buffer_allocator, resource_transfer);
    const auto addr = resource_default->GetGPUVirtualAddress();
    const uint32_t stride_in_bytes = mesh_binary_info.at("stride_in_bytes");
    switch (mesh_resource_type) {
      case kMeshResourceTypeTransform:
        break;
      case kMeshResourceTypeIndex:
        mesh_views.index_buffer_view = {
          .BufferLocation = addr,
          .SizeInBytes    = buffer_size,
          .Format         = stride_in_bytes == 4 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT,
        };
        break;
      default:
        mesh_views.vertex_buffer_view[ConvertToVertexBufferType(static_cast<MeshResourceType>(mesh_resource_type))] = {
          .BufferLocation = addr,
          .SizeInBytes    = buffer_size,
          .StrideInBytes  = stride_in_bytes,
        };
    }
    mesh_allocations[mesh_resource_type] = allocation_default;
    mesh_resources[mesh_resource_type] = resource_default;
  }
  return std::make_tuple(mesh_views, mesh_allocations, mesh_resources);
}
auto ParseModelData(const nlohmann::json& json) {
  ModelData model_data{};
  const auto& json_meshes = json.at("meshes");
  model_data.mesh_num = GetUint32(json_meshes.size());
  model_data.instance_num = AllocateArrayScene<uint32_t>(model_data.mesh_num);
  model_data.transform_index = AllocateArrayScene<uint32_t*>(model_data.mesh_num);
  model_data.index_buffer_len = AllocateArrayScene<uint32_t>(model_data.mesh_num);
  model_data.index_buffer_offset = AllocateArrayScene<uint32_t>(model_data.mesh_num);
  model_data.vertex_buffer_offset = AllocateArrayScene<uint32_t>(model_data.mesh_num);
  model_data.material_setting_index = AllocateArrayScene<uint32_t>(model_data.mesh_num);
  for (uint32_t i = 0; i < model_data.mesh_num; i++) {
    const auto& json_mesh = json_meshes[i];
    {
      const auto& transform_json = json_mesh.at("transform");
      model_data.instance_num[i] = GetUint32(transform_json.size());
      model_data.transform_index[i] = AllocateArrayScene<uint32_t>(model_data.instance_num[i]);
      for (uint32_t j = 0; j < model_data.instance_num[i]; j++) {
        model_data.transform_index[i][j] = transform_json[j];
      }
    }
    model_data.index_buffer_len[i] = json_mesh.at("index_buffer_len");
    model_data.index_buffer_offset[i] = json_mesh.at("index_buffer_offset");
    model_data.vertex_buffer_offset[i] = json_mesh.at("vertex_buffer_index_offset");
    model_data.material_setting_index[i] = json_mesh.at("material_index");
  }
  return model_data;
}
auto GetMaterialHash(const nlohmann::json& json) {
  StrHash hash{};
  hash = CombineHash(CalcStrHash("MESH_DEFORM_TYPE"), hash);
  hash = CombineHash(CalcStrHash("MESH_DEFORM_TYPE_STATIC"), hash);
  hash = CombineHash(CalcStrHash("OPACITY_TYPE"), hash);
  if (strcmp(json.at("alpha_mode").get<std::string>().c_str(), "MASK") == 0) {
    hash = CombineHash(CalcStrHash("OPACITY_TYPE_ALPHA_MASK"), hash);
  } else {
    assert(strcmp(json.at("alpha_mode").get<std::string>().c_str(), "OPAQUE") == 0);
    hash = CombineHash(CalcStrHash("OPACITY_TYPE_OPAQUE"), hash);
  }
  return hash;
}
auto SetTexture(const nlohmann::json& json, uint32_t* texture, uint32_t* sampler) {
  *texture = json.at("texture");
  *sampler = json.at("sampler");
}
auto ParseMaterials(const nlohmann::json& json_material_settings) {
  MaterialData material_data{};
  const auto& json_materials = json_material_settings.at("materials");
  material_data.material_num = GetUint32(json_materials.size());
  material_data.material_variation_hash = AllocateArrayScene<StrHash>(material_data.material_num);
  auto albedo_infos = AllocateArrayFrame<shader::AlbedoInfo>(material_data.material_num);
  auto misc_infos = AllocateArrayFrame<shader::MaterialMiscInfo>(material_data.material_num);
  for (uint32_t i = 0; i < material_data.material_num; i++) {
    const auto& json_material = json_materials[i];
    material_data.material_variation_hash[i] = GetMaterialHash(json_material);
    const auto& json_albedo = json_material.at("albedo");
    memcpy(&albedo_infos[i].factor.x, json_albedo.at("factor").get<std::vector<float>>().data(), sizeof(albedo_infos[i].factor));
    SetTexture(json_albedo.at("texture"), &albedo_infos[i].tex, &albedo_infos[i].sampler);
    const auto& json_normal = json_material.at("normal");
    misc_infos[i].normal_scale = json_normal.at("scale");
    SetTexture(json_normal.at("texture"), &misc_infos[i].normal_tex, &misc_infos[i].normal_sampler);
    const auto& json_emissive = json_material.at("emissive");
    memcpy(&misc_infos[i].emissive_factor.x, json_emissive.at("factor").get<std::vector<float>>().data(), sizeof(misc_infos[i].emissive_factor));
    SetTexture(json_emissive.at("texture"), &misc_infos[i].emissive_tex, &misc_infos[i].emissive_sampler);
    const auto& json_metallic_roughness_occlusion = json_material.at("metallic_roughness_occlusion");
    misc_infos[i].metallic_factor = json_metallic_roughness_occlusion.at("metallic_factor");
    misc_infos[i].roughness_factor = json_metallic_roughness_occlusion.at("roughness_factor");
    misc_infos[i].occlusion_strength = json_metallic_roughness_occlusion.at("occlusion_strength");
    SetTexture(json_metallic_roughness_occlusion.at("texture"), &misc_infos[i].metallic_roughness_occlusion_tex, &misc_infos[i].metallic_roughness_occlusion_sampler);
  }
  return std::make_tuple(material_data, albedo_infos, misc_infos);
}
auto GetTexturePath(const char* const filename, const char* const texture_name) {
  const uint32_t texture_path_len = 128;
  char texture_path[texture_path_len];
  SwapFilename(filename, texture_name, texture_path_len, texture_path);
  wchar_t* dst{nullptr};
  CopyStrToWstrContainer(&dst, std::string_view(texture_path), MemoryType::kFrame);
  return dst;
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
auto ReserveTextureTransfer(const wchar_t* const texture_filepath, const char* const texture_name, const uint32_t frame_index, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
  ID3D12Resource* resource_default{nullptr};
  D3D12MA::Allocation* allocation_default{nullptr};
  const auto texture_creation_info = GatherTextureCreationInfo(device, texture_filepath);
  if (texture_creation_info.resource_desc.Width == 0) {
    return std::make_tuple(allocation_default, resource_default, texture_creation_info.resource_desc);
  }
  if (texture_creation_info.resource_desc.Flags) {
    logwarn("texture flag({}):{}", texture_name, texture_creation_info.resource_desc.Flags);
  }
  ID3D12Resource* resource_upload{nullptr};
  D3D12MA::Allocation* allocation_upload{nullptr};
  CreateBuffer(D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ,
               GetBufferDesc(texture_creation_info.total_size_in_bytes),
               nullptr, buffer_allocator,
               &allocation_upload, &resource_upload);
  CreateBuffer(D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON,
               texture_creation_info.resource_desc,
               nullptr, buffer_allocator,
               &allocation_default, &resource_default);
  SetD3d12Name(resource_upload, texture_name);
  SetD3d12Name(resource_default, texture_name);
  FillUploadResource(texture_creation_info, resource_upload);
  if (!ReserveResourceTransfer(frame_index, texture_creation_info.subresource_num, texture_creation_info.layout,
                               resource_upload, allocation_upload,
                               resource_default, resource_transfer)) {
    logwarn("texture ReserveResourceTransfer failed. {}", texture_name);
  }
  return std::make_tuple(allocation_default, resource_default, texture_creation_info.resource_desc);
}
auto GetDefaultDescriptorHandle(const char* const texture_name, const DescriptorHeapSet& default_texture_descriptor_heap_set) {
  auto texture_index = kDefaultTextureIndexBlack;
  if (strcmp(texture_name, "white")) {
    texture_index = kDefaultTextureIndexWhite;
  }
  if (strcmp(texture_name, "yellow")) {
    texture_index = kDefaultTextureIndexYellow;
  }
  if (strcmp(texture_name, "normal")) {
    texture_index = kDefaultTextureIndexNormal;
  }
  return GetDescriptorHandle(default_texture_descriptor_heap_set.heap_head_addr, default_texture_descriptor_heap_set.handle_increment_size, texture_index);
}
auto ParseTextureResources(const nlohmann::json& json_material_settings, const DescriptorHeapSet& default_texture_descriptor_heap_set, const char* const filename, const uint32_t frame_index, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
  const auto& json_textures = json_material_settings.at("textures");
  const auto texture_num = GetUint32(json_textures.size());
  auto texture_allocations = AllocateArrayFrame<D3D12MA::Allocation*>(texture_num);
  auto texture_resources = AllocateArrayFrame<ID3D12Resource*>(texture_num);
  auto texture_descriptor_heap_set = CreateDescriptorHeapSet(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, texture_num);
  for (uint32_t i = 0; i < texture_num; i++) {
    const auto& texture_name_str = json_textures[i].at("path").get<std::string>();
    const auto texture_name = texture_name_str.c_str();
    if (strchr(texture_name, '.') == nullptr) {
      // default texture (yellow, black, white, normal)
      texture_allocations[i] = nullptr;
      texture_resources[i] = nullptr;
      const auto src_handle = GetDefaultDescriptorHandle(texture_name, default_texture_descriptor_heap_set);
      const auto dst_handle = GetDescriptorHandle(texture_descriptor_heap_set.heap_head_addr, texture_descriptor_heap_set.handle_increment_size, i);
      device->CopyDescriptorsSimple(1, dst_handle, src_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
      continue;
    }
    const auto texture_path = GetTexturePath(filename, texture_name);
    auto [allocation_default, resource_default, resource_desc] = ReserveTextureTransfer(texture_path, texture_name, frame_index, device, buffer_allocator, resource_transfer);
    if (resource_desc.Width == 0) {
          logwarn("invalid texture {}", texture_name);
          continue;
    }
    const auto handle = GetDescriptorHandle(texture_descriptor_heap_set.heap_head_addr, texture_descriptor_heap_set.handle_increment_size, i);
    const auto view_desc = GetSrvDescTexture2D(resource_desc);
    device->CreateShaderResourceView(resource_default, &view_desc, handle);
    texture_allocations[i] = allocation_default;
    texture_resources[i] = resource_default;
  }
  return std::make_tuple(texture_num, texture_allocations, texture_resources, texture_descriptor_heap_set);
}
auto GetFilterType(const std::string_view& filter) {
  if (filter.compare("point") == 0) {
    return D3D12_FILTER_TYPE_POINT;
  }
  return D3D12_FILTER_TYPE_LINEAR;
}
auto ConvertToD3d12Filter(const std::string_view& min_filter_str, const std::string_view& mip_filter_str, const std::string_view& mag_filter_str) {
  const auto min_filter = GetFilterType(min_filter_str);
  const auto mip_filter = GetFilterType(mip_filter_str);
  const auto mag_filter = GetFilterType(mag_filter_str);
  return D3D12_ENCODE_BASIC_FILTER(min_filter, mip_filter, mag_filter, D3D12_FILTER_REDUCTION_TYPE_STANDARD);
}
auto ConvertToD3d12TextureAddressMode(const std::string_view& address_str) {
  if (address_str.compare("wrap") == 0) {
    return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  }
  if (address_str.compare("mirror") == 0) {
    return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
  }
  if (address_str.compare("clamp") == 0) {
    return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  }
  if (address_str.compare("border") == 0) {
    return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
  }
  if (address_str.compare("mirror_once") == 0) {
    return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
  }
  return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
}
auto ConvertToD3d12TextureAddressModeList(const nlohmann::json& address_mode) {
  assert(address_mode.size() == 2);  // 2d texture only so far.
  return std::make_pair(ConvertToD3d12TextureAddressMode(address_mode[0]), ConvertToD3d12TextureAddressMode(address_mode[1]));
}
auto ParseSamplerResources(const nlohmann::json& json_material_settings, D3d12Device* device) {
  const auto& json_samplers = json_material_settings.at("samplers");
  const auto sampler_num = GetUint32(json_samplers.size());
  auto sampler_descriptor_heap_set = CreateDescriptorHeapSet(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, sampler_num);
  D3D12_SAMPLER_DESC sampler_desc{};
  sampler_desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler_desc.MipLODBias = 0.0f;
  sampler_desc.MaxAnisotropy = 16;
  sampler_desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
  sampler_desc.MinLOD = 0.0f;
  sampler_desc.MaxLOD = 65535.0f;
  for (uint32_t i = 0; i < sampler_num; i++) {
    const auto& json_sampler = json_samplers[i];
    sampler_desc.Filter = ConvertToD3d12Filter(json_sampler.at("min_filter"), json_sampler.at("mip_filter"), json_sampler.at("mag_filter"));
    std::tie(sampler_desc.AddressU, sampler_desc.AddressV) = ConvertToD3d12TextureAddressModeList(json_sampler.at("mapmode"));
    const auto handle = GetDescriptorHandle(sampler_descriptor_heap_set.heap_head_addr, sampler_descriptor_heap_set.handle_increment_size, i);
    device->CreateSampler(&sampler_desc, handle);
  }
  return sampler_descriptor_heap_set;
}
auto GetConstantBufferDesc(const D3D12_GPU_VIRTUAL_ADDRESS&  addr, const uint32_t size) {
  return D3D12_CONSTANT_BUFFER_VIEW_DESC{
    .BufferLocation = addr,
    .SizeInBytes = size,
  };
}
auto CreateCbv(ID3D12Resource* resource, const uint32_t buffer_size, const DescriptorHeapSet& descriptor_heap_set, const uint32_t index, D3d12Device* device) {
  const auto resource_desc = GetConstantBufferDesc(resource->GetGPUVirtualAddress(), buffer_size);
  const auto handle = GetDescriptorHandle(descriptor_heap_set.heap_head_addr, descriptor_heap_set.handle_increment_size, index);
  device->CreateConstantBufferView(&resource_desc, handle);
}
template <typename T, uint32_t index>
auto CreateConstantBufer(const uint32_t num, const void* buffer, const char* const name, const uint32_t frame_index, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer,  const DescriptorHeapSet& descriptor_heap_set) {
  const auto size = AlignAddress(GetUint32(sizeof(T)) * num, 256); // cbv size must be multiple of 256
  auto [allocation, resource] = ReserveBufferTransfer(size, buffer, name, frame_index, buffer_allocator, resource_transfer);
  CreateCbv(resource, size, descriptor_heap_set, index, device);
  return std::make_pair(allocation, resource);
}
auto CreateMaterialInfoResources(const uint32_t material_num, const shader::AlbedoInfo* const albedo_infos, const shader::MaterialMiscInfo* const material_misc_infos, const uint32_t frame_index, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
  auto descriptor_heap_set = CreateDescriptorHeapSet(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2);
  auto [albedo_allocation, albedo_resource] = CreateConstantBufer<shader::AlbedoInfo, 0>(material_num, albedo_infos, "albedo", frame_index, device, buffer_allocator, resource_transfer, descriptor_heap_set);
  auto [material_misc_allocation, material_misc_resource] = CreateConstantBufer<shader::MaterialMiscInfo, 1>(material_num, material_misc_infos, "material_misc", frame_index, device, buffer_allocator, resource_transfer, descriptor_heap_set);
  return std::make_tuple(albedo_allocation, albedo_resource, material_misc_allocation, material_misc_resource, descriptor_heap_set);
}
auto ReserveDefaultTextureTransfer(const uint32_t texture_index, const char* const texture_name, const DescriptorHeapSet& descriptor_heap_set, const uint32_t frame_index, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
  auto texture_path = GetTexturePath(kDefaultTexturePath, texture_name);
  auto [allocation_default, resource_default, resource_desc] = ReserveTextureTransfer(texture_path, texture_name, frame_index, device, buffer_allocator, resource_transfer);
  assert(resource_desc.Width > 0);
  const auto handle = GetDescriptorHandle(descriptor_heap_set.heap_head_addr, descriptor_heap_set.handle_increment_size, texture_index);
  const auto view_desc = GetSrvDescTexture2D(resource_desc);
  device->CreateShaderResourceView(resource_default, &view_desc, handle);
  return std::make_pair(allocation_default, resource_default);
}
} // namespace
ModelDataSet LoadModelData(const char* const filename, const DescriptorHeapSet& default_texture_descriptor_heap_set, const uint32_t frame_index, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
  loginfo("loading {}", filename);
  auto json = LoadJson(filename);
  auto [mesh_views, mesh_allocations, mesh_resources] = ParseMeshViews(json, filename, frame_index, buffer_allocator, resource_transfer);
  auto model_data = ParseModelData(json);
  const auto& json_material_settings = json.at("material_settings");
  const auto [material_data, material_albedo_infos, material_misc_infos] = ParseMaterials(json_material_settings);
  const auto [texture_num, texture_allocations, texture_resources, texture_descriptor_heap_set] = ParseTextureResources(json_material_settings, default_texture_descriptor_heap_set, filename, frame_index, device, buffer_allocator, resource_transfer);
  const auto sampler_descriptor_heap_set = ParseSamplerResources(json_material_settings, device);
  auto [albedo_allocation, albedo_resource, material_misc_allocation, material_misc_resource, material_descriptor_heap_set] = CreateMaterialInfoResources(material_data.material_num, material_albedo_infos, material_misc_infos, frame_index, device, buffer_allocator, resource_transfer);
  const uint32_t resource_num = model_data.mesh_num * kMeshResourceTypeNum + texture_num + 2/*material_albedo_infos+material_misc_infos*/;
  auto allocations = AllocateArrayScene<D3D12MA::Allocation*>(resource_num);
  auto resources = AllocateArrayScene<ID3D12Resource*>(resource_num);
  {
    uint32_t resource_index = 0;
    for (; resource_index < kMeshResourceTypeNum; resource_index++) {
      allocations[resource_index] = mesh_allocations[resource_index];
      resources[resource_index] = mesh_resources[resource_index];
    }
    allocations[resource_index] = albedo_allocation;
    resources[resource_index] = albedo_resource;
    resource_index++;
    allocations[resource_index] = material_misc_allocation;
    resources[resource_index] = material_misc_resource;
    resource_index++;
    for (uint32_t i = 0; i < texture_num; i++) {
      allocations[resource_index + i] = texture_allocations[i];
      resources[resource_index + i] = texture_resources[i];
    }
  }
  return {
    .model_data = model_data,
    .mesh_views = mesh_views,
    .material_data = material_data,
    .material_views = {
      .material_descriptor_heap_set = std::move(material_descriptor_heap_set),
      .texture_descriptor_heap_set = std::move(texture_descriptor_heap_set),
      .sampler_descriptor_heap_set = std::move(sampler_descriptor_heap_set),
    },
    .resource_set = {
      .resource_num = resource_num,
      .allocations = allocations,
      .resources = resources,
    },
  };
}
void ReleaseResourceSet(ResourceSet* resource_set) {
  for (uint32_t i = 0; i < resource_set->resource_num; i++) {
    if (resource_set->allocations[i]) {
      resource_set->allocations[i]->Release();
    }
    if (resource_set->resources[i]) {
      resource_set->resources[i]->Release();
    }
  }
}
void ReleaseMaterialViewDescriptorHeaps(MaterialViews* material_views) {
  material_views->material_descriptor_heap_set.heap->Release();
  material_views->texture_descriptor_heap_set.heap->Release();
  material_views->sampler_descriptor_heap_set.heap->Release();
}
auto LoadDefaultTextures(const uint32_t frame_index, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
  auto allocations = AllocateArrayScene<D3D12MA::Allocation*>(kDefaultTextureIndexNum);
  auto resources = AllocateArrayScene<ID3D12Resource*>(kDefaultTextureIndexNum);
  auto descriptor_heap_set = CreateDescriptorHeapSet(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kDefaultTextureIndexNum);
  const char* const texture_name[] = {
    "black.dds",
    "white.dds",
    "yellow.dds",
    "normal.dds",
  };
  static_assert(countof(texture_name) == kDefaultTextureIndexNum);
  for (uint32_t i = 0; i < kDefaultTextureIndexNum; i++) {
    std::tie(allocations[i], resources[i]) = ReserveDefaultTextureTransfer(i, texture_name[i], descriptor_heap_set, frame_index, device, buffer_allocator, resource_transfer);
  }
  return DefaultTextureSet{
    .resource_set = {
      .resource_num = kDefaultTextureIndexNum,
      .allocations = allocations,
      .resources = resources,
    },
    .descriptor_heap_set = std::move(descriptor_heap_set),
  };
}
} // namespace illuminate
#include <memory>
#include "d3d12_dxgi_core.h"
#include "d3d12_device.h"
#include "d3d12_command_list.h"
#include "d3d12_swapchain.h"
#include "d3d12_win32_window.h"
namespace illuminate {
class GraphicDevice {
 public:
  static std::unique_ptr<GraphicDevice> CreateGraphicDevice(const nlohmann::json&);
  virtual ~GraphicDevice();
  auto DxgiAdapter() { return dxgi_core_.GetAdapter(); }
  auto D3d12Device() { return device_.Get(); }
  auto GpuBufferAllocator() { return buffer_allocator_; }
  auto ResourceTransferManager() { return &resource_transfer_; }
 private:
  GraphicDevice(const nlohmann::json& config);
  GraphicDevice() = delete;
  GraphicDevice(const GraphicDevice&) = delete;
  GraphicDevice(GraphicDevice&&) = delete;
  void operator= (const GraphicDevice&) = delete;
  uint32_t frame_buffer_num_;
  uint32_t width_;
  uint32_t height_;
  DxgiCore dxgi_core_;
  Device device_;
  D3D12MA::Allocator* buffer_allocator_;
  ResourceTransfer resource_transfer_;
  CommandListSet command_list_set_;
  Window window_;
  Swapchain swapchain_;
};
} // namespace illuminate
#include "d3d12_json_parser.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
namespace illuminate {
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  // if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) { return true; } // TODO comment-in
  switch (msg) {
    case WM_DESTROY: {
      ::PostQuitMessage(0);
      return 0;
    }
  }
  return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
std::unique_ptr<GraphicDevice> GraphicDevice::CreateGraphicDevice(const nlohmann::json& config) {
  return std::unique_ptr<GraphicDevice>(new GraphicDevice(config));
}
GraphicDevice::GraphicDevice(const nlohmann::json& config) {
  frame_buffer_num_ = config.at("frame_buffer_num");
  dxgi_core_.Init();
  device_.Init(DxgiAdapter());
  buffer_allocator_ = GetBufferAllocator(DxgiAdapter(), D3d12Device());
  {
    const auto& resource_transfer_config = config.at("resource_transfer_config");
    resource_transfer_ = PrepareResourceTransferer(frame_buffer_num_, resource_transfer_config.at("max_buffer_transfer_num_per_frame"), resource_transfer_config.at("max_mip_map_num"));
  }
  {
    const auto& command_queues = config.at("command_queue");
    const auto command_queue_num = GetUint32(command_queues.size());
    auto command_queue_type = AllocateArrayFrame<D3D12_COMMAND_LIST_TYPE>(command_queue_num);
    auto command_queue_priority = AllocateArrayFrame<D3D12_COMMAND_QUEUE_PRIORITY>(command_queue_num);
    auto command_list_num_per_queue = AllocateArrayFrame<uint32_t>(command_queue_num);
    uint32_t command_allocator_num_per_queue_type[kCommandQueueTypeNum]{};
    for (uint32_t i = 0; i < command_queue_num; i++) {
      auto command_queue_type_str = GetStringView(command_queues[i], "type");
      command_queue_type[i] = D3D12_COMMAND_LIST_TYPE_DIRECT;
      if (command_queue_type_str.compare("compute") == 0) {
        command_queue_type[i] = D3D12_COMMAND_LIST_TYPE_COMPUTE;
      } else if (command_queue_type_str.compare("copy") == 0) {
        command_queue_type[i] = D3D12_COMMAND_LIST_TYPE_COPY;
      }
      auto command_queue_priority_str = GetStringView(command_queues[i], "priority");
      command_queue_priority[i] = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
      if (command_queue_priority_str.compare("high") == 0) {
        command_queue_priority[i] = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
      } else if (command_queue_priority_str.compare("global realtime") == 0) {
        command_queue_priority[i] = D3D12_COMMAND_QUEUE_PRIORITY_GLOBAL_REALTIME;
      }
      command_list_num_per_queue[i] = command_queues[i].at("command_list_num");
    }
    command_list_set_.Init(D3d12Device(), command_queue_num, command_queue_type, command_queue_priority, command_list_num_per_queue, frame_buffer_num_, command_allocator_num_per_queue_type);
    for (uint32_t i = 0; i < command_queue_num; i++) {
      SetD3d12Name(command_list_set_.GetCommandQueue(i), GetStringView(command_queues[i], "name"));
    }
  }
  {
    const auto& window = config.at("window");
    width_ = window.at("width");
    height_ = window.at("height");
    window_.Init(GetStringView(window.at("title")).data(), width_, height_, WndProc);
  }
  {
    const auto& swapchain = config.at("swapchain");
    uint32_t command_queue_index = 0;
    const auto& command_queues = config.at("command_queue");
    const auto& name = GetStringView(swapchain.at("command_queue"));
    for (uint32_t i = 0; i < command_queues.size(); i++) {
      if (GetStringView(command_queues[i].at("name")) == name) {
        command_queue_index = i;
        break;
      }
    }
    const auto swapchain_format = GetDxgiFormat(swapchain.at("format"));
    swapchain_.Init(dxgi_core_.GetFactory(), command_list_set_.GetCommandQueue(command_queue_index), D3d12Device(), window_.GetHwnd(), swapchain_format, frame_buffer_num_ + 1, frame_buffer_num_, DXGI_USAGE_RENDER_TARGET_OUTPUT);
  }
}
GraphicDevice::~GraphicDevice() {
  ClearResourceTransfer(frame_buffer_num_, ResourceTransferManager());
  swapchain_.Term();
  window_.Term();
  command_list_set_.Term();
  buffer_allocator_->Release();
  device_.Term();
  dxgi_core_.Term();
}
}
#include "doctest/doctest.h"
TEST_CASE("scene loader") { // NOLINT
  using namespace illuminate; // NOLINT
  DxgiCore dxgi_core;
  dxgi_core.Init(); // NOLINT
  Device device;
  device.Init(dxgi_core.GetAdapter()); // NOLINT
  auto buffer_allocator = GetBufferAllocator(dxgi_core.GetAdapter(), device.Get());
  const uint32_t frame_num = 2;
  uint32_t frame_index = 0;
  const uint32_t max_mip_num = 12;
  auto resource_transfer = PrepareResourceTransferer(frame_num, 1024, max_mip_num);
  auto default_textures = LoadDefaultTextures(frame_index, device.Get(), buffer_allocator, &resource_transfer);
  auto model_data_set = LoadModelData("scenedata/BoomBoxWithAxes/BoomBoxWithAxes.json", default_textures.descriptor_heap_set, frame_index, device.Get(), buffer_allocator, &resource_transfer);
  ClearResourceTransfer(frame_num, &resource_transfer);
  ReleaseResourceSet(&model_data_set.resource_set);
  ReleaseMaterialViewDescriptorHeaps(&model_data_set.material_views);
  ReleaseResourceSet(&default_textures.resource_set);
  default_textures.descriptor_heap_set.heap->Release();
  buffer_allocator->Release();
  device.Term();
  dxgi_core.Term();
  ClearAllAllocations();
}
TEST_CASE("scene viewer") { // NOLINT
  using namespace illuminate; // NOLINT
  auto graphic_device = GraphicDevice::CreateGraphicDevice(LoadJson("configs/config_default.json"));
  uint32_t frame_index = 0;
  auto default_textures = LoadDefaultTextures(frame_index, graphic_device->D3d12Device(), graphic_device->GpuBufferAllocator(), graphic_device->ResourceTransferManager());
  auto model_data_set = LoadModelData("scenedata/BoomBoxWithAxes/BoomBoxWithAxes.json", default_textures.descriptor_heap_set, frame_index, graphic_device->D3d12Device(), graphic_device->GpuBufferAllocator(), graphic_device->ResourceTransferManager());
  ReleaseResourceSet(&model_data_set.resource_set);
  ReleaseMaterialViewDescriptorHeaps(&model_data_set.material_views);
  ReleaseResourceSet(&default_textures.resource_set);
  default_textures.descriptor_heap_set.heap->Release();
  graphic_device.reset();
  ClearAllAllocations();
}
