#include "d3d12_scene_loader.h"
#include <fstream>
#include "gfxminimath/gfxminimath.h"
#include "d3d12_descriptors.h"
#include "d3d12_gpu_buffer_allocator.h"
#include "d3d12_memory_allocators.h"
#include "d3d12_resource_transfer.h"
#include "d3d12_src_common.h"
#include "d3d12_texture_util.h"
#include "shader/include/shader_defines.h"
namespace illuminate {
enum MeshResourceType : uint32_t {
  kMeshResourceTypeTransformOffset,
  kMeshResourceTypeTransformIndex,
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
  uint32_t* index_buffer_len{nullptr};
  uint32_t* index_buffer_offset{nullptr};
  uint32_t* vertex_buffer_offset{nullptr};
  uint32_t* material_setting_index{nullptr};
};
struct MeshViews {
  D3D12_CPU_DESCRIPTOR_HANDLE transform_offset_handle{};
  D3D12_CPU_DESCRIPTOR_HANDLE transform_index_handle{};
  D3D12_CPU_DESCRIPTOR_HANDLE transform_handle{};
  D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
  D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view[kVertexBufferTypeNum]{};
};
struct MaterialData {
  uint32_t material_num{};
  StrHash* material_variation_hash{};
};
struct DescriptorHeaps {
  DescriptorHeapSet mesh_descriptor_heap_set{};
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
  DescriptorHeaps descriptor_heaps{};
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
    case kMeshResourceTypePosition : return kVertexBufferTypePosition;
    case kMeshResourceTypeNormal   : return kVertexBufferTypeNormal;
    case kMeshResourceTypeTangent  : return kVertexBufferTypeTangent;
    case kMeshResourceTypeTexcoord : return kVertexBufferTypeTexCoord0;
  }
  assert(false);
  return kVertexBufferTypeNum;
}
auto ReserveBufferTransfer(const uint32_t buffer_size, const void* buffer_data, const uint32_t resource_size, const char* const buffer_name, const uint32_t frame_index, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
  D3D12MA::Allocation *allocation_default{nullptr}, *allocation_upload{nullptr}; 
  ID3D12Resource *resource_default{nullptr}, *resource_upload{nullptr};
  {
    // create resources
    const auto desc = GetBufferDesc(resource_size);
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
constexpr auto GetByteAddressBufferSrvDesc(const uint32_t buffer_size) {
  auto desc = D3D12_SHADER_RESOURCE_VIEW_DESC {
    .Format = DXGI_FORMAT_R32_TYPELESS,
    .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
    .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    .Buffer = {
      .FirstElement = 0,
      .NumElements = buffer_size / 4, // 4 for sizeof(R32_TYPELESS)
      .StructureByteStride = 0,
      .Flags = D3D12_BUFFER_SRV_FLAG_RAW,
    },
  };
  return desc;
}
auto CreateByteAddressBufferSrv(const uint32_t buffer_size, const DescriptorHeapSet& descriptor_heap_set, const uint32_t index, ID3D12Resource* resource, D3d12Device* device) {
  auto desc = GetByteAddressBufferSrvDesc(buffer_size);
  const auto handle = GetDescriptorHandle(descriptor_heap_set.heap_head_addr, descriptor_heap_set.handle_increment_size, index);
  device->CreateShaderResourceView(resource, &desc, handle);
  return handle;
}
auto ParseMeshViews(const nlohmann::json& json, const char* const filename, const uint32_t frame_index, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
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
    "transform_offset",
    "transform_index",
    "transform",
    "index",
    "position",
    "normal",
    "tangent",
    "texcoord",
  };
  static_assert(countof(mesh_type_name) == kMeshResourceTypeNum);
  auto mesh_descriptor_heap_set = CreateDescriptorHeapSet(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kMeshResourceTypeNum - static_cast<uint32_t>(kVertexBufferTypeNum) - 1/*index buffer*/);
  for (uint32_t mesh_resource_type = 0; mesh_resource_type < kMeshResourceTypeNum; mesh_resource_type++) {
    const auto& mesh_binary_info = json_binary_info.at(mesh_type_name[mesh_resource_type]);
    const auto buffer_size = mesh_binary_info.at("size_in_bytes").get<uint32_t>();
    const uint32_t buffer_name_len = 128;
    char buffer_name[buffer_name_len];
    snprintf(buffer_name, buffer_name_len, "%s_%s", filename, mesh_type_name[mesh_resource_type]);
    const uint32_t offset_in_bytes = mesh_binary_info.at("offset_in_bytes");
    auto [allocation_default, resource_default] = ReserveBufferTransfer(buffer_size, &mesh_binary_buffer[offset_in_bytes], buffer_size, buffer_name, frame_index, buffer_allocator, resource_transfer);
    const auto addr = resource_default->GetGPUVirtualAddress();
    const uint32_t stride_in_bytes = mesh_binary_info.at("stride_in_bytes");
    switch (mesh_resource_type) {
      case kMeshResourceTypeTransformOffset:
        mesh_views.transform_offset_handle = CreateByteAddressBufferSrv(buffer_size, mesh_descriptor_heap_set, 0, resource_default, device);
        break;
      case kMeshResourceTypeTransformIndex:
        mesh_views.transform_index_handle = CreateByteAddressBufferSrv(buffer_size, mesh_descriptor_heap_set, 1, resource_default, device);
        break;
      case kMeshResourceTypeTransform:
        mesh_views.transform_handle = CreateByteAddressBufferSrv(buffer_size, mesh_descriptor_heap_set, 2, resource_default, device);
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
  return std::make_tuple(mesh_views, mesh_allocations, mesh_resources, mesh_descriptor_heap_set);
}
auto ParseModelData(const nlohmann::json& json) {
  ModelData model_data{};
  const auto& json_meshes = json.at("meshes");
  model_data.mesh_num = GetUint32(json_meshes.size());
  model_data.instance_num = AllocateArrayScene<uint32_t>(model_data.mesh_num);
  model_data.index_buffer_len = AllocateArrayScene<uint32_t>(model_data.mesh_num);
  model_data.index_buffer_offset = AllocateArrayScene<uint32_t>(model_data.mesh_num);
  model_data.vertex_buffer_offset = AllocateArrayScene<uint32_t>(model_data.mesh_num);
  model_data.material_setting_index = AllocateArrayScene<uint32_t>(model_data.mesh_num);
  for (uint32_t i = 0; i < model_data.mesh_num; i++) {
    const auto& json_mesh = json_meshes[i];
    model_data.instance_num[i] = json_mesh.at("instance_num");
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
auto GetConstantBufferDesc(const D3D12_GPU_VIRTUAL_ADDRESS& addr, const uint32_t size) {
  return D3D12_CONSTANT_BUFFER_VIEW_DESC{
    .BufferLocation = addr,
    .SizeInBytes = size,
  };
}
auto CreateCbv(const D3D12_GPU_VIRTUAL_ADDRESS& addr, const uint32_t buffer_size, const DescriptorHeapSet& descriptor_heap_set, const uint32_t index, D3d12Device* device) {
  const auto resource_desc = GetConstantBufferDesc(addr, buffer_size);
  const auto handle = GetDescriptorHandle(descriptor_heap_set.heap_head_addr, descriptor_heap_set.handle_increment_size, index);
  device->CreateConstantBufferView(&resource_desc, handle);
  return handle;
}
template <typename T, uint32_t index>
auto CreateConstantBufer(const uint32_t num, const void* buffer, const char* const name, const uint32_t frame_index, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer,  const DescriptorHeapSet& descriptor_heap_set) {
  const auto size = GetUint32(sizeof(T)) * num;
  const auto aligned_buffer_size = AlignAddress(size, 256);
  auto [allocation, resource] = ReserveBufferTransfer(size, buffer, aligned_buffer_size, name, frame_index, buffer_allocator, resource_transfer); // cbv size must be multiple of 256
  CreateCbv(resource->GetGPUVirtualAddress(), aligned_buffer_size, descriptor_heap_set, index, device);
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
  auto [mesh_views, mesh_allocations, mesh_resources, mesh_descriptor_heap_set] = ParseMeshViews(json, filename, frame_index, device, buffer_allocator, resource_transfer);
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
    .descriptor_heaps = {
      .mesh_descriptor_heap_set = std::move(mesh_descriptor_heap_set),
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
void ReleaseMaterialViewDescriptorHeaps(DescriptorHeaps* descriptor_heaps) {
  descriptor_heaps->mesh_descriptor_heap_set.heap->Release();
  descriptor_heaps->material_descriptor_heap_set.heap->Release();
  descriptor_heaps->texture_descriptor_heap_set.heap->Release();
  descriptor_heaps->sampler_descriptor_heap_set.heap->Release();
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
#include "rps/rps.h"
#include "d3d12_dxgi_core.h"
#include "d3d12_device.h"
#include "d3d12_command_list.h"
#include "d3d12_swapchain.h"
#include "d3d12_win32_window.h"
namespace illuminate {
class GraphicDevice final {
 public:
  static std::unique_ptr<GraphicDevice> CreateGraphicDevice();
  ~GraphicDevice();
  auto GetDxgiFactory() { return dxgi_core_.GetFactory(); }
  auto GetDxgiAdapter() { return dxgi_core_.GetAdapter(); }
  auto GetDevice() { return device_.Get(); }
 private:
  GraphicDevice();
  DxgiCore dxgi_core_;
  Device device_;
  GraphicDevice(const GraphicDevice&) = delete;
  GraphicDevice(GraphicDevice&&) = delete;
  void operator= (const GraphicDevice&) = delete;
  void operator= (GraphicDevice&&) = delete;
};
class RenderGraph;
struct SceneData;
class CommandRecorder final {
 public:
  static std::unique_ptr<CommandRecorder> CreateCommandRecorder(const nlohmann::json& config, DxgiFactory* dxgi_factory, DxgiAdapter* dxgi_adapter, D3d12Device* device);
  ~CommandRecorder();
  constexpr auto GetCommandQueueNum() const { return command_queue_num_; }
  constexpr auto GetCommandQueueType() const { return command_list_set_.GetCommandQueueTypeList(); }
  constexpr auto GetFrameBufferIndex() const { return frame_buffer_index_; }
  auto GetGpuBufferAllocator() { return buffer_allocator_; }
  auto GetResourceTransferManager() { return &resource_transfer_; }
  bool ProcessWindowMessage();
  void PreUpdate();
  void RecordCommands(RenderGraph* render_graph, const SceneData& scene_data);
  void Present();
  void WaitAll() { command_queue_signals_.WaitAll(device_); }
 private:
  CommandRecorder(const nlohmann::json& config, DxgiFactory* dxgi_factory, DxgiAdapter* dxgi_adapter, D3d12Device* device);
  RpsResult UpdateRenderGraph(RenderGraph* render_graph);
  D3d12CommandList* GetCommandList(const uint32_t command_queue_index);
  uint64_t TrasnferReservedResources();
  std::pair<uint32_t*, uint32_t*> GetFirstAndLastBatchIndexPerQueue(const RpsRenderGraphBatchLayout& batch_layout);
  D3d12Device* device_;
  Window window_;
  Swapchain swapchain_;
  D3D12MA::Allocator* buffer_allocator_;
  ResourceTransfer resource_transfer_;
  DescriptorGpu descriptor_gpu_;
  uint32_t command_queue_num_;
  CommandListSet command_list_set_;
  CommandQueueSignals command_queue_signals_;
  uint64_t** frame_signals_;
  uint32_t frame_count_{0};
  uint32_t frame_buffer_index_{0};
  uint32_t frame_buffer_num_{0};
  uint32_t copy_queue_index_{0};
  CommandRecorder() = delete;
  CommandRecorder(const CommandRecorder&) = delete;
  CommandRecorder(CommandRecorder&&) = delete;
  void operator=(const CommandRecorder&) = delete;
  void operator=(CommandRecorder&&) = delete;
};
class RenderGraph final {
 public:
  struct Config {
    D3d12Device* device{};
    uint32_t command_queue_num{};
    const D3D12_COMMAND_LIST_TYPE* command_queue_type{};
    uint32_t material_hash_list_len{};
    StrHash* material_hash_list{};
  };
  struct GeomPassParams {
    StrHash material_index{};
  };
  static std::unique_ptr<RenderGraph> CreateRenderGraph(const Config& config);
  ~RenderGraph();
  constexpr auto GetRenderGraph() { return rps_render_graph_; }
 private:
  RenderGraph(const Config& config);
  RpsDevice rps_device_;
  RpsRenderGraph rps_render_graph_;
  GeomPassParams prez_pass_params_;
  RenderGraph(const RenderGraph&) = delete;
  RenderGraph(RenderGraph&&) = delete;
  void operator=(const RenderGraph&) = delete;
  void operator=(RenderGraph&&) = delete;
};
} // namespace illuminate
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "rps/runtime/d3d12/rps_d3d12_runtime.h"
#include "d3d12_json_parser.h"
#include "d3d12_shader_compiler.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
namespace illuminate {
struct SceneParams;
struct UtilFuncs;
struct GpuHandles;
struct RenderPassCommonData {
  ModelDataSet* model_data_set{nullptr};
  MaterialList* material_list{nullptr};
  SceneParams*  scene_params{nullptr};
  UtilFuncs*    util_funcs{nullptr};
  GpuHandles*   gpu_handles{nullptr};
};
struct SceneData {
  ModelDataSet* model_data_set{nullptr};
  MaterialList* material_list{nullptr};
  SceneParams*  scene_params{nullptr};
};
struct SceneParams {
  float camera_pos[3]{};
  float fov_vertical{40.0f};
  float camera_focus[3]{};
  float near_z{0.001f};
  Size2d primarybuffer_size;
  float far_z{1000.0f};
  float light_direction[3]{};
};
struct UtilFuncs {
  using WriteToGpuHandle = std::function<D3D12_GPU_DESCRIPTOR_HANDLE(const D3D12_CPU_DESCRIPTOR_HANDLE* const, const uint32_t)>;                                                                                
  WriteToGpuHandle write_to_gpu_handle{};
};
struct GpuHandles {
  D3D12_GPU_DESCRIPTOR_HANDLE geom_pass_gpu_handle{};
};
namespace {
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) { return true; }
  switch (msg) {
    case WM_DESTROY: {
      ::PostQuitMessage(0);
      return 0;
    }
  }
  return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
auto InitImgui(HWND hwnd, D3d12Device* device, const uint32_t frame_buffer_num, const DXGI_FORMAT format, const D3D12_CPU_DESCRIPTOR_HANDLE& imgui_font_cpu_handle, const D3D12_GPU_DESCRIPTOR_HANDLE& imgui_font_gpu_handle) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX12_Init(device, frame_buffer_num, format, nullptr/*descriptor heap not used in single viewport mode*/, imgui_font_cpu_handle, imgui_font_gpu_handle);
  if (!ImGui_ImplDX12_CreateDeviceObjects()) {
    logerror("ImGui_ImplDX12_CreateDeviceObjects failed.");
    assert(false);
  }
}
auto TermImgui() {
  ImPlot::DestroyContext();
  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
}
auto UpdateImgui() {
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
}
auto SetupGraphicDevices(const nlohmann::json& json) {
  auto graphic_device = GraphicDevice::CreateGraphicDevice();
  auto command_recorder = CommandRecorder::CreateCommandRecorder(json, graphic_device->GetDxgiFactory(), graphic_device->GetDxgiAdapter(), graphic_device->GetDevice());
  return std::make_pair(std::move(graphic_device), std::move(command_recorder));
}
auto SetupRenderGraph(GraphicDevice* graphic_device, CommandRecorder* command_recorder, const MaterialPack& material_pack) {
  auto render_graph = RenderGraph::CreateRenderGraph({
      .device             = graphic_device->GetDevice(),
      .command_queue_num  = command_recorder->GetCommandQueueNum(),
      .command_queue_type = command_recorder->GetCommandQueueType(),
      .material_hash_list_len = material_pack.material_list.material_num,
      .material_hash_list     = material_pack.config.material_hash_list,
    });
  return render_graph;
}
void RecordTransferResourceCommand(D3d12CommandList* command_list, const uint32_t frame_index, ResourceTransfer* resource_transfer) {
  for (uint32_t i = 0; i < resource_transfer->transfer_reserved_buffer_num[frame_index]; i++) {
    command_list->CopyResource(resource_transfer->transfer_reserved_buffer_dst[frame_index][i],
                               resource_transfer->transfer_reserved_buffer_src[frame_index][i]);
  }
  for (uint32_t i = 0; i < resource_transfer->transfer_reserved_texture_num[frame_index]; i++) {
    for (uint32_t j = 0; j < resource_transfer->texture_subresource_num[frame_index][i]; j++) {
      CD3DX12_TEXTURE_COPY_LOCATION src(resource_transfer->transfer_reserved_texture_src[frame_index][i],
                                        resource_transfer->texture_layout[frame_index][i][j]);
      CD3DX12_TEXTURE_COPY_LOCATION dst(resource_transfer->transfer_reserved_texture_dst[frame_index][i], j);
      command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }
  }
}
auto GetCopyQueueIndex(const uint32_t command_queue_num, const D3D12_COMMAND_LIST_TYPE* const command_queue_type) {
  for (uint32_t i = 0; i < command_queue_num; i++) {
    if (command_queue_type[i] == D3D12_COMMAND_LIST_TYPE_COPY) { return i; }
  }
  for (uint32_t i = 0; i < command_queue_num; i++) {
    if (command_queue_type[i] == D3D12_COMMAND_LIST_TYPE_DIRECT) { return i; }
  }
  assert(false);
  return 0U;
}
auto AllocateFrameSignals(const uint32_t signal_num) {
  auto signal_queue_index = AllocateArrayFrame<uint32_t>(signal_num);
  auto fence_val = AllocateArrayFrame<uint64_t>(signal_num);
  std::fill(signal_queue_index, signal_queue_index + signal_num, 0);
  std::fill(fence_val, fence_val + signal_num, 0);
  return std::make_pair(signal_queue_index, fence_val);
}
auto RegisterWaitSignalForResourceTransfer(const uint32_t batch_index, const uint32_t* const first_batch_index_per_queue, const uint32_t command_queue_index, const uint64_t copy_queue_signal, const uint32_t copy_queue_index_, CommandQueueSignals* command_queue_signals) {
  if (batch_index != first_batch_index_per_queue[command_queue_index]) { return; }
  if (copy_queue_signal == CommandQueueSignals::kInvalidSignalVal) { return; }
  if (command_queue_index == copy_queue_index_) { return; }
  const auto result = command_queue_signals->RegisterWaitOnCommandQueue(copy_queue_index_, command_queue_index, copy_queue_signal);
  if (!result) {
    logerror("failed RegisterWaitOnCommandQueue for copy_queue_signal. {} {} {} {}", batch_index, copy_queue_index_, command_queue_index, copy_queue_signal);
  }
}
auto RegisterWaitOnCommandQueue(const RpsRenderGraphBatchLayout& batch_layout, const RpsCommandBatch& batch, CommandQueueSignals* command_queue_signals, const uint32_t* const signal_queue_index, const uint64_t* const fence_val, const uint32_t command_queue_index) {
  for (uint32_t wait_fence_index = batch.waitFencesBegin; wait_fence_index < (batch.waitFencesBegin + batch.numWaitFences); wait_fence_index++) {
    const auto fence_index = batch_layout.pWaitFenceIndices[wait_fence_index];
    if (!command_queue_signals->RegisterWaitOnCommandQueue(signal_queue_index[fence_index], command_queue_index, fence_val[fence_index])) {
      logerror("RegisterWaitOnCommandQueue failed. {} {} {} {}", command_queue_index, wait_fence_index, fence_val[batch_layout.pWaitFenceIndices[wait_fence_index]]);
      return false;
    }
  }
  return true;
}
auto RegisterQueueSignal(const RpsCommandBatch& batch, const uint32_t batch_index, const uint32_t* const last_batch_index_per_queue, CommandQueueSignals* command_queue_signals, const uint32_t command_queue_index, uint64_t* const frame_signals, uint32_t* const signal_queue_index, uint64_t* const fence_val) {
  if (batch.signalFenceIndex == RPS_INDEX_NONE_U32 && batch_index != last_batch_index_per_queue[command_queue_index]) { return; }
  const auto next_signal_val = command_queue_signals->SucceedSignal(command_queue_index);
  if (next_signal_val == CommandQueueSignals::kInvalidSignalVal) {
    logerror("command_queue_signals.SucceedSignal failed. {} {}", command_queue_index, batch_index);
    return;
  }
  frame_signals[command_queue_index] = next_signal_val;
  if (batch.signalFenceIndex != RPS_INDEX_NONE_U32) {
    const auto fence_index = batch.signalFenceIndex;
    signal_queue_index[fence_index] = command_queue_index;
    fence_val[fence_index] = next_signal_val;
  }
}
} // namespace
std::unique_ptr<GraphicDevice> GraphicDevice::CreateGraphicDevice() {
  return std::unique_ptr<GraphicDevice>(new GraphicDevice());
}
GraphicDevice::GraphicDevice() {
  dxgi_core_.Init();
  device_.Init(dxgi_core_.GetAdapter());
}
GraphicDevice::~GraphicDevice() {
  device_.Term();
  dxgi_core_.Term();
}
std::unique_ptr<CommandRecorder> CommandRecorder::CreateCommandRecorder(const nlohmann::json& config, DxgiFactory* dxgi_factory, DxgiAdapter* dxgi_adapter, D3d12Device* device) {
  return std::unique_ptr<CommandRecorder>(new CommandRecorder(config, dxgi_factory, dxgi_adapter, device));
}
CommandRecorder::CommandRecorder(const nlohmann::json& config, DxgiFactory* dxgi_factory, DxgiAdapter* dxgi_adapter, D3d12Device* device) {
  device_ = device;
  frame_buffer_num_ = config.at("frame_buffer_num");
  buffer_allocator_ = GetBufferAllocator(dxgi_adapter, device);
  {
    const auto& resource_transfer_config = config.at("resource_transfer_config");
    resource_transfer_ = PrepareResourceTransferer(frame_buffer_num_, resource_transfer_config.at("max_buffer_transfer_num_per_frame"), resource_transfer_config.at("max_mip_map_num"));
  }
  {
    const auto& command_queues = config.at("command_queue");
    command_queue_num_ = GetUint32(command_queues.size());
    auto command_queue_type = AllocateArrayFrame<D3D12_COMMAND_LIST_TYPE>(command_queue_num_);
    auto command_queue_priority = AllocateArrayFrame<D3D12_COMMAND_QUEUE_PRIORITY>(command_queue_num_);
    auto command_list_num_per_queue = AllocateArrayFrame<uint32_t>(command_queue_num_);
    uint32_t command_allocator_num_per_queue_type[kCommandQueueTypeNum]{};
    for (uint32_t i = 0; i < command_queue_num_; i++) {
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
      command_allocator_num_per_queue_type[GetCommandQueueTypeIndex(command_queue_type[i])] += command_list_num_per_queue[i];
    }
    command_list_set_.Init(device, command_queue_num_, command_queue_type, command_queue_priority, command_list_num_per_queue, frame_buffer_num_, command_allocator_num_per_queue_type);
    for (uint32_t i = 0; i < command_queue_num_; i++) {
      SetD3d12Name(command_list_set_.GetCommandQueue(i), GetStringView(command_queues[i], "name"));
    }
    copy_queue_index_ = GetCopyQueueIndex(command_queue_num_, command_queue_type);
  }
  command_queue_signals_.Init(device, command_queue_num_, command_list_set_.GetCommandQueueList());
  frame_signals_ = AllocateArraySystem<uint64_t*>(frame_buffer_num_);
  for (uint32_t i = 0; i < frame_buffer_num_; i++) {
    frame_signals_[i] = AllocateArraySystem<uint64_t>(command_queue_num_);
    memset(frame_signals_[i], 0, command_queue_num_);
  }
  {
    const auto& window = config.at("window");
    const auto width = window.at("width").get<uint32_t>();
    const auto height = window.at("height").get<uint32_t>();
    window_.Init(GetStringView(window.at("title")).data(), width, height, WndProc);
  }
  {
    const auto& swapchain = config.at("swapchain");
    uint32_t command_queue_index = 0;
    const auto& command_queues = config.at("command_queue");
    const auto& name = GetStringView(swapchain.at("command_queue"));
    const auto command_queue_num = GetUint32(command_queues.size());
    for (uint32_t i = 0; i < command_queue_num; i++) {
      if (GetStringView(command_queues[i].at("name")) == name) {
        command_queue_index = i;
        break;
      }
    }
    const auto swapchain_format = GetDxgiFormat(swapchain.at("format"));
    const auto frame_buffer_num = config.at("frame_buffer_num");
    swapchain_.Init(dxgi_factory, command_list_set_.GetCommandQueue(command_queue_index), device, window_.GetHwnd(), swapchain_format, frame_buffer_num + 1, frame_buffer_num, DXGI_USAGE_RENDER_TARGET_OUTPUT);
  }
  {
    const auto& descriptor_gpu = config.at("descriptor_gpu");
    descriptor_gpu_.Init(device, descriptor_gpu.at("gpu_handle_num_view"), descriptor_gpu.at("gpu_handle_num_sampler"));
    descriptor_gpu_.SetPersistentViewHandleNum(descriptor_gpu.at("persistent_view_num"));
    descriptor_gpu_.SetPersistentSamplerHandleNum(descriptor_gpu.at("persistent_sampler_num"));
  }
  auto descriptor_heap_set = CreateDescriptorHeapSet(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 32);
  const uint32_t imgui_descriptor_index = 0;
  const auto imgui_cpu_handle = GetDescriptorHandle(descriptor_heap_set.heap_head_addr, descriptor_heap_set.handle_increment_size, imgui_descriptor_index);
  const auto swapchain_format = GetDxgiFormat(config.at("swapchain").at("format"));
  InitImgui(window_.GetHwnd(), device, frame_buffer_num_, swapchain_format, imgui_cpu_handle, descriptor_gpu_.GetViewGpuHandle(imgui_descriptor_index));
  descriptor_gpu_.WriteToPersistentViewHandleRange(imgui_descriptor_index, 1, imgui_cpu_handle, device);
  descriptor_heap_set.heap->Release();
}
CommandRecorder::~CommandRecorder() {
  WaitAll();
  TermImgui();
  descriptor_gpu_.Term();
  ClearResourceTransfer(frame_buffer_num_, GetResourceTransferManager());
  buffer_allocator_->Release();
  command_queue_signals_.Term();
  command_list_set_.Term();
  swapchain_.Term();
  window_.Term();
}
bool CommandRecorder::ProcessWindowMessage() {
  if (!window_.ProcessMessage()) { return false; }
  return true;
}
void CommandRecorder::PreUpdate() {
  UpdateImgui();
}
void CommandRecorder::RecordCommands(RenderGraph* render_graph, const SceneData& scene_data) {
  swapchain_.UpdateBackBufferIndex();
  command_queue_signals_.WaitOnCpu(device_, frame_signals_[frame_buffer_index_]);
  command_list_set_.SetCurrentFrameBufferIndex(frame_buffer_index_);
  if (const auto result = UpdateRenderGraph(render_graph); result != RPS_OK) {
    logerror("rpsRenderGraphUpdate failed. {}", result);
    return;
  }
  const auto copy_queue_signal = TrasnferReservedResources();
  RpsRenderGraphBatchLayout batch_layout = {};
  if (const auto result = rpsRenderGraphGetBatchLayout(render_graph->GetRenderGraph(), &batch_layout); result != RPS_OK) {
    logerror("rpsRenderGraphGetBatchLayout failed. {}", result);
    return;
  }
  auto [first_batch_index_per_queue, last_batch_index_per_queue] = GetFirstAndLastBatchIndexPerQueue(batch_layout);
  UtilFuncs util_funcs {
    .write_to_gpu_handle = [descriptor_gpu = &descriptor_gpu_, device = device_](const D3D12_CPU_DESCRIPTOR_HANDLE* const cpu_handles, const uint32_t cpu_handle_num) {
      return descriptor_gpu->WriteToTransientViewHandleRange(cpu_handle_num, cpu_handles, device);
    },
  };
  GpuHandles gpu_handles{};
  RenderPassCommonData render_pass_common_data {
    .model_data_set = scene_data.model_data_set,
    .material_list  = scene_data.material_list,
    .scene_params   = scene_data.scene_params,
    .util_funcs     = &util_funcs,
    .gpu_handles    = &gpu_handles,
  };
  auto [signal_queue_index, fence_val] = AllocateFrameSignals(batch_layout.numFenceSignals);
  for (uint32_t batch_index = 0; batch_index < batch_layout.numCmdBatches; batch_index++) {
    const auto& batch = batch_layout.pCmdBatches[batch_index];
    const auto command_queue_index = batch.queueIndex;
    RegisterWaitSignalForResourceTransfer(batch_index, first_batch_index_per_queue, command_queue_index, copy_queue_signal, copy_queue_index_, &command_queue_signals_);
    auto command_list = GetCommandList(command_queue_index);
    if (!RegisterWaitOnCommandQueue(batch_layout, batch, &command_queue_signals_, signal_queue_index, fence_val, command_queue_index)) { return; }
    RpsRenderGraphRecordCommandInfo record_info = {
      .hCmdBuffer    = rpsD3D12CommandListToHandle(command_list),
      .pUserContext  = &render_pass_common_data,
      .cmdBeginIndex = batch.cmdBegin,
      .numCmds       = batch.numCmds,
      // .flags         = RPS_RECORD_COMMAND_FLAG_ENABLE_COMMAND_DEBUG_MARKERS,
    };
    if (const auto result = rpsRenderGraphRecordCommands(render_graph->GetRenderGraph(), &record_info); result != RPS_OK) {
      logerror("rpsRenderGraphRecordCommands failed. {} {}", result, batch_index);
      return;
    }
    command_list_set_.ExecuteCommandList(command_queue_index);
    RegisterQueueSignal(batch, batch_index, last_batch_index_per_queue, &command_queue_signals_, command_queue_index, frame_signals_[frame_buffer_index_], signal_queue_index, fence_val);
  }
}
void CommandRecorder::Present() {
  swapchain_.Present();
  frame_count_++;
  frame_buffer_index_ = (frame_count_) % frame_buffer_num_;
}
D3d12CommandList* CommandRecorder::GetCommandList(const uint32_t command_queue_index) {
  auto command_list = command_list_set_.GetCommandList(device_, command_queue_index);
  if (command_queue_index != copy_queue_index_) {
    descriptor_gpu_.SetDescriptorHeapsToCommandList(1, &command_list);
  }
  return command_list;
}
RpsResult CommandRecorder::UpdateRenderGraph(RenderGraph* render_graph) {
  auto back_buffers = AllocateArrayFrame<RpsRuntimeResource>(swapchain_.GetSwapchainBufferNum());
  for (uint32_t i = 0; i < swapchain_.GetSwapchainBufferNum(); i++) {
    back_buffers[i] = rpsD3D12ResourceToHandle(swapchain_.GetResource(i));
  }
  RpsResourceDesc back_buffer_desc = {
    .type              = RPS_RESOURCE_TYPE_IMAGE_2D,
    .temporalLayers    = swapchain_.GetSwapchainBufferNum(),
    .flags             = 0,
    .image = {
      .width       = swapchain_.GetWidth(),
      .height      = swapchain_.GetHeight(),
      .arrayLayers = 1,
      .mipLevels   = 1,
      .format      = rpsFormatFromDXGI(swapchain_.GetDxgiFormat()),
      .sampleCount = 1,
    },
  };
  const auto camera_buffer_size = GetUint32(sizeof(shader::SceneCameraData));
  RpsConstant args[] = {&back_buffer_desc, &frame_buffer_num_, &camera_buffer_size};
  const RpsRuntimeResource* arg_resources[] = {back_buffers};
  const auto gpu_completed_frame_index = (frame_count_ > frame_buffer_num_) ? static_cast<uint64_t>(frame_count_ - frame_buffer_num_) : RPS_GPU_COMPLETED_FRAME_INDEX_NONE;
  RpsRenderGraphUpdateInfo update_info = {
    .frameIndex               = frame_count_,
    .gpuCompletedFrameIndex   = gpu_completed_frame_index,
    .diagnosticFlags          = RpsDiagnosticFlags(RPS_DIAGNOSTIC_ENABLE_RUNTIME_DEBUG_NAMES | ((gpu_completed_frame_index == RPS_GPU_COMPLETED_FRAME_INDEX_NONE) ? RPS_DIAGNOSTIC_ENABLE_ALL: RPS_DIAGNOSTIC_NONE)),
    .numArgs                  = countof(args),
    .ppArgs                   = args,
    .ppArgResources           = arg_resources,
  };
  return rpsRenderGraphUpdate(render_graph->GetRenderGraph(), &update_info);
}
uint64_t CommandRecorder::TrasnferReservedResources() {
  auto copy_queue_signal = CommandQueueSignals::kInvalidSignalVal;
  if (IsResourceTransferReserved(frame_buffer_index_, &resource_transfer_)) {
    RecordTransferResourceCommand(GetCommandList(copy_queue_index_), frame_buffer_index_, &resource_transfer_);
    command_list_set_.ExecuteCommandList(copy_queue_index_);
    copy_queue_signal = command_queue_signals_.SucceedSignal(copy_queue_index_);
    assert(copy_queue_signal != CommandQueueSignals::kInvalidSignalVal);
  }
  NotifyTransferReservedResourcesProcessed(frame_buffer_index_, &resource_transfer_);
  return copy_queue_signal;
}
std::pair<uint32_t*, uint32_t*> CommandRecorder::GetFirstAndLastBatchIndexPerQueue(const RpsRenderGraphBatchLayout& batch_layout) {
  auto first_batch_index_per_queue = AllocateArrayFrame<uint32_t>(command_queue_num_);
  std::fill(first_batch_index_per_queue, first_batch_index_per_queue + command_queue_num_, ~0U);
  auto last_batch_index_per_queue = AllocateArrayFrame<uint32_t>(command_queue_num_);
  std::fill(last_batch_index_per_queue, last_batch_index_per_queue + command_queue_num_, ~0U);
  for (uint32_t batch_index = 0; batch_index < batch_layout.numCmdBatches; batch_index++) {
    if (first_batch_index_per_queue[batch_layout.pCmdBatches[batch_index].queueIndex] == ~0U) {
      first_batch_index_per_queue[batch_layout.pCmdBatches[batch_index].queueIndex] = batch_index;
    }
    last_batch_index_per_queue[batch_layout.pCmdBatches[batch_index].queueIndex] = batch_index;
  }
  return {first_batch_index_per_queue, last_batch_index_per_queue};
}
namespace {
auto MakeVec3(const float array[3]) {
  return gfxminimath::vec3(array[0], array[1], array[2]);
}
auto MakeVec4(const float array[4]) {
  return gfxminimath::vec4(array[0], array[1], array[2], array[3]);
}
auto MakeVec4(const float array[3], const float w) {
  return gfxminimath::vec4(array[0], array[1], array[2], w);
}
auto CalcLightVectorVs(const float light_direction[3], const gfxminimath::matrix& view_matrix) {
  auto light_direction_vs = gfxminimath::vec3(mul(view_matrix, MakeVec4(light_direction, 0.0f)));
  return normalize_vector(light_direction_vs);
}
void GetLightOriginLocationInScreenSpace(const gfxminimath::vec3& light_direction_vs, const gfxminimath::matrix& projection_matrix, const uint32_t primarybuffer_width, const uint32_t primarybuffer_height, int32_t light_origin_location_in_screen_space[2]) {
  using namespace gfxminimath;
  auto light_origin = mul(projection_matrix, append_w(light_direction_vs * 100000.0f, 1.0f));
  light_origin = (light_origin + 1.0f) * 0.5f;
  light_origin *= vec4(static_cast<float>(primarybuffer_width), static_cast<float>(primarybuffer_height), 0.0f, 0.0f);
  light_origin = round(light_origin);
  float tmp[4];
  light_origin.store(tmp);
  light_origin_location_in_screen_space[0] = static_cast<int32_t>(tmp[0]);
  light_origin_location_in_screen_space[1] = static_cast<int32_t>(tmp[1]);
}
auto GetAspectRatio(const Size2d& buffer_size) {
  return static_cast<float>(buffer_size.width) / buffer_size.height;
}
void GetCompactProjectionParam(const float fov_vertical_radian, const float aspect_ratio, const float z_near, const float z_far, float compact_projection_param[4]) {
  // https://shikihuiku.github.io/post/projection_matrix/
  // https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixperspectivefovlh
  /*m11*/ compact_projection_param[0] = 1.0f / std::tan(fov_vertical_radian * 0.5f);
  /*m00*/ compact_projection_param[1] = compact_projection_param[0] / aspect_ratio;
  /*m22*/ compact_projection_param[2] = z_far / (z_far - z_near);
  /*m23*/ compact_projection_param[3] = -z_near * compact_projection_param[2];
}
auto GetProjectionMatrix(const float compact_projection_param[4]) {
  return gfxminimath::matrix(compact_projection_param[1], 0.0f,  0.0f,  0.0f,
                             0.0f, compact_projection_param[0],  0.0f,  0.0f,
                             0.0f, 0.0f, compact_projection_param[2], compact_projection_param[3],
                             0.0f,  0.0f,  1.0f, 0.0f);
}
auto CalcViewMatrix(float camera_pos[3], float camera_focus[3]) {
  return gfxminimath::lookat_lh(MakeVec3(camera_pos), MakeVec3(camera_focus));
}
auto UpdateSceneData(const RpsCmdCallbackContext* context, ID3D12Resource* camera_data_resource, D3D12_CPU_DESCRIPTOR_HANDLE camera_data_handle) {
  using namespace gfxminimath;
  const auto render_pass_common_data = static_cast<RenderPassCommonData*>(context->pUserRecordContext);
  const auto scene_params = render_pass_common_data->scene_params;
  const auto view_matrix = CalcViewMatrix(scene_params->camera_pos, scene_params->camera_focus);
  float compact_projection_param[4];
  GetCompactProjectionParam(scene_params->fov_vertical * gfxminimath::kDegreeToRadian, GetAspectRatio(scene_params->primarybuffer_size), scene_params->near_z, scene_params->far_z, compact_projection_param);
  const auto projection_matrix = GetProjectionMatrix(compact_projection_param);
  const auto light_direction_vs = CalcLightVectorVs(scene_params->light_direction, view_matrix);
  int32_t light_origin_location_in_screen_space[2];
  GetLightOriginLocationInScreenSpace(light_direction_vs, projection_matrix, scene_params->primarybuffer_size.width, scene_params->primarybuffer_size.height, light_origin_location_in_screen_space);
  float light_direction_vs_array[3];
  to_array(light_direction_vs, light_direction_vs_array);
  float view_matrix_array[16];
  to_array_column_major(view_matrix, view_matrix_array);
  float projection_matrix_array[16];
  to_array_column_major(projection_matrix, projection_matrix_array);
  // const auto light_slope_zx = light_direction_vs[2] / light_direction_vs[0];
  // const auto far_div_near = scene_params->far_z / scene_params->near_z;
  {
    shader::SceneCameraData camera_data{};
    memcpy(&camera_data.view_matrix, view_matrix_array, sizeof(camera_data.view_matrix));
    memcpy(&camera_data.projection_matrix, projection_matrix_array, sizeof(camera_data.projection_matrix));
    auto camera_data_ptr = MapResource(camera_data_resource, sizeof(camera_data));
    memcpy(camera_data_ptr, &camera_data, sizeof(camera_data));
    UnmapResource(camera_data_resource);
  }
  {
    D3D12_CPU_DESCRIPTOR_HANDLE handles[] = {
      camera_data_handle,
      render_pass_common_data->model_data_set->mesh_views.transform_offset_handle,
      render_pass_common_data->model_data_set->mesh_views.transform_index_handle,
      render_pass_common_data->model_data_set->mesh_views.transform_handle,
    };
    render_pass_common_data->gpu_handles->geom_pass_gpu_handle = render_pass_common_data->util_funcs->write_to_gpu_handle(handles, countof(handles));
  }
}
auto GeomPass(const RpsCmdCallbackContext* context) {
  auto command_list = rpsD3D12CommandListFromHandle(context->hCommandBuffer);
  const auto& pass_params = static_cast<RenderGraph::GeomPassParams*>(context->pCmdCallbackContext);
  const auto render_pass_common_data = static_cast<RenderPassCommonData*>(context->pUserRecordContext);
  command_list->SetGraphicsRootSignature(GetMaterialRootsig(*render_pass_common_data->material_list, pass_params->material_index));
  command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  command_list->SetGraphicsRootDescriptorTable(1, render_pass_common_data->gpu_handles->geom_pass_gpu_handle);
  const auto& model_data = render_pass_common_data->model_data_set->model_data;
  command_list->IASetIndexBuffer(&render_pass_common_data->model_data_set->mesh_views.index_buffer_view);
  command_list->IASetVertexBuffers(0, kVertexBufferTypeNum, render_pass_common_data->model_data_set->mesh_views.vertex_buffer_view);
  uint32_t prev_variation_hash = 0;
  for (uint32_t i = 0; i < model_data.mesh_num; i++) {
    command_list->SetGraphicsRoot32BitConstant(0, i, 0);
    const auto& material_variation_hash = render_pass_common_data->model_data_set->material_data.material_variation_hash[model_data.material_setting_index[i]];
    if (prev_variation_hash != material_variation_hash) {
      auto variation_index = FindMaterialVariationIndex(*render_pass_common_data->material_list, pass_params->material_index, material_variation_hash);
      if (variation_index == MaterialList::kInvalidIndex) {
        logwarn("material variation not found. {} {}", i, material_variation_hash);
        variation_index = 0;
      }
      const auto pso_index = GetMaterialPsoIndex(*render_pass_common_data->material_list, pass_params->material_index, variation_index);
      command_list->SetPipelineState(render_pass_common_data->material_list->pso_list[pso_index]);
    }
    command_list->DrawIndexedInstanced(model_data.index_buffer_len[i],
                                       model_data.instance_num[i],
                                       model_data.index_buffer_offset[i],
                                       model_data.vertex_buffer_offset[i],
                                       0);
  }
}
auto RenderImgui(const RpsCmdCallbackContext* context) {
  ImGui::Render();
  auto command_list = rpsD3D12CommandListFromHandle(context->hCommandBuffer);
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), command_list);
}
void RecordDebugMarker([[maybe_unused]] void* context, const RpsRuntimeOpRecordDebugMarkerArgs* args) {
  auto* command_list = rpsD3D12CommandListFromHandle(args->hCommandBuffer);
  switch (args->mode)
  {
    case RPS_RUNTIME_DEBUG_MARKER_BEGIN:
      PIXBeginEvent(command_list, 0, args->text);
      break;
    case RPS_RUNTIME_DEBUG_MARKER_END:
      PIXEndEvent(command_list);
      break;
    case RPS_RUNTIME_DEBUG_MARKER_LABEL:
      PIXSetMarker(command_list, 0, args->text);
      break;
  }
}
void RpsLogger([[maybe_unused]]void* context, [[maybe_unused]]const char* format, ...) {
}
} // namespace
RPS_DECLARE_RPSL_ENTRY(default_rendergraph, rps_main)
std::unique_ptr<RenderGraph> RenderGraph::CreateRenderGraph(const Config& config) {
  return std::unique_ptr<RenderGraph>(new RenderGraph(config));
}
RenderGraph::RenderGraph(const Config& config) {
  {
    RpsDeviceCreateInfo create_info = {
      .allocator = {
        .pfnAlloc = AllocateRenderGraph,
        .pfnFree  = FreeRenderGraph,
      },
      .printer = {
        .pfnPrintf  = RpsLogger,
      },
    };
    RpsRuntimeDeviceCreateInfo runtime_create_info = {
      .pUserContext = this,
      .callbacks = {
        .pfnRecordDebugMarker = &RecordDebugMarker,
      },
    };
    RpsD3D12RuntimeDeviceCreateInfo runtime_device_create_info = {
      .pDeviceCreateInfo  = &create_info,
      .pRuntimeCreateInfo = &runtime_create_info,
      .pD3D12Device       = config.device,
      // .flags = RPS_D3D12_RUNTIME_FLAG_PREFER_ENHANCED_BARRIERS;
    };
    const auto result = rpsD3D12RuntimeDeviceCreate(&runtime_device_create_info, &rps_device_);
    assert(result == RPS_OK);
  }
  {
    auto queue_flags = AllocateArrayFrame<RpsQueueFlags>(config.command_queue_num);
    for (uint32_t i = 0; i < config.command_queue_num; i++) {
      auto flag = RPS_QUEUE_FLAG_GRAPHICS;
      const auto type = config.command_queue_type[i];
      if (type == D3D12_COMMAND_LIST_TYPE_COMPUTE) {
        flag = RPS_QUEUE_FLAG_COMPUTE;
      } else if (type == D3D12_COMMAND_LIST_TYPE_COPY) {
        flag = RPS_QUEUE_FLAG_COPY;
      }
      queue_flags[i] = flag;
    }
    RpsRenderGraphCreateInfo render_graph_create_info = {
      .scheduleInfo = {
        .numQueues = config.command_queue_num,
        .pQueueInfos = queue_flags,
      },
      .mainEntryCreateInfo = {
        .hRpslEntryPoint = RPS_ENTRY_REF(default_rendergraph, rps_main),
      },
    };
    const auto result = rpsRenderGraphCreate(rps_device_, &render_graph_create_info, &rps_render_graph_);
    assert(result == RPS_OK);
  }
  {
    auto rpsl_entry = rpsRenderGraphGetMainEntry(rps_render_graph_);
    {
      const auto result = rpsProgramBindNode(rpsl_entry, "update_scene_data", &UpdateSceneData);
      assert(result == RPS_OK);
    }
    {
      prez_pass_params_.material_index = FindHashIndex(config.material_hash_list_len, config.material_hash_list, CalcStrHash("prez"));
      const auto result = rpsProgramBindNode(rpsl_entry, "prez_pass", &GeomPass, &prez_pass_params_, RPS_CMD_CALLBACK_FLAG_NONE);
      assert(result == RPS_OK);
    }
    {
      const auto result = rpsProgramBindNode(rpsl_entry, "imgui", &RenderImgui);
      assert(result == RPS_OK);
    }
  }
}
RenderGraph::~RenderGraph() {
  rpsRenderGraphDestroy(rps_render_graph_);
  rpsDeviceDestroy(rps_device_);
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
  ReleaseMaterialViewDescriptorHeaps(&model_data_set.descriptor_heaps);
  ReleaseResourceSet(&default_textures.resource_set);
  default_textures.descriptor_heap_set.heap->Release();
  buffer_allocator->Release();
  device.Term();
  dxgi_core.Term();
  ClearAllAllocations();
}
TEST_CASE("scene viewer") { // NOLINT
  using namespace illuminate; // NOLINT
  auto [graphic_device, command_recorder] = SetupGraphicDevices(LoadJson("configs/config_default.json"));
  auto material_pack = BuildMaterialList(graphic_device->GetDevice(), LoadJson("configs/materials.json"));
  auto render_graph = SetupRenderGraph(graphic_device.get(), command_recorder.get(), material_pack);
  auto default_textures = LoadDefaultTextures(command_recorder->GetFrameBufferIndex(), graphic_device->GetDevice(), command_recorder->GetGpuBufferAllocator(), command_recorder->GetResourceTransferManager());
  auto model_data_set = LoadModelData("scenedata/BoomBoxWithAxes/BoomBoxWithAxes.json", default_textures.descriptor_heap_set, command_recorder->GetFrameBufferIndex(), graphic_device->GetDevice(), command_recorder->GetGpuBufferAllocator(), command_recorder->GetResourceTransferManager());
  SceneParams scene_params{};
  SceneData scene_data{
    .model_data_set = &model_data_set,
    .material_list  = &material_pack.material_list,
    .scene_params   = &scene_params,
  };
  for (uint32_t i = 0; i < 100; i++) {
    if (!command_recorder->ProcessWindowMessage()) { break; }
    command_recorder->PreUpdate();
    ResetAllocation(MemoryType::kFrame);
    command_recorder->RecordCommands(render_graph.get(), scene_data);
    command_recorder->Present();
  }
  command_recorder->WaitAll();
  ReleaseResourceSet(&model_data_set.resource_set);
  ReleaseMaterialViewDescriptorHeaps(&model_data_set.descriptor_heaps);
  ReleaseResourceSet(&default_textures.resource_set);
  default_textures.descriptor_heap_set.heap->Release();
  ReleasePsoAndRootsig(&material_pack.material_list);
  render_graph.reset();
  command_recorder.reset();
  graphic_device.reset();
  ClearAllAllocations();
}
