#include "d3d12_src_common.h"
namespace illuminate {
void SetD3d12Name(ID3D12Object* obj, const char* name) {
  if (obj == nullptr) {
    loginfo("obj:{} is null", name);
    return;
  }
  const auto len = GetUint32(strlen(name)) + 1;
  auto hr = obj->SetPrivateData(WKPDID_D3DDebugObjectName, len, name);
  if (FAILED(hr)) {
    logwarn("SetD3d12Name failed. {} {}", hr, name);
  }
}
void SetD3d12Name(ID3D12Object* obj, const std::string_view name) {
  SetD3d12Name(obj, name.data());
}
uint32_t GetD3d12Name(ID3D12Object* obj, const uint32_t dst_size, char* dst) {
  auto retval = dst_size;
  auto hr = obj->GetPrivateData(WKPDID_D3DDebugObjectName, &retval, dst);
  if (SUCCEEDED(hr)) {
    return retval;
  }
  loginfo("GetD3d12Name failed. {} {} {}", hr, retval, dst_size);
  return 0;
}
void CopyStrToWstrContainer(wchar_t** dst, const std::string_view src, const MemoryType memory_type) {
  const auto len = static_cast<uint32_t>(src.size());
  *dst = AllocateArray<wchar_t>(memory_type, len + 1);
  for (uint32_t i = 0; i < len; i++) {
    (*dst)[i] = src[i];
  }
  (*dst)[len] = 0;
}
}
#include "doctest/doctest.h"
TEST_CASE("CopyStrToWstrContainer") {
  using namespace illuminate; // NOLINT
  wchar_t* dst{};
  std::string str("hellow world");
  CopyStrToWstrContainer(&dst, str, MemoryType::kFrame);
  CHECK_NE(dst, nullptr);
  CHECK_EQ(wcscmp(dst, L"hellow world"), 0);
  wchar_t* dst2{};
  std::string str2("hellow world2");
  CopyStrToWstrContainer(&dst2, str2, MemoryType::kFrame);
  CHECK_NE(dst, nullptr);
  CHECK_EQ(wcscmp(dst, L"hellow world"), 0);
  CHECK_NE(dst, nullptr);
  CHECK_EQ(wcscmp(dst2, L"hellow world2"), 0);
}
#include "d3d12_device.h"
#include "d3d12_dxgi_core.h"
TEST_CASE("GetSetD3d12Name") {
  using namespace illuminate; // NOLINT
  DxgiCore dxgi_core;
  CHECK_UNARY(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK_UNARY(device.Init(dxgi_core.GetAdapter())); // NOLINT
  const auto device_name = "device_name";
  SetD3d12Name(device.Get(), device_name);
  const uint32_t num = 128;
  char dst[num]{};
  CHECK_EQ(GetD3d12Name(device.Get(), num, dst), strlen(device_name) + 1);
  CHECK_EQ(strcmp(dst, device_name), 0);
  device.Term();
  dxgi_core.Term();
}
