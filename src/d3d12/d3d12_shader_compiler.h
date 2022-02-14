#ifndef ILLUMINATE_D3D12_SHADER_COMPILER_H
#define ILLUMINATE_D3D12_SHADER_COMPILER_H
#include "d3d12_header_common.h"
#include "d3d12_memory_allocators.h"
#include "illuminate/util/hash_map.h"
namespace illuminate {
class PsoRootsigManager {
 public:
  bool Init(const nlohmann::json& material_json, const char** shader_code_list, D3d12Device* device, MemoryAllocationJanitor* allocator);
  void Term();
  void FindMaterial(const std::string_view material_name, ID3D12RootSignature** rootsig, ID3D12PipelineState** pso) {
    const auto hash = CalcStrHash(material_name.data());
    *rootsig = rootsig_list_[*rootsig_index_.Get(hash)];
    *pso = pso_list_[*pso_index_.Get(hash)];
  }
 private:
  HMODULE library_{nullptr};
  IDxcCompiler3* compiler_{nullptr};
  IDxcUtils* utils_{nullptr};
  IDxcIncludeHandler* include_handler_{nullptr};
  HashMap<uint32_t, MemoryAllocationJanitor> rootsig_index_;
  HashMap<uint32_t, MemoryAllocationJanitor> pso_index_;
  uint32_t rootsig_num_{0};
  uint32_t pso_num_{0};
  ID3D12RootSignature** rootsig_list_{nullptr};
  ID3D12PipelineState** pso_list_{nullptr};
};
}
#endif
