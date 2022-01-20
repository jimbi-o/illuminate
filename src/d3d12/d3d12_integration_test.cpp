#include "d3d12_header_common.h"
#include "illuminate/util/hash_map.h"
#include "d3d12_render_graph.h"
#include "d3d12_src_common.h"
namespace illuminate {
ID3D12DescriptorHeap* CreateDescriptorHeap(D3d12Device* const device, const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t descriptor_num, const D3D12_DESCRIPTOR_HEAP_FLAGS flags) {
  D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc = {
    .Type = type,
    .NumDescriptors = descriptor_num,
    .Flags = flags,
    .NodeMask = 0,
  };
  ID3D12DescriptorHeap* descriptor_heap{nullptr};
  auto hr = device->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&descriptor_heap));
  if (FAILED(hr)) {
    logerror("CreateDescriptorHeap failed. {} {} {} {}", hr, type, descriptor_num, flags);
    assert(false && "CreateDescriptorHeap failed");
    return nullptr;
  }
  return descriptor_heap;
}
template <typename A>
class DescriptorCpu {
 public:
  bool Init(D3d12Device* const device, const uint32_t* descriptor_handle_num_per_view_type_or_sampler, A* allocator) {
    for (uint32_t i = 0; i < kViewTypeNum; i++) {
      handles_[i].SetAllocator(allocator);
      total_handle_num_[GetViewTypeIndex(static_cast<ViewType>(i))] += descriptor_handle_num_per_view_type_or_sampler[i];
    }
    for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
      if (total_handle_num_[i] == 0) { continue; }
      const auto type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i);
      handle_increment_size_[i] = device->GetDescriptorHandleIncrementSize(type);
      descriptor_heap_[i] = CreateDescriptorHeap(device, type, total_handle_num_[i], D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
      if (descriptor_heap_[i] == nullptr) {
        assert(false && "CreateDescriptorHeap failed");
        for (uint32_t j = 0; j < i; j++) {
          descriptor_heap_[j]->Release();
          descriptor_heap_[j] = nullptr;
        }
        return false;
      }
      heap_start_[i] = descriptor_heap_[i]->GetCPUDescriptorHandleForHeapStart().ptr;
      handle_num_[i] = 0;
    }
    return true;
  }
  void Term() {
    for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; i++) {
      if (descriptor_heap_[i]) {
        auto refval = descriptor_heap_[i]->Release();
        assert(refval == 0);
      }
    }
  }
  const D3D12_CPU_DESCRIPTOR_HANDLE* GetHandle(const StrHash& name, const ViewType type) {
    auto view_type_index = static_cast<uint32_t>(type);
    auto ptr = handles_[view_type_index].Get(name);
    if (ptr != nullptr) { return ptr; }
    auto index = GetViewTypeIndex(type);
    assert(handle_num_[index] <= total_handle_num_[index] && "handle num exceeded handle pool size");
    D3D12_CPU_DESCRIPTOR_HANDLE handle{};
    handle.ptr = heap_start_[index] + handle_num_[index] * handle_increment_size_[index];
    if (!handles_[view_type_index].Insert(name, std::move(handle))) {
      assert(false && "failed to insert handle to map, increase map table size");
      return nullptr;
    }
    handle_num_[index]++;
    return handles_[view_type_index].Get(name);
  }
  const D3D12_CPU_DESCRIPTOR_HANDLE* GetHandle(const StrHash& name, const ViewType type) const {
    auto view_type_index = static_cast<uint32_t>(type);
    return handles_[view_type_index].Get(name);
  }
 private:
  static constexpr auto GetViewTypeIndex(const ViewType& type) {
    switch (type) {
      case ViewType::kCbv: { return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; }
      case ViewType::kSrv: { return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; }
      case ViewType::kUav: { return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; }
      case ViewType::kSampler: { return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER; }
      case ViewType::kRtv: { return D3D12_DESCRIPTOR_HEAP_TYPE_RTV; }
      case ViewType::kDsv: { return D3D12_DESCRIPTOR_HEAP_TYPE_DSV; }
    }
    assert(false && "invalid ViewType");
    return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  }
  HashMap<D3D12_CPU_DESCRIPTOR_HANDLE, A> handles_[kViewTypeNum];
  ID3D12DescriptorHeap* descriptor_heap_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};
  uint32_t total_handle_num_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};
  uint32_t handle_num_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};
  uint32_t handle_increment_size_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};
  uint64_t heap_start_[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};
};
class DescriptorGpu {
 public:
  static auto InitDescriptorHeapSetGpu(D3d12Device* const device, const D3D12_DESCRIPTOR_HEAP_TYPE type, const uint32_t handle_num) {
    DescriptorHeapSetGpu descriptor_heap;
    descriptor_heap.total_handle_num = handle_num;
    descriptor_heap.current_handle_num = 0;
    descriptor_heap.handle_increment_size = device->GetDescriptorHandleIncrementSize(type);
    descriptor_heap.descriptor_heap = CreateDescriptorHeap(device, type, handle_num, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    descriptor_heap.heap_start_cpu = descriptor_heap.descriptor_heap->GetCPUDescriptorHandleForHeapStart().ptr;
    descriptor_heap.heap_start_gpu = descriptor_heap.descriptor_heap->GetGPUDescriptorHandleForHeapStart().ptr;
    return descriptor_heap;
  }
  static const uint32_t kHandleNumCbvSrvUav = 1024;
  static const uint32_t kHandleNumSampler = 128;
  bool Init(D3d12Device* device) {
    descriptor_cbv_srv_uav_ = InitDescriptorHeapSetGpu(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kHandleNumCbvSrvUav);
    if (descriptor_cbv_srv_uav_.descriptor_heap == nullptr) {
      return false;
    }
    descriptor_sampler_ = InitDescriptorHeapSetGpu(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, kHandleNumSampler);
    return descriptor_sampler_.descriptor_heap != nullptr;
  }
  void Term() {
    {
      auto refval = descriptor_cbv_srv_uav_.descriptor_heap->Release();
      if (refval > 0) {
        logerror("descriptor_cbv_srv_uav_ descriptor_heap reference left: {}", refval);
        assert(false && "");
      }
    }
    {
      auto refval = descriptor_sampler_.descriptor_heap->Release();
      if (refval > 0) {
        logerror("descriptor_sampler_ descriptor_heap reference left: {}", refval);
        assert(false && "");
      }
    }
  }
  template <typename AllocatorCpu>
  auto CopyDescriptors(D3d12Device* device, const uint32_t buffer_num, const RenderPassBuffer* buffers, const DescriptorCpu<AllocatorCpu>& descriptor_cpu) {
    auto tmp_allocator = GetTemporalMemoryAllocator();
    auto buffer_is_view = AllocateArray<bool>(&tmp_allocator, buffer_num);
    uint32_t view_num = 0;
    for (uint32_t i = 0; i < buffer_num; i++) {
      switch (buffers[i].state) {
        case ViewType::kCbv:
        case ViewType::kSrv:
        case ViewType::kUav: {
          buffer_is_view[i] = true;
          view_num++;
          break;
        }
        default: {
          buffer_is_view[i] = false;
          break;
        }
      }
    }
    if (view_num == 0) {
      return D3D12_GPU_DESCRIPTOR_HANDLE{};
    }
    auto handle_num = AllocateArray<uint32_t>(&tmp_allocator, view_num);
    auto handles = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(&tmp_allocator, view_num);
    uint32_t view_index = 0;
    for (uint32_t i = 0; i < buffer_num; i++) {
      if (!buffer_is_view[i]) { continue; }
      auto& buffer = buffers[i];
      auto handle = descriptor_cpu.GetHandle(buffer.buffer_name, buffer.state);
      if (handle == nullptr) { continue; }
      if (i > 0 && handles[i - 1].ptr + descriptor_cbv_srv_uav_.handle_increment_size == handle->ptr) {
        handle_num[view_index]++;
        continue;
      }
      handle_num[view_index] = 1;
      handles[view_index].ptr = handle->ptr;
      view_index++;
    }
    assert(view_index > 0 && "buffer view count bug");
    if (descriptor_cbv_srv_uav_.current_handle_num + view_num >= descriptor_cbv_srv_uav_.total_handle_num) {
      descriptor_cbv_srv_uav_.current_handle_num = 0;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE dst_handle{descriptor_cbv_srv_uav_.heap_start_cpu + descriptor_cbv_srv_uav_.current_handle_num * descriptor_cbv_srv_uav_.handle_increment_size};
    if (view_index == 1) {
      assert(handle_num[0] == view_num);
      device->CopyDescriptorsSimple(view_num, dst_handle, handles[0], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    } else {
#ifdef BUILD_WITH_TEST
      uint32_t handle_num_debug = 0;
      for (uint32_t i = 0; i < view_index; i++) {
        handle_num_debug += handle_num[i];
      }
      assert(handle_num_debug == view_num);
#endif
      device->CopyDescriptors(1, &dst_handle, &view_num, view_index, handles, handle_num, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{descriptor_cbv_srv_uav_.heap_start_gpu + descriptor_cbv_srv_uav_.current_handle_num * descriptor_cbv_srv_uav_.handle_increment_size};
    descriptor_cbv_srv_uav_.current_handle_num += view_num;
    return gpu_handle;
  }
  auto GetOffsetView() const { return descriptor_cbv_srv_uav_.current_handle_num; }
  auto GetMaxOffsetView() const { return descriptor_cbv_srv_uav_.total_handle_num; }
 private:
  struct DescriptorHeapSetGpu {
    uint32_t handle_increment_size{};
    uint32_t total_handle_num{0};
    uint32_t current_handle_num{0};
    ID3D12DescriptorHeap* descriptor_heap{nullptr};
    uint64_t heap_start_cpu{};
    uint64_t heap_start_gpu{};
  };
  DescriptorHeapSetGpu descriptor_cbv_srv_uav_;
  DescriptorHeapSetGpu descriptor_sampler_;
};
auto GetUavDimension(const D3D12_RESOURCE_DIMENSION dimension, const uint16_t depth_or_array_size) {
  switch (dimension) {
    case D3D12_RESOURCE_DIMENSION_UNKNOWN: {
      assert(false && "D3D12_RESOURCE_DIMENSION_UNKNOWN selected");
      return D3D12_UAV_DIMENSION_UNKNOWN;
    }
    case D3D12_RESOURCE_DIMENSION_BUFFER: {
      return D3D12_UAV_DIMENSION_BUFFER;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D: {
      if (depth_or_array_size > 1) {
        return D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
      }
      return D3D12_UAV_DIMENSION_TEXTURE1D;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D: {
      if (depth_or_array_size > 1) {
        return D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
      }
      return D3D12_UAV_DIMENSION_TEXTURE2D;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D: {
      return D3D12_UAV_DIMENSION_TEXTURE3D;
    }
  }
  logerror("GetUavDimension invalid dimension {}", dimension);
  assert(false && "invalid dimension");
  return D3D12_UAV_DIMENSION_UNKNOWN;
}
auto GetSrvDimension(const D3D12_RESOURCE_DIMENSION dimension, const uint16_t depth_or_array_size) {
  switch (dimension) {
    case D3D12_RESOURCE_DIMENSION_UNKNOWN: {
      assert(false && "D3D12_RESOURCE_DIMENSION_UNKNOWN selected");
      return D3D12_SRV_DIMENSION_UNKNOWN;
    }
    case D3D12_RESOURCE_DIMENSION_BUFFER: {
      return D3D12_SRV_DIMENSION_BUFFER;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D: {
      if (depth_or_array_size > 1) {
        return D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
      }
      return D3D12_SRV_DIMENSION_TEXTURE1D;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D: {
      if (depth_or_array_size > 1) {
        return D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
      }
      return D3D12_SRV_DIMENSION_TEXTURE2D;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D: {
      return D3D12_SRV_DIMENSION_TEXTURE3D;
    }
  }
  // cubemap?
  logerror("GetSrvDimension invalid dimension {}", dimension);
  assert(false && "invalid dimension");
  return D3D12_SRV_DIMENSION_UNKNOWN;
}
bool CreateView(D3d12Device* device, const ViewType view_type, const BufferConfig& config, ID3D12Resource* resource, const D3D12_CPU_DESCRIPTOR_HANDLE* handle) {
  switch (view_type) {
    case ViewType::kCbv: {
      assert(false && "CreateView cbv not implemented");
      return false;
    }
    case ViewType::kSrv: {
      auto desc = D3D12_SHADER_RESOURCE_VIEW_DESC{
        .Format = config.format,
        .ViewDimension = GetSrvDimension(config.dimension, config.depth_or_array_size),
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
      };
      switch (desc.ViewDimension) {
        case D3D12_SRV_DIMENSION_UNKNOWN: {
          assert(false && "invalid srv view dimension");
          break;
        }
        case D3D12_SRV_DIMENSION_BUFFER: {
          assert(false && "D3D12_SRV_DIMENSION_BUFFER not implemented");
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURE1D: {
          desc.Texture1D = {
            .MostDetailedMip = 0,
            .MipLevels = ~0U,
            .ResourceMinLODClamp = 0.0f,
          };
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURE1DARRAY: {
          desc.Texture1DArray = {
            .MostDetailedMip = 0,
            .MipLevels = ~0U,
            .FirstArraySlice = 0,
            .ArraySize = config.depth_or_array_size,
            .ResourceMinLODClamp = 0.0f,
          };
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURE2D: {
          desc.Texture2D = {
            .MostDetailedMip = 0,
            .MipLevels = ~0U,
            .PlaneSlice = 0,
            .ResourceMinLODClamp = 0.0f,
          };
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURE2DARRAY: {
          desc.Texture2DArray = {
            .MostDetailedMip = 0,
            .MipLevels = ~0U,
            .FirstArraySlice = 0,
            .ArraySize = config.depth_or_array_size,
            .PlaneSlice = 0,
            .ResourceMinLODClamp = 0.0f,
          };
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURE2DMS: {
          desc.Texture2DMS = {};
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY: {
          desc.Texture2DMSArray = {};
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURE3D: {
          desc.Texture3D = {
            .MostDetailedMip = 0,
            .MipLevels = ~0U,
            .ResourceMinLODClamp = 0.0f,
          };
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURECUBE: {
          desc.TextureCube = {
            .MostDetailedMip = 0,
            .MipLevels = ~0U,
            .ResourceMinLODClamp = 0.0f,
          };
          break;
        }
        case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY: {
          desc.TextureCubeArray = {
            .MostDetailedMip = 0,
            .MipLevels = ~0U,
            .First2DArrayFace = 0,
            .NumCubes = config.depth_or_array_size,
            .ResourceMinLODClamp = 0.0f,
          };
          break;
        }
        case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE: {
          desc.RaytracingAccelerationStructure = {
            .Location = resource->GetGPUVirtualAddress(),
          };
          break;
        }
      }
      device->CreateShaderResourceView(resource, &desc, *handle);
      return true;
    }
    case ViewType::kUav: {
      auto desc = D3D12_UNORDERED_ACCESS_VIEW_DESC{
        .Format = config.format,
        .ViewDimension = GetUavDimension(config.dimension, config.depth_or_array_size),
      };
      switch (desc.ViewDimension) {
        case D3D12_UAV_DIMENSION_UNKNOWN: {
          assert(false && "invalid uav view dimension");
          break;
        }
        case D3D12_UAV_DIMENSION_BUFFER: {
          assert(false && "D3D12_UAV_DIMENSION_BUFFER not implemented");
          desc.Buffer = {
          };
          break;
        }
        case D3D12_UAV_DIMENSION_TEXTURE1D: {
          desc.Texture1D = {
            .MipSlice = 0,
          };
          break;
        }
        case D3D12_UAV_DIMENSION_TEXTURE1DARRAY: {
          desc.Texture1DArray = {
            .MipSlice = 0,
            .FirstArraySlice = 0,
            .ArraySize = config.depth_or_array_size,
          };
          break;
        }
        case D3D12_UAV_DIMENSION_TEXTURE2D: {
          desc.Texture2D = {
            .MipSlice = 0,
            .PlaneSlice = 0,
          };
          break;
        }
        case D3D12_UAV_DIMENSION_TEXTURE2DARRAY: {
          desc.Texture2DArray = {
            .MipSlice = 0,
            .FirstArraySlice = 0,
            .ArraySize = config.depth_or_array_size,
            .PlaneSlice = 0,
          };
          break;
        }
        case D3D12_UAV_DIMENSION_TEXTURE3D: {
          desc.Texture3D = {
            .MipSlice = 0,
            .FirstWSlice = 0,
            .WSize = config.depth_or_array_size,
          };
          break;
        }
      }
      device->CreateUnorderedAccessView(resource, nullptr, &desc, *handle);
      return true;
    }
    case ViewType::kSampler: {
      assert(false && "CreateView sampler not implemented");
      return false;
    }
    case ViewType::kRtv: {
      assert(false && "CreateView rtv not implemented");
      return false;
    }
    case ViewType::kDsv: {
      assert(false && "CreateView dsv not implemented");
      return false;
    }
  }
  assert(false && "CreateView invalid view type");
  return false;
}
}
#include "D3D12MemAlloc.h"
#include "doctest/doctest.h"
#include <nlohmann/json.hpp>
#include "d3d12_command_list.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"
#include "d3d12_dxgi_core.h"
#include "d3d12_render_graph_json_parser.h"
#include "d3d12_swapchain.h"
#include "d3d12_win32_window.h"
namespace illuminate {
namespace {
auto GetTestJson() {
  return R"(
{
  "frame_buffer_num": 2,
  "frame_loop_num": 5,
  "window": {
    "title": "integration test",
    "width": 500,
    "height" : 300
  },
  "command_queue": [
    {
      "name": "queue_graphics",
      "type": "3d",
      "priority": "normal",
      "command_list_num": 1
    },
    {
      "name": "queue_compute",
      "type": "compute",
      "priority": "normal",
      "command_list_num": 1
    }
  ],
  "command_allocator": {
    "direct": 1,
    "compute": 1
  },
  "swapchain": {
    "command_queue": "queue_graphics",
    "format": "R8G8B8A8_UNORM",
    "usage": ["RENDER_TARGET_OUTPUT"]
  },
  "buffer": [
    {
      "name": "uav",
      "format": "R8G8B8A8_UNORM",
      "heap_type": "default",
      "dimension": "texture2d",
      "width": 100,
      "height": 20,
      "depth_or_array_size": 1,
      "miplevels": 1,
      "format": "R8G8B8A8_UNORM",
      "sample_count": 1,
      "sample_quality": 0,
      "layout": "unknown",
      "flags": ["uav"],
      "mip_width": 0,
      "mip_height": 0,
      "mip_depth": 0,
      "initial_state": "uav",
      "clear_color": [1,0,0,1],
      "descriptor_type": ["uav", "srv"]
    }
  ],
  "render_pass": [
    {
      "name": "clear uav",
      "command_queue": "queue_compute",
      "buffers": [
        {
          "name": "uav",
          "state": "uav"
        }
      ],
      "pass_vars": {
        "clear_color": [1, 0, 0, 1]
      },
      "prepass_barrier": [
        {
          "buffer_name": "uav",
          "type": "uav"
        }
      ],
      "postpass_barrier": [
        {
          "buffer_name": "uav",
          "type": "uav"
        }
      ]
    },
    {
      "name": "output to swapchain",
      "command_queue": "queue_graphics",
      "buffers": [
        {
          "name": "swapchain",
          "state": "rtv"
        }
      ],
      "pass_vars": {
        "clear_color": [0, 1, 0, 1]
      },
      "prepass_barrier": [
        {
          "buffer_name": "swapchain",
          "type": "transition",
          "split_type": "none",
          "state_before": "present",
          "state_after": "rtv"
        }
      ],
      "postpass_barrier": [
        {
          "buffer_name": "swapchain",
          "type": "transition",
          "split_type": "none",
          "state_before": "rtv",
          "state_after": "present"
        }
      ]
    }
  ],
  "descriptor_handle_num_per_view_type_or_sampler": {
    "cbv": 0,
    "srv": 1,
    "uav": 1,
    "sampler": 0,
    "rtv": 0,
    "dsv": 0
  }
}
)"_json;
}
void ExecuteBarrier(D3d12CommandList* command_list, const uint32_t barrier_num, const Barrier* barrier_config, ID3D12Resource** resource) {
  auto allocator = GetTemporalMemoryAllocator();
  auto barriers = AllocateArray<D3D12_RESOURCE_BARRIER>(&allocator, barrier_num);
  for (uint32_t i = 0; i < barrier_num; i++) {
    auto& config = barrier_config[i];
    auto& barrier = barriers[i];
    barrier.Type  = config.type;
    barrier.Flags = config.flag;
    switch (barrier.Type) {
      case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION: {
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.pResource   = resource[i];
        barrier.Transition.StateBefore = config.state_before;
        barrier.Transition.StateAfter  = config.state_after;
        break;
      }
      case D3D12_RESOURCE_BARRIER_TYPE_ALIASING: {
        assert(false && "aliasing barrier not implemented yet");
        break;
      }
      case D3D12_RESOURCE_BARRIER_TYPE_UAV: {
        barrier.UAV.pResource   = resource[i];
        break;
      }
    }
  }
  command_list->ResourceBarrier(barrier_num, barriers);
}
template <typename A>
auto GetRenderPassVarSizeMap(A* allocator) {
  HashMap<uint32_t, A> render_pass_var_size(allocator);
  render_pass_var_size.Insert(SID("output to swapchain"), sizeof(FLOAT) * 4);
  render_pass_var_size.Insert(SID("clear uav"), sizeof(UINT) * 4);
  return render_pass_var_size;
}
using RenderPassFunction = void (*)(D3d12CommandList*, const void*, const D3D12_GPU_DESCRIPTOR_HANDLE*, const D3D12_CPU_DESCRIPTOR_HANDLE*, ID3D12Resource**);
void ClearRtv(D3d12CommandList* command_list, const void* pass_vars, [[maybe_unused]]const D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles, const D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles, [[maybe_unused]]ID3D12Resource** resources) {
  command_list->ClearRenderTargetView(*cpu_handles, static_cast<const FLOAT*>(pass_vars), 0, nullptr);
}
void ParsePassParamClearRtv(const nlohmann::json& j, void* dst) {
  auto src_color = j.at("clear_color");
  auto dst_color = static_cast<FLOAT*>(dst);
  for (uint32_t i = 0; i < 4; i++) {
    dst_color[i] = src_color[i];
  }
}
void ClearUav(D3d12CommandList* command_list, const void* pass_vars, const D3D12_GPU_DESCRIPTOR_HANDLE* gpu_handles, const D3D12_CPU_DESCRIPTOR_HANDLE* cpu_handles, ID3D12Resource** resources) {
  // command_list->ClearUnorderedAccessViewUint(*gpu_handles, *cpu_handles, *resources, static_cast<const UINT*>(pass_vars), 0, nullptr); // TODO comment-in
}
void ParsePassParamClearUav(const nlohmann::json& j, void* dst) {
  auto src_color = j.at("clear_color");
  auto dst_color = static_cast<UINT*>(dst);
  for (uint32_t i = 0; i < 4; i++) {
    dst_color[i] = src_color[i];
  }
}
void* D3d12BufferAllocatorAllocCallback(size_t Size, size_t Alignment, [[maybe_unused]] void* pUserData) {
  return gSystemMemoryAllocator->Allocate(Size, Alignment);
}
void D3d12BufferAllocatorFreeCallback([[maybe_unused]] void* pMemory, [[maybe_unused]] void* pUserData) {}
auto GetBufferAllocator(DxgiAdapter* adapter, D3d12Device* device) {
  using namespace D3D12MA;
  ALLOCATION_CALLBACKS allocation_callbacks{
    .pAllocate = D3d12BufferAllocatorAllocCallback,
    .pFree = D3d12BufferAllocatorFreeCallback,
    .pUserData = nullptr,
  };
  ALLOCATOR_DESC desc{
    .Flags = ALLOCATOR_FLAG_NONE,
    .pDevice = device,
    .PreferredBlockSize = 0, // will use default (64MiB)
    .pAllocationCallbacks = &allocation_callbacks,
    .pAdapter = adapter,
  };
  Allocator* allocator{nullptr};
  auto hr = CreateAllocator(&desc, &allocator);
  if (FAILED(hr)) {
    logerror("CreateAllocator failed. {}", hr);
  }
  return allocator;
}
struct BufferAllocation {
  D3D12MA::Allocation* allocation{nullptr};
  ID3D12Resource* resource{nullptr};
};
void ReleaseBufferAllocation(BufferAllocation* b) {
  b->allocation->Release();
  b->resource->Release();
}
auto CreateBuffer(const BufferConfig& config, D3D12MA::Allocator* allocator) {
  using namespace D3D12MA;
  ALLOCATION_DESC allocation_desc{
    .Flags = ALLOCATION_FLAG_NONE,
    .HeapType = config.heap_type,
    .ExtraHeapFlags = D3D12_HEAP_FLAG_NONE,
    .CustomPool = nullptr,
  };
  D3D12_RESOURCE_DESC1 resource_desc{
    .Dimension = config.dimension,
    .Alignment = config.alignment,
    .Width = config.width,
    .Height = config.height,
    .DepthOrArraySize = config.depth_or_array_size,
    .MipLevels = config.miplevels,
    .Format = config.format,
    .SampleDesc = {
      .Count = config.sample_count,
      .Quality = config.sample_quality,
    },
    .Layout = config.layout,
    .Flags = config.flags,
    .SamplerFeedbackMipRegion = {
      .Width = config.mip_width,
      .Height = config.mip_height,
      .Depth = config.mip_depth,
    },
  };
  auto clear_value = (resource_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER && (resource_desc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) != 0) ? &config.clear_value : nullptr;
  BufferAllocation buffer_allocation{};
  auto hr = allocator->CreateResource2(&allocation_desc, &resource_desc, config.initial_state, clear_value, nullptr, &buffer_allocation.allocation, IID_PPV_ARGS(&buffer_allocation.resource));
  if (FAILED(hr)) {
    logerror("failed to create resource. {}", hr);
    assert(false && "failed to create resource");
    if (buffer_allocation.allocation) {
      buffer_allocation.allocation->Release();
      buffer_allocation.allocation = nullptr;
    }
    if (buffer_allocation.resource) {
      buffer_allocation.resource->Release();
      buffer_allocation.resource = nullptr;
    }
  }
  return buffer_allocation;
}
void ExecuteBarrier(D3d12CommandList* command_list, const uint32_t barrier_num, const Barrier* barrier_config, const HashMap<BufferAllocation, MemoryAllocationJanitor>& buffer_list, const HashMap<ID3D12Resource*, MemoryAllocationJanitor>& extra_buffer_list) {
  auto allocator = GetTemporalMemoryAllocator();
  auto resource_list = AllocateArray<ID3D12Resource*>(&allocator, barrier_num);
  for (uint32_t i = 0; i < barrier_num; i++) {
    auto& buffer_name = barrier_config[i].buffer_name;
    auto buffer_allocation = buffer_list.Get(buffer_name);
    resource_list[i] = (buffer_allocation == nullptr) ? *extra_buffer_list.Get(buffer_name) : buffer_allocation->resource;
  }
  ExecuteBarrier(command_list, barrier_num, barrier_config, resource_list);
}
auto PrepareGpuHandleList(D3d12Device* device, const uint32_t render_pass_num, const RenderPass* render_pass_list, const DescriptorCpu<MemoryAllocationJanitor>& descriptor_cpu, DescriptorGpu* const descriptor_gpu, MemoryAllocationJanitor* allocator) {
  HashMap<D3D12_GPU_DESCRIPTOR_HANDLE, MemoryAllocationJanitor> gpu_handle_list(allocator);
  for (uint32_t i = 0; i < render_pass_num; i++) {
    auto& render_pass = render_pass_list[i];
    if (!gpu_handle_list.Insert(render_pass.name, descriptor_gpu->CopyDescriptors(device, render_pass.buffer_num, render_pass.buffers, descriptor_cpu))) {
      logerror("gpu_handle_list.Insert failed. {}", i);
      assert(false && "gpu_handle_list.Insert failed.");
    }
  }
  return gpu_handle_list;
}
} // namespace anonymous
} // namespace illuminate
TEST_CASE("d3d12 integration test") { // NOLINT
  using namespace illuminate; // NOLINT
  auto allocator = GetTemporalMemoryAllocator();
  HashMap<RenderPassFunction, MemoryAllocationJanitor> render_pass_functions(&allocator);
  HashMap<RenderPassVarParseFunction, MemoryAllocationJanitor> render_pass_var_parse_functions(&allocator);
  CHECK_UNARY(render_pass_functions.Insert(SID("output to swapchain"), ClearRtv));
  CHECK_UNARY(render_pass_var_parse_functions.Insert(SID("output to swapchain"), ParsePassParamClearRtv));
  CHECK_UNARY(render_pass_functions.Insert(SID("clear uav"), ClearUav));
  CHECK_UNARY(render_pass_var_parse_functions.Insert(SID("clear uav"), ParsePassParamClearUav));
  auto render_pass_var_size = GetRenderPassVarSizeMap(&allocator);
  RenderGraph render_graph;
  ParseRenderGraphJson(GetTestJson(), render_pass_var_size, render_pass_var_parse_functions, &allocator, &render_graph);
  const uint32_t swapchain_buffer_num = render_graph.frame_buffer_num + 1;
  DxgiCore dxgi_core;
  CHECK_UNARY(dxgi_core.Init()); // NOLINT
  Device device;
  CHECK_UNARY(device.Init(dxgi_core.GetAdapter())); // NOLINT
  Window window;
  CHECK_UNARY(window.Init(render_graph.window_title, render_graph.window_width, render_graph.window_height)); // NOLINT
  CommandListSet command_list_set;
  CHECK_UNARY(command_list_set.Init(device.Get(),
                                    render_graph.command_queue_num,
                                    render_graph.command_queue_type,
                                    render_graph.command_queue_priority,
                                    render_graph.command_list_num_per_queue,
                                    render_graph.frame_buffer_num,
                                    render_graph.command_allocator_num_per_queue_type));
  CommandQueueSignals command_queue_signals;
  command_queue_signals.Init(device.Get(), render_graph.command_queue_num, command_list_set.GetCommandQueueList());
  auto buffer_allocator = GetBufferAllocator(dxgi_core.GetAdapter(), device.Get());
  CHECK_NE(buffer_allocator, nullptr);
  DescriptorCpu<MemoryAllocationJanitor> descriptor_cpu;
  CHECK_UNARY(descriptor_cpu.Init(device.Get(), render_graph.descriptor_handle_num_per_view_type_or_sampler, &allocator));
  HashMap<BufferAllocation, MemoryAllocationJanitor> buffer_list(&allocator);
  for (uint32_t i = 0; i < render_graph.buffer_num; i++) {
    CAPTURE(i);
    auto buffer = CreateBuffer(render_graph.buffer_list[i], buffer_allocator);
    CHECK_NE(buffer.allocation, nullptr);
    CHECK_NE(buffer.resource, nullptr);
    CHECK_UNARY(buffer_list.Insert(render_graph.buffer_list[i].name, std::move(buffer)));
    for (uint32_t j = 0; j < render_graph.buffer_list[i].descriptor_type_num; j++) {
      CAPTURE(j);
      auto cpu_handle = descriptor_cpu.GetHandle(render_graph.buffer_list[i].name, render_graph.buffer_list[i].descriptor_type[j]);
      CHECK_NE(cpu_handle, nullptr);
      CHECK_UNARY(CreateView(device.Get(), render_graph.buffer_list[i].descriptor_type[j], render_graph.buffer_list[i], buffer.resource, cpu_handle));
    }
  }
  DescriptorGpu descriptor_gpu;
  CHECK_UNARY(descriptor_gpu.Init(device.Get()));
  Swapchain swapchain;
  CHECK_UNARY(swapchain.Init(dxgi_core.GetFactory(), command_list_set.GetCommandQueue(render_graph.swapchain_command_queue_index), device.Get(), window.GetHwnd(), render_graph.swapchain_format, swapchain_buffer_num, render_graph.frame_buffer_num, render_graph.swapchain_usage)); // NOLINT
  HashMap<ID3D12Resource*, MemoryAllocationJanitor> extra_buffer_list(&allocator);
  extra_buffer_list.Reserve(SID("swapchain")); // because MemoryAllocationJanitor is a stack allocator, all allocations must be done before frame loop starts.
  auto frame_signals = AllocateArray<uint64_t*>(&allocator, render_graph.frame_buffer_num);
  auto gpu_descriptor_offset_start = AllocateArray<uint64_t>(&allocator, render_graph.frame_buffer_num);
  auto gpu_descriptor_offset_end = AllocateArray<uint64_t>(&allocator, render_graph.frame_buffer_num);
  for (uint32_t i = 0; i < render_graph.frame_buffer_num; i++) {
    frame_signals[i] = AllocateArray<uint64_t>(&allocator, render_graph.command_queue_num);
    for (uint32_t j = 0; j < render_graph.command_queue_num; j++) {
      frame_signals[i][j] = 0;
    }
    gpu_descriptor_offset_start[i] = ~0u;
    gpu_descriptor_offset_end[i] = ~0u;
  }
  uint32_t tmp_memory_max_offset = 0U;
  for (uint32_t i = 0; i < render_graph.frame_loop_num; i++) {
    auto single_frame_allocator = GetTemporalMemoryAllocator();
    auto used_command_queue = AllocateArray<bool>(&single_frame_allocator, render_graph.command_queue_num);
    for (uint32_t j = 0; j < render_graph.command_queue_num; j++) {
      used_command_queue[j] = false;
    }
    const auto frame_index = i % render_graph.frame_buffer_num;
    command_queue_signals.WaitOnCpu(device.Get(), frame_signals[frame_index]);
    command_list_set.SucceedFrame();
    swapchain.UpdateBackBufferIndex();
    extra_buffer_list.Replace(SID("swapchain"), swapchain.GetResource());
    gpu_descriptor_offset_start[frame_index] = descriptor_gpu.GetOffsetView();
    auto gpu_handle_list = PrepareGpuHandleList(device.Get(), render_graph.render_pass_num, render_graph.render_pass_list, descriptor_cpu, &descriptor_gpu, &single_frame_allocator);
    gpu_descriptor_offset_end[frame_index] = descriptor_gpu.GetOffsetView();
    {
      // check handle num is sufficient for this frame
      uint32_t view_num = 0;
      for (uint32_t j = 0; j < render_graph.render_pass_num; j++) {
        for (uint32_t k = 0; k < render_graph.render_pass_list[j].buffer_num; k++) {
          switch (render_graph.render_pass_list[j].buffers[k].state) {
            case ViewType::kCbv:
            case ViewType::kSrv:
            case ViewType::kUav:
              view_num++;
              break;
            default:
              break;
          }
        }
      }
      if (gpu_descriptor_offset_start[frame_index] <= gpu_descriptor_offset_end[frame_index]) {
        CHECK_EQ(gpu_descriptor_offset_end[frame_index] - gpu_descriptor_offset_start[frame_index], view_num);
      } else {
        CHECK_EQ(descriptor_gpu.GetMaxOffsetView() - gpu_descriptor_offset_end[frame_index] + gpu_descriptor_offset_start[frame_index], view_num);
      }
    }
    if (i > 0) {
      // TODO check handle in fly is not overwritten
    }
    for (uint32_t k = 0; k < render_graph.render_pass_num; k++) {
      const auto& render_pass = render_graph.render_pass_list[k];
      auto command_list = command_list_set.GetCommandList(device.Get(), render_pass.command_queue_index); // TODO decide command list reuse policy for multi-thread
      ExecuteBarrier(command_list, render_pass.prepass_barrier_num, render_pass.prepass_barrier, buffer_list, extra_buffer_list);
      {
        auto render_pass_allocator = GetTemporalMemoryAllocator();
        auto resource_list = AllocateArray<ID3D12Resource*>(&render_pass_allocator, render_pass.buffer_num);
        auto cpu_handle_list = AllocateArray<D3D12_CPU_DESCRIPTOR_HANDLE>(&render_pass_allocator, render_pass.buffer_num);
        for (uint32_t b = 0; b < render_pass.buffer_num; b++) {
          auto& buffer = render_pass.buffers[b];
          auto buffer_allocation = buffer_list.Get(buffer.buffer_name);
          resource_list[b] = (buffer_allocation != nullptr) ? buffer_allocation->resource : *extra_buffer_list.Get(buffer.buffer_name);
          cpu_handle_list[b] = (buffer.buffer_name != SID("swapchain")) ? *descriptor_cpu.GetHandle(buffer.buffer_name, buffer.state) : swapchain.GetRtvHandle();
          tmp_memory_max_offset = std::max(GetTemporalMemoryOffset(), tmp_memory_max_offset);
        }
        (**render_pass_functions.Get(render_pass.name))(command_list, render_pass.pass_vars, gpu_handle_list.Get(render_pass.name), cpu_handle_list, resource_list);
        tmp_memory_max_offset = std::max(GetTemporalMemoryOffset(), tmp_memory_max_offset);
      }
      ExecuteBarrier(command_list, render_pass.postpass_barrier_num, render_pass.postpass_barrier, buffer_list, extra_buffer_list);
      used_command_queue[render_pass.command_queue_index] = true;
      command_list_set.ExecuteCommandList(render_pass.command_queue_index); // TODO
      frame_signals[frame_index][render_pass.command_queue_index] = command_queue_signals.SucceedSignal(render_pass.command_queue_index);
    } // render pass
    swapchain.Present();
    tmp_memory_max_offset = std::max(GetTemporalMemoryOffset(), tmp_memory_max_offset);
  }
  command_queue_signals.WaitAll(device.Get());
  swapchain.Term();
  descriptor_gpu.Term();
  descriptor_cpu.Term();
  for (uint32_t i = 0; i < render_graph.buffer_num; i++) {
    ReleaseBufferAllocation(buffer_list.Get(render_graph.buffer_list[i].name));
  }
  buffer_allocator->Release();
  command_queue_signals.Term();
  command_list_set.Term();
  window.Term();
  device.Term();
  dxgi_core.Term();
  loginfo("memory global:{} temp:{}", gSystemMemoryAllocator->GetOffset(), tmp_memory_max_offset);
  gSystemMemoryAllocator->Reset();
}
