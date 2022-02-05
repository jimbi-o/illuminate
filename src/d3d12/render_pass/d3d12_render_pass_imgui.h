#ifndef ILLUMINATE_RENDER_PASS_D3D12_IMGUI_H
#define ILLUMINATE_RENDER_PASS_D3D12_IMGUI_H
#include "d3d12_render_pass_common.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
namespace illuminate {
class RenderPassImguiUpdate {
 public:
  static void* Init([[maybe_unused]]RenderPassFuncArgsInit* args) { return nullptr; }
  static void Term([[maybe_unused]]void* ptr) {}
  static void Update([[maybe_unused]]RenderPassFuncArgsUpdate* args) {
  }
  static void Render(RenderPassFuncArgsRender* args) {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::GetIO().Fonts->SetTexID((ImTextureID)args->gpu_handles[0].ptr);
    ImGui::NewFrame();
  }
 private:
  RenderPassImguiUpdate() = delete;
};
class RenderPassImguiRender {
 public:
  static void* Init(RenderPassFuncArgsInit* args) {
    auto cpu_handle_font = args->descriptor_cpu->GetHandle(SID("imgui_font"), DescriptorType::kSrv);
    assert(cpu_handle_font);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplWin32_Init(args->hwnd);
    ImGui_ImplDX12_Init(args->device, args->frame_buffer_num, args->main_buffer_format.swapchain, nullptr/*descriptor heap not used in single viewport mode*/, *cpu_handle_font, {}/*gpu_handle updated every frame before rendering*/);
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
  static void Update([[maybe_unused]]RenderPassFuncArgsUpdate* args) {
  }
  static void Render(RenderPassFuncArgsRender* args) {
    ImGui::Text("Hello, world %d", 123);
    ImGui::Render();
    args->command_list->OMSetRenderTargets(1, &args->cpu_handles[0], true, nullptr);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), args->command_list);
  }
 private:
  RenderPassImguiRender() = delete;
};
}
#endif
