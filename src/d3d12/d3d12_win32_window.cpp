#include "d3d12_win32_window.h"
#include "d3d12_src_common.h"
namespace illuminate {
namespace {
static HWND InitWindow(const char* const title, const uint32_t width, const uint32_t height, WindowCallback callback_func) {
  WNDCLASSEX wc;
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = callback_func;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 2);
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = title;
  wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
  if (!RegisterClassEx(&wc)) {
    auto err = GetLastError();
    logfatal("RegisterClassEx failed. {}", err);
    return nullptr;
  }
  auto hwnd = CreateWindowEx(0,
                             title,
                             title,
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             width, height,
                             nullptr,
                             nullptr,
                             wc.hInstance,
                             nullptr);
  if (!hwnd) {
    auto err = GetLastError();
    logfatal("CreateWindowEx failed. {}", err);
    return nullptr;
  }
  ShowWindow(hwnd, SW_SHOWDEFAULT);
  UpdateWindow(hwnd);
  return hwnd;
}
bool CloseWindow(HWND hwnd, const char* const title) {
  if (!DestroyWindow(hwnd)) {
    auto err = GetLastError();
    logwarn("DestroyWindow failed. {}", err);
    return false;
  }
  if (!UnregisterClass(title, GetModuleHandle(nullptr))) {
    auto err = GetLastError();
    logwarn("UnregisterClass failed. {}", err);
    return false;
  }
  return true;
}
bool SetWindowStyle(HWND hwnd, const UINT windowStyle) {
  SetLastError(0);
  if (SetWindowLongPtrW(hwnd, GWL_STYLE, windowStyle) != 0) return true;
  auto err = GetLastError();
  if (err == 0) { return true; }
  logwarn("SetWindowLongPtrW failed. {}", err);
  return false;
}
RECT GetCurrentDisplayRectInfo(HWND hwnd) {
  auto monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFOEX info = {};
  info.cbSize = sizeof(MONITORINFOEX);
  GetMonitorInfo(monitor, &info);
  return info.rcMonitor;
}
static RECT GetCurrentWindowRectInfo(HWND hwnd) {
  RECT rect = {};
  if (!GetWindowRect(hwnd, &rect)) {
    auto err = GetLastError();
    logwarn("GetWindowRect failed. {}", err);
  }
  return rect;
}
bool SetWindowPos(HWND hwnd, HWND hWndInsertAfter, const RECT& rect) {
  if (::SetWindowPos(hwnd, hWndInsertAfter, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_FRAMECHANGED | SWP_NOACTIVATE)) return true;
  auto err = GetLastError();
  logwarn("SetWindowPos failed. {}", err);
  return false;
}
bool SetFullscreenMode(HWND hwnd) {
  // not sure if this (FSBW) works for HDR mode.
  // use SetFullscreenState() in such case.
  // also unsure about if FPS caps unlike exclusive fullscreen mode.
  if (!SetWindowStyle(hwnd, WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX))) return false;
  if (!SetWindowPos(hwnd, HWND_TOP, GetCurrentDisplayRectInfo(hwnd))) return false;
  ShowWindow(hwnd, SW_MAXIMIZE);
  return true;
}
bool SetBackToWindowMode(HWND hwnd, const RECT& rect) {
  if (!SetWindowStyle(hwnd, WS_OVERLAPPEDWINDOW)) return false;
  if (!SetWindowPos(hwnd, HWND_NOTOPMOST, rect)) return false;
  ShowWindow(hwnd, SW_NORMAL);
  return true;
}
} // anonymous namespace
bool Window::Init(const char* const title, const uint32_t width, const uint32_t height, WindowCallback callback_func) {
  ProcessMessage();
  title_ = CreateString(title, MemoryType::kSystem);
  hwnd_ = illuminate::InitWindow(title_, width, height, callback_func == nullptr ? DefWindowProc : callback_func);
  window_closed_ = (hwnd_ == nullptr);
  return !window_closed_;
}
void Window::Term() {
  if (!window_closed_) {
    if (!DestroyWindow(hwnd_)) {
      auto err = GetLastError();
      logwarn("DestroyWindow failed. {} {}", title_, err);
    }
  }
  if (!UnregisterClass(title_, GetModuleHandle(nullptr))) {
    auto err = GetLastError();
    logwarn("UnregisterClass failed. {} {}", title_, err);
  }
}
bool Window::ProcessMessage() {
  MSG msg = {};
  while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
    if (msg.message == WM_QUIT) {
      window_closed_ = true;
    }
  }
  return !window_closed_;
}
}
#include "doctest/doctest.h"
TEST_CASE("win32 windows func test") { // NOLINT
  auto hwnd = illuminate::InitWindow("test window", 200, 100, DefWindowProc);
  CHECK(hwnd != nullptr); // NOLINT
  auto windowRect = illuminate::GetCurrentWindowRectInfo(hwnd);
  CHECK(windowRect.right - windowRect.left == 200); // NOLINT
  CHECK(windowRect.bottom - windowRect.top == 100); // NOLINT
  CHECK(illuminate::SetFullscreenMode(hwnd) == true); // NOLINT
  CHECK(illuminate::SetBackToWindowMode(hwnd, windowRect) == true); // NOLINT
  CHECK(illuminate::CloseWindow(hwnd, "test window") == true); // NOLINT
}
TEST_CASE("win32 window class") { // NOLINT
  illuminate::Window window;
  REQUIRE(window.Init("hello", 100, 200, nullptr));
  window.Term();
}
