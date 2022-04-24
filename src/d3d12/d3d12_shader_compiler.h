#ifndef ILLUMINATE_D3D12_SHADER_COMPILER_H
#define ILLUMINATE_D3D12_SHADER_COMPILER_H
#include "d3d12_header_common.h"
#include "d3d12_memory_allocators.h"
#include "illuminate/core/strid.h"
namespace illuminate {
class PsoRootsigManager {
 public:
  bool Init(const nlohmann::json& material_json, D3d12Device* device, MemoryAllocationJanitor* allocator);
  void Term();
  constexpr auto GetMaterialRootsigIndex(const uint32_t material) const {
    return material;
  }
  constexpr auto GetMaterialPsoIndex(const uint32_t material, const uint32_t variation_index) const {
    return material_pso_offset_[material] + variation_index;
  }
  constexpr auto GetMaterialRootsig(const uint32_t material) const {
    return rootsig_list_[GetMaterialRootsigIndex(material)];
  }
  constexpr auto GetMaterialPso(const uint32_t material) const {
    return pso_list_[GetMaterialPsoIndex(material, 0)];
  }
  constexpr auto GetMaterialPso(const uint32_t material, const uint32_t variation_index) const {
    return pso_list_[GetMaterialPsoIndex(material, variation_index)];
  }
 private:
  HMODULE library_{nullptr};
  IDxcCompiler3* compiler_{nullptr};
  IDxcUtils* utils_{nullptr};
  IDxcIncludeHandler* include_handler_{nullptr};
  uint32_t rootsig_num_{0};
  ID3D12RootSignature** rootsig_list_{nullptr};
  uint32_t pso_num_{0};
  ID3D12PipelineState** pso_list_{nullptr};
  uint32_t* material_pso_offset_{nullptr};
};
struct MaterialVariationMap {
  uint32_t material_num{0};
  StrHash* material_hash_list{nullptr};
  uint32_t* variation_hash_list_len{nullptr};
  StrHash** variation_hash_list{nullptr};
};
MaterialVariationMap CreateMaterialVariationMap(const nlohmann::json& material_json, MemoryAllocationJanitor* allocator);
uint32_t GetMaterialVariationIndex(const uint32_t material, const StrHash& variation, const MaterialVariationMap& material_variation_map);
}
#endif
