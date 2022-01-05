#ifndef ILLUMINATE_WIN32_H
#define ILLUMINATE_WIN32_H
#include <cstdint>
#include <string>
#include <functional>
#include <Windows.h>
namespace illuminate {
using WindowCallback = std::function<bool(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)>;
class Window {
 public:
  bool Init(const char* const title, const uint32_t width, const uint32_t height);
  void Term();
  HWND GetHwnd() const { return hwnd_; }
  void AddCallback(WindowCallback&&); // note: shared between all instances so far.
  void ProcessMessage();
 private:
  static LRESULT CallbackFunc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  std::string title_;
  HWND hwnd_ = nullptr;
};
}
#endif
