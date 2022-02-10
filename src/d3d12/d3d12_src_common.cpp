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
void CopyStrToWstrContainer(wchar_t** dst, const std::string_view src, MemoryAllocationJanitor* allocator) {
  const auto len = static_cast<uint32_t>(src.size());
  *dst = AllocateArray<wchar_t>(allocator, len + 1);
  for (uint32_t i = 0; i < len; i++) {
    (*dst)[i] = src[i];
  }
  (*dst)[len] = 0;
}
}
#include "doctest/doctest.h"
TEST_CASE("CopyStrToWstrContainer") {
  using namespace illuminate; // NOLINT
  auto allocator = GetTemporalMemoryAllocator();
  wchar_t* dst{};
  std::string str("hellow world");
  CopyStrToWstrContainer(&dst, str, &allocator);
  CHECK_NE(dst, nullptr);
  CHECK_EQ(wcscmp(dst, L"hellow world"), 0);
  wchar_t* dst2{};
  std::string str2("hellow world2");
  CopyStrToWstrContainer(&dst2, str2, &allocator);
  CHECK_NE(dst, nullptr);
  CHECK_EQ(wcscmp(dst, L"hellow world"), 0);
  CHECK_NE(dst, nullptr);
  CHECK_EQ(wcscmp(dst2, L"hellow world2"), 0);
}
