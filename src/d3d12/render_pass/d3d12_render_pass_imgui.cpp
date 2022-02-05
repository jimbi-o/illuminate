#include "d3d12_render_pass_imgui.h"
namespace illuminate {
void RenderPassImgui::RegisterGUI() {
  ImGui::Text("Hello, world %d", 123);
}
}
