#ifndef ILLUMINATE_D3D12_RENDER_PASS_IMGUI_H
#define ILLUMINATE_D3D12_RENDER_PASS_IMGUI_H
#include "d3d12_render_pass_common.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
namespace illuminate {
class RenderPassImgui {
 public:
  static void* Init(RenderPassFuncArgsInit* args) {
    const auto imgui_font_buffer_index = *(args->named_buffer_allocator_index->Get(SID("imgui_font")));
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
    return nullptr;
  }
  static void Term([[maybe_unused]]void* ptr) {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
  }
  static void Update([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::GetIO().Fonts->SetTexID((ImTextureID)args_per_pass->gpu_handles[0].ptr);
    ImGui::NewFrame();
    RegisterGUI();
  }
  static auto IsRenderNeeded([[maybe_unused]]const void* args) { return true; }
  static auto Render([[maybe_unused]]RenderPassFuncArgsRenderCommon* args_common, RenderPassFuncArgsRenderPerPass* args_per_pass) {
    ImGui::Render();
    args_per_pass->command_list->OMSetRenderTargets(1, &args_per_pass->cpu_handles[1], true, nullptr);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), args_per_pass->command_list);
  }
 private:
  static void RegisterGUI();
  RenderPassImgui() = delete;
};
}
#endif
