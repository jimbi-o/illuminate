#ifndef ILLUMINATE_D3D12_DXGI_CORE_H
#define ILLUMINATE_D3D12_DXGI_CORE_H
#include "d3d12_header_common.h"
namespace illuminate {
class DxgiCore {
 public:
  enum class AdapterFlag : uint8_t { kHighPerformance, kWarp, };
  bool Init(const AdapterFlag& adapter_flag = AdapterFlag::kHighPerformance);
  void Term();
  DxgiFactory* GetFactory() { return factory_; }
  DxgiAdapter* GetAdapter() { return adapter_; }
  bool IsTearingAllowed();
 private:
  HMODULE library_ = nullptr;
  DxgiFactory* factory_ = nullptr;
  DxgiAdapter* adapter_ = nullptr;
};
}
#endif
