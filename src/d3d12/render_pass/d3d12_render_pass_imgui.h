#ifndef ILLUMINATE_D3D12_RENDER_PASS_IMGUI_H
#define ILLUMINATE_D3D12_RENDER_PASS_IMGUI_H
#include "d3d12_render_pass_common.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
namespace illuminate {
class RenderPassImgui {
 public:
  static void* Init(RenderPassFuncArgsInit* args, const uint32_t render_pass_index) {
    const auto imgui_font_buffer_index = GetBufferAllocationIndex(*args->buffer_list, args->render_pass_list[render_pass_index].buffer_list[0].buffer_index);
    auto cpu_handle_font = args->descriptor_cpu->GetHandle(imgui_font_buffer_index, DescriptorType::kSrv);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplWin32_Init(args->hwnd);
    ImGui_ImplDX12_Init(args->device, args->frame_buffer_num, args->main_buffer_format.swapchain, nullptr/*descriptor heap not used in single viewport mode*/, cpu_handle_font, {}/*gpu_handle updated every frame before rendering*/);
    if (!ImGui_ImplDX12_CreateDeviceObjects()) {
      logerror("ImGui_ImplDX12_CreateDeviceObjects failed.");
      assert(false);
    }
    return PrepareParam(args->render_pass_num, args->render_pass_list, args->allocator);
  }
  static void Term() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
  }
  static void Update(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::GetIO().Fonts->SetTexID((ImTextureID)args_per_pass->gpu_handles[0].ptr);
    ImGui::NewFrame();
    RegisterGUI(args_common, args_per_pass);
  }
  static auto IsRenderNeeded([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, [[maybe_unused]]RenderPassFuncArgsRenderPerPass* args_per_pass) { return true; }
  static auto Render([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
    ImGui::Render();
    args_per_pass->command_list->OMSetRenderTargets(1, &args_per_pass->cpu_handles[1], true, nullptr);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), args_per_pass->command_list);
  }
 private:
  static void* PrepareParam(const uint32_t render_pass_num, RenderPass* render_pass_list, MemoryAllocationJanitor* allocator);
  static void RegisterGUI(RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass);
  RenderPassImgui() = delete;
};
}
#endif
