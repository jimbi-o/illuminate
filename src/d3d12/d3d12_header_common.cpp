#include "d3d12_header_common.h"
namespace illuminate {
uint32_t GetPhysicalWidth(const MainBufferSize& buffer_size, const BufferSizeRelativeness& relativeness, const float scale) {
  float val = 0.0f;
  switch (relativeness) {
    case BufferSizeRelativeness::kAbsolute: {
      val = scale;
      break;
    }
    case BufferSizeRelativeness::kSwapchainRelative: {
      val = buffer_size.swapchain.width * scale;
      break;
    }
    case BufferSizeRelativeness::kPrimaryBufferRelative: {
      val = buffer_size.primarybuffer.width * scale;
      break;
    }
  }
  return static_cast<uint32_t>(val);
}
uint32_t GetPhysicalHeight(const MainBufferSize& buffer_size, const BufferSizeRelativeness& relativeness, const float scale) {
  float val = 0.0f;
  switch (relativeness) {
    case BufferSizeRelativeness::kAbsolute: {
      val = scale;
      break;
    }
    case BufferSizeRelativeness::kSwapchainRelative: {
      val = buffer_size.swapchain.height * scale;
      break;
    }
    case BufferSizeRelativeness::kPrimaryBufferRelative: {
      val = buffer_size.primarybuffer.height * scale;
      break;
    }
  }
  return static_cast<uint32_t>(val);
}
}
