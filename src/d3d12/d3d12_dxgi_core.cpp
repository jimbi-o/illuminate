#include "d3d12_dxgi_core.h"
#include "d3d12_src_common.h"
#include "../win32_dll_util_macro.h"
#ifndef NDEBUG
#include "dxgidebug.h"
#endif
namespace illuminate {
bool DxgiCore::Init(const AdapterFlag& adapter_flag) {
  SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  library_ = LoadLibrary("Dxgi.dll");
  assert(library_ && "failed to load Dxgi.dll");
  auto hr = CALL_DLL_FUNCTION(library_, CreateDXGIFactory2)(0, IID_PPV_ARGS(&factory_));
  assert(SUCCEEDED(hr) && factory_ && "CreateDXGIFactory2 failed");
  if (adapter_flag == AdapterFlag::kWarp) {
    hr = factory_->EnumWarpAdapter(IID_PPV_ARGS(&adapter_));
  } else {
    hr = factory_->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter_));
  }
  assert(SUCCEEDED(hr) && adapter_ && "EnumAdapter failed");
  {
    DXGI_ADAPTER_DESC1 desc;
    adapter_->GetDesc1(&desc);
    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
      loginfo("IDXGIAdapter is software");
    }
  }
  return true;
};
void DxgiCore::Term() {
  auto refval = adapter_->Release();
  if (refval != 0UL) {
    logerror("adapter reference left. {}", refval);
  }
  refval = factory_->Release();
  if (refval != 0UL) {
    logerror("factory reference left. {}", refval);
  }
#ifndef NDEBUG
  IDXGIDebug1* debug = nullptr;
  auto hr = CALL_DLL_FUNCTION(library_, DXGIGetDebugInterface1)(0, IID_PPV_ARGS(&debug));
  if (FAILED(hr)) {
    logwarn("DXGIGetDebugInterface failed. {}", hr);
    return;
  }
  debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
  debug->Release();
#endif
  FreeLibrary(library_);
}
bool DxgiCore::IsTearingAllowed() {
  BOOL tearing = FALSE;
  auto hr = factory_->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing));
  if (FAILED(hr)) {
    logwarn("CheckFeatureSupport(tearing) failed. {}", hr);
    return false;
  }
  return tearing;
}
}
#include "doctest/doctest.h"
TEST_CASE("dxgi core") { // NOLINT
  using namespace illuminate; // NOLINT
  DxgiCore core;
  CHECK_UNARY(core.Init(DxgiCore::AdapterFlag::kHighPerformance)); // NOLINT
  CHECK_UNARY(core.IsTearingAllowed()); // NOLINT
  core.Term();
  CHECK_UNARY(core.Init(DxgiCore::AdapterFlag::kWarp)); // NOLINT
  CHECK_UNARY(core.IsTearingAllowed()); // NOLINT
  core.Term();
}
