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
  constexpr auto GetRootsigIndex(const uint32_t pso_index) const {
    return pso_index_to_rootsig_index_map_[pso_index];
  }
  constexpr auto GetRootsig(const uint32_t pso_index) const {
    return rootsig_list_[GetRootsigIndex(pso_index)];
  }
  constexpr auto GetPso(const uint32_t pso_index) const {
    return pso_list_[pso_index];
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
  uint32_t* pso_index_to_rootsig_index_map_{nullptr};
};
uint32_t CreateMaterialStrHashList(const nlohmann::json& material_json, StrHash** hash_list_ptr, MemoryAllocationJanitor* allocator);
}
#endif
