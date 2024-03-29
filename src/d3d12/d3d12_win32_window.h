#ifndef ILLUMINATE_D3D12_WIN32_H
#define ILLUMINATE_D3D12_WIN32_H
#include <cstdint>
#include <string>
#include <functional>
#include <Windows.h>
namespace illuminate {
using WindowCallback = LRESULT (*)(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
class Window {
 public:
  bool Init(const char* const title, const uint32_t width, const uint32_t height, WindowCallback callback_func);
  void Term();
  HWND GetHwnd() const { return hwnd_; }
  bool ProcessMessage();
 private:
  const char* title_{nullptr};
  HWND hwnd_{nullptr};
  bool window_closed_{false};
};
}
#endif
