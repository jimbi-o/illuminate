#include "d3d12_scene_loader.h"
#include <fstream>
#include "d3d12_gpu_buffer_allocator.h"
#include "d3d12_memory_allocators.h"
#include "d3d12_resource_transfer.h"
#include "d3d12_src_common.h"
#include "d3d12_texture_util.h"
#include "shader/include/shader_defines.h"
namespace illuminate {
enum MeshResourceType {
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
struct ModelResources {
  D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
  D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view[kMeshResourceTypeNum]{};
  D3D12MA::Allocation* mesh_allocations[kMeshResourceTypeNum]{};
  ID3D12Resource* mesh_resources[kMeshResourceTypeNum]{};
};
struct MaterialSettings {
  StrHash* material_variation_hash{};
};
struct MaterialResources {
  D3D12MA::Allocation* material_allocation{};
  ID3D12Resource* material_resource{};
  D3D12MA::Allocation** texture_allocations{};
  ID3D12Resource** texture_resources{};
};
struct ModelDataSet {
  ModelData model_data{};
  ModelResources model_resources{};
  MaterialSettings material_settings{};
  MaterialResources material_resources{};
};
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
auto ParseModelResources(const nlohmann::json& json, const char* const filename, const uint32_t frame_index, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
  ModelResources model_resources{};
  const uint32_t binary_path_len = 128;
  char binary_path[binary_path_len];
  SwapFilename(filename, json.at("binary_filename").get<std::string>().c_str(), binary_path_len, binary_path);
  auto mesh_binary_buffer = ReadAllBytesFromFileToTemporaryBuffer(binary_path);
  // TODO check transform matrix column/row-major
  const auto& json_binary_info = json.at("binary_info");
  for (uint32_t mesh_resource_type = 0; mesh_resource_type < kMeshResourceTypeNum; mesh_resource_type++) {
    const char* mesh_type_name[] = {
      "transform",
      "index",
      "position",
      "normal",
      "tangent",
      "texcoord",
    };
    const auto& mesh_binary_info = json_binary_info.at(mesh_type_name[mesh_resource_type]);
    const auto buffer_size = mesh_binary_info.at("size_in_bytes").get<uint32_t>();
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
      const uint32_t buffer_name_len = 128;
      char buffer_name[buffer_name_len];
      snprintf(buffer_name, buffer_name_len, "U%s_%s", filename, mesh_type_name[mesh_resource_type]);
      SetD3d12Name(resource_upload, buffer_name);
      snprintf(buffer_name, buffer_name_len, "%s_%s", filename, mesh_type_name[mesh_resource_type]);
      SetD3d12Name(resource_default, buffer_name);
    }
    {
      // fill upload buffer
      auto dst = MapResource(resource_upload, buffer_size);
      const uint32_t offset_in_bytes = mesh_binary_info.at("offset_in_bytes");
      memcpy(dst, &mesh_binary_buffer[offset_in_bytes], buffer_size);
      UnmapResource(resource_upload);
    }
    {
      // fill views
      const auto addr = resource_default->GetGPUVirtualAddress();
      const uint32_t stride_in_bytes = mesh_binary_info.at("stride_in_bytes");
      switch (mesh_resource_type) {
        case kMeshResourceTypeTransform:
          break;
        case kMeshResourceTypeIndex:
          model_resources.index_buffer_view = {
            .BufferLocation = addr,
            .SizeInBytes    = buffer_size,
            .Format         = stride_in_bytes == 4 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT,
          };
          break;
        default:
          model_resources.vertex_buffer_view[ConvertToVertexBufferType(static_cast<MeshResourceType>(mesh_resource_type))] = {
            .BufferLocation = addr,
            .SizeInBytes    = buffer_size,
            .StrideInBytes  = stride_in_bytes,
          };
      }
    }
    ReserveResourceTransfer(frame_index, resource_upload, allocation_upload, resource_default, resource_transfer);
    model_resources.mesh_allocations[mesh_resource_type] = allocation_default;
    model_resources.mesh_resources[mesh_resource_type] = resource_default;
  }
  return model_resources;
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
  MaterialSettings material_settings{};
  MaterialResources material_resources{};
  const auto& json_materials = json_material_settings.at("materials");
  const uint32_t material_num = GetUint32(json_materials.size());
  material_settings.material_variation_hash = AllocateArrayScene<StrHash>(material_num);
  auto albedo_infos = AllocateArrayFrame<shader::AlbedoInfo>(material_num);
  auto misc_infos = AllocateArrayFrame<shader::MaterialMiscInfo>(material_num);
  for (uint32_t i = 0; i < material_num; i++) {
    const auto& json_material = json_materials[i];
    material_settings.material_variation_hash[i] = GetMaterialHash(json_material);
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
  return std::make_tuple(material_settings, albedo_infos, misc_infos);
}
  // TODO
  // const auto& json_textures = json_material_settings.at("textures");
  // const auto& json_samplers = json_material_settings.at("samplers");
  // model_data.texture_num = GetUint32(json_textures.size());
  // model_data.sampler_num = GetUint32(json_samplers.size());
  // D3D12_CPU_DESCRIPTOR_HANDLE cpu_handles[kSceneDescriptorHandleTypeNum]{};
  // ID3D12Resource** texture_resources{};
  // ID3D12DescriptorHeap* descriptor_heap{nullptr};
  // ID3D12DescriptorHeap* sampler_descriptor_heap{nullptr};
} // namespace
ModelDataSet LoadModelData(const char* const filename, const uint32_t frame_index, D3d12Device* device, D3D12MA::Allocator* buffer_allocator, ResourceTransfer* resource_transfer) {
  loginfo("loading {}", filename);
  auto json = LoadJson(filename);
  auto model_resources = ParseModelResources(json, filename, frame_index, buffer_allocator, resource_transfer);
  auto model_data = ParseModelData(json);
  const auto& json_material_settings = json.at("material_settings");
   auto [material_settings, material_albedo_infos, material_misc_infos] = ParseMaterials(json_material_settings);
  // return {model_data, model_resources, material_settings, material_resources};
  assert(false);
  return {};
}
void ReleaseModelResources(ModelResources* model_resources) {
  for (uint32_t i = 0; i < kMeshResourceTypeNum; i++) {
    model_resources->mesh_allocations[i]->Release();
    model_resources->mesh_resources[i]->Release();
  }
}
} // namespace illuminate
#include "doctest/doctest.h"
#include "d3d12_dxgi_core.h"
#include "d3d12_device.h"
TEST_CASE("scene loader") { // NOLINT
  using namespace illuminate; // NOLINT
  DxgiCore dxgi_core;
  dxgi_core.Init(); // NOLINT
  Device device;
  device.Init(dxgi_core.GetAdapter()); // NOLINT
  auto buffer_allocator = GetBufferAllocator(dxgi_core.GetAdapter(), device.Get());
  const uint32_t frame_num = 2;
  uint32_t frame_index = 0;
  auto resource_transfer = PrepareResourceTransferer(frame_num, 1024, 11);
  auto model_data_set = LoadModelData("scenedata/BoomBoxWithAxes/BoomBoxWithAxes.json", frame_index, device.Get(), buffer_allocator, &resource_transfer);
  ClearResourceTransfer(2, &resource_transfer);
  ReleaseModelResources(&model_data_set.model_resources);
  buffer_allocator->Release();
  device.Term();
  dxgi_core.Term();
  ClearAllAllocations();
}
