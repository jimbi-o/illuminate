#include "d3d12_src_common.h"
namespace illuminate {
void SetD3d12Name(ID3D12Object* obj, const std::string& name) {
  if (obj == nullptr) {
    loginfo("obj:{} is null", name);
    return;
  }
  std::wstring wstr(name.begin(), name.end());
  auto hr = obj->SetName(wstr.c_str());
  if (FAILED(hr)) {
    logwarn("SetName failed. {} {}", hr, name);
  }
}
}
