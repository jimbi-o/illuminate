#include "d3d12_texture_util.h"
#include <memory>
#include "DDSTextureLoader.h"
#include "d3dx12.h"
#include "d3d12_src_common.h"
namespace illuminate {
} // namespace illuminate
#include "doctest/doctest.h"
#include "d3d12_dxgi_core.h"
#include "d3d12_device.h"
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
TEST_CASE("create white texture") { // NOLINT
  // ../tools/texconv.exe -nologo -fl 12.1 -keepcoverage 0.75 -sepalpha -if CUBIC -f BC7_UNORM -y ../tmp/white.png
  using namespace illuminate; // NOLINT
  DxgiCore dxgi_core;
  dxgi_core.Init(); // NOLINT
  Device device;
  device.Init(dxgi_core.GetAdapter()); // NOLINT
  ID3D12Resource* resource{};
  std::unique_ptr<std::uint8_t[]> dds_data;
  std::vector<D3D12_SUBRESOURCE_DATA> subresources;
  auto hr = DirectX::LoadDDSTextureFromFile(device.Get(), L"textures/white.dds", &resource, dds_data, subresources);
  CHECK_UNARY(SUCCEEDED(hr));
  CHECK_NE(resource, nullptr);
  resource->Release();
  device.Term();
  dxgi_core.Term();
  gSystemMemoryAllocator->Reset();
}
TEST_CASE("texture util") { // NOLINT
  using namespace illuminate; // NOLINT
  DxgiCore dxgi_core;
  dxgi_core.Init(); // NOLINT
  Device device;
  device.Init(dxgi_core.GetAdapter()); // NOLINT
  CommandListSet command_list_set;
  {
    const auto type = D3D12_COMMAND_LIST_TYPE_COPY;
    const auto priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    const uint32_t command_list_num_per_queue[] = {0,0,1};
    const uint32_t command_allocator_num_per_queue_type[] = {0,0,1};
    command_list_set.Init(device.Get(), 1, &type, &priority, command_list_num_per_queue, 2, command_allocator_num_per_queue_type);
  }
  CommandQueueSignals command_queue_signals;
  uint64_t signal_val_list[] = {0UL};
  command_queue_signals.Init(device.Get(), 1, command_list_set.GetCommandQueueList());
  {
    // main process
  }
  command_queue_signals.WaitOnCpu(device.Get(), signal_val_list);
  command_queue_signals.WaitAll(device.Get());
  command_queue_signals.Term();
  command_list_set.Term();
  device.Term();
  dxgi_core.Term();
  gSystemMemoryAllocator->Reset();
}
