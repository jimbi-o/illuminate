#ifndef ILLUMINATE_D3D12_SHADER_COMPILER_H
#define ILLUMINATE_D3D12_SHADER_COMPILER_H
#include "d3d12_header_common.h"
#include "d3d12_memory_allocators.h"
#include "illuminate/core/strid.h"
namespace illuminate {
struct MaterialList {
  static const uint32_t kInvalidIndex = ~0U;
  uint32_t material_num{0};
  StrHash* material_hash_list{nullptr};
  uint32_t* variation_hash_list_len{nullptr};
  StrHash** variation_hash_list{nullptr};
  uint32_t rootsig_num{0};
  ID3D12RootSignature** rootsig_list{nullptr};
  uint32_t pso_num{0};
  ID3D12PipelineState** pso_list{nullptr};
  uint32_t* material_pso_offset{nullptr};
  uint32_t* vertex_buffer_type_flags{nullptr}; // aligned with pso_list
};
class ShaderCompiler {
 public:
  bool Init();
  void Term();
  MaterialList BuildMaterials(const nlohmann::json& material_json, D3d12Device* device, MemoryAllocationJanitor* allocator);
 private:
  HMODULE library_{nullptr};
  IDxcCompiler3* compiler_{nullptr};
  IDxcUtils* utils_{nullptr};
  IDxcIncludeHandler* include_handler_{nullptr};
};
constexpr auto GetMaterialRootsigIndex([[maybe_unused]]const MaterialList& material_list, const uint32_t material) {
  return material;
}
constexpr auto GetMaterialPsoIndex(const MaterialList& material_list, const uint32_t material, const uint32_t variation_index) {
  return material_list.material_pso_offset[material] + variation_index;
}
constexpr auto GetMaterialRootsig(const MaterialList& material_list, const uint32_t material) {
  return material_list.rootsig_list[GetMaterialRootsigIndex(material_list, material)];
}
constexpr auto GetMaterialPso(const MaterialList& material_list, const uint32_t material) {
  return material_list.pso_list[GetMaterialPsoIndex(material_list, material, 0)];
}
constexpr auto GetMaterialPso(const MaterialList& material_list, const uint32_t material, const uint32_t variation_index) {
  return material_list.pso_list[GetMaterialPsoIndex(material_list, material, variation_index)];
}
uint32_t FindMaterialVariationIndex(const MaterialList& material_list, const uint32_t material, const StrHash variation_hash);
void ReleasePsoAndRootsig(MaterialList*);
}
#endif
