#ifndef ILLUMINATE_D3D12_MODEL_MATERIAL_INFO_H
#define ILLUMINATE_D3D12_MODEL_MATERIAL_INFO_H
#include "d3d12_header_common.h"
#include "d3d12_memory_allocators.h"
namespace illuminate {
struct MaterialCategoryList {
  uint32_t* rootsig{nullptr};
  uint32_t** pso{nullptr};
};
void CreateMaterialCategoryList(const nlohmann::json&, const nlohmann::json&, MemoryAllocationJanitor* allocator, MaterialCategoryList* material_category_list);
}
#endif
