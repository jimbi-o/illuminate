#include "d3d12_header_common.h"
#include "d3d12_src_common.h"
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
uint32_t GetDxgiFormatPerPixelSizeInBytes(const DXGI_FORMAT format) {
  switch (format) {
    case DXGI_FORMAT_R32G32B32_FLOAT: return 12;
  }
  logerror("GetDxgiFormatPerPixelSizeInBytes not implemented yet. {}", format);
  assert(false && "GetDxgiFormatPerPixelSizeInBytes");
  return 0;
}
uint32_t GetVertexBufferTypeNum(const uint32_t vertex_buffer_type_flags) {
  auto num = vertex_buffer_type_flags;
  uint32_t count = 0;
  for (uint32_t i = 0; i < kVertexBufferTypeNum && num > 0; i++) {
    count += (num & 1);
    num >>= 1;
  }
  return count;
}
uint32_t GetVertexBufferTypeFlag(const VertexBufferType vertex_buffer_type) {
  if (vertex_buffer_type >= kVertexBufferTypeNum) { return ~0U; }
  return 1 << vertex_buffer_type;
}
VertexBufferType GetVertexBufferTypeAtIndex(const uint32_t vertex_buffer_type_flags, const uint32_t index) {
  auto num = vertex_buffer_type_flags;
  uint32_t count = 0;
  for (uint32_t i = 0; i < kVertexBufferTypeNum && num > 0; i++) {
    if (num & 1) {
      if (count >= index) {
        return static_cast<VertexBufferType>(i);
      }
      count++;
    }
    num >>= 1;
  }
  return kVertexBufferTypeNum;
}
}
#include "doctest/doctest.h"
TEST_CASE("vertex buffer type flag counting") { // NOLINT
  using namespace illuminate;
  CHECK_EQ(GetVertexBufferTypeNum(0), 0);
  CHECK_EQ(GetVertexBufferTypeNum(1), 1);
  CHECK_EQ(GetVertexBufferTypeNum(2), 1);
  CHECK_EQ(GetVertexBufferTypeNum(4), 1);
  CHECK_EQ(GetVertexBufferTypeNum(8), 1);
  CHECK_EQ(GetVertexBufferTypeNum(3), 2);
  CHECK_EQ(GetVertexBufferTypeNum(5), 2);
  CHECK_EQ(GetVertexBufferTypeNum(6), 2);
  CHECK_EQ(GetVertexBufferTypeNum(7), 3);
  CHECK_EQ(GetVertexBufferTypeNum(10), 2);
  CHECK_EQ(GetVertexBufferTypeNum(11), 3);
  CHECK_EQ(GetVertexBufferTypeNum(12), 2);
  CHECK_EQ(GetVertexBufferTypeNum(13), 3);
  CHECK_EQ(GetVertexBufferTypeNum(14), 3);
  CHECK_EQ(GetVertexBufferTypeNum(15), 4);
  CHECK_EQ(GetVertexBufferTypeFlag(kVertexBufferTypePosition),  1);
  CHECK_EQ(GetVertexBufferTypeFlag(kVertexBufferTypeNormal),    2);
  CHECK_EQ(GetVertexBufferTypeFlag(kVertexBufferTypeTangent),   4);
  CHECK_EQ(GetVertexBufferTypeFlag(kVertexBufferTypeTexCoord0), 8);
  CHECK_EQ(GetVertexBufferTypeFlag(kVertexBufferTypeNum),       ~0U);
  CHECK_EQ(GetVertexBufferTypeAtIndex(1, 0), kVertexBufferTypePosition);
  CHECK_EQ(GetVertexBufferTypeAtIndex(2, 0), kVertexBufferTypeNormal);
  CHECK_EQ(GetVertexBufferTypeAtIndex(4, 0), kVertexBufferTypeTangent);
  CHECK_EQ(GetVertexBufferTypeAtIndex(8, 0), kVertexBufferTypeTexCoord0);
  CHECK_EQ(GetVertexBufferTypeAtIndex(3, 0), kVertexBufferTypePosition);
  CHECK_EQ(GetVertexBufferTypeAtIndex(3, 1), kVertexBufferTypeNormal);
  CHECK_EQ(GetVertexBufferTypeAtIndex(5, 0), kVertexBufferTypePosition);
  CHECK_EQ(GetVertexBufferTypeAtIndex(5, 1), kVertexBufferTypeTangent);
  CHECK_EQ(GetVertexBufferTypeAtIndex(6, 0), kVertexBufferTypeNormal);
  CHECK_EQ(GetVertexBufferTypeAtIndex(6, 1), kVertexBufferTypeTangent);
  CHECK_EQ(GetVertexBufferTypeAtIndex(7, 0), kVertexBufferTypePosition);
  CHECK_EQ(GetVertexBufferTypeAtIndex(7, 1), kVertexBufferTypeNormal);
  CHECK_EQ(GetVertexBufferTypeAtIndex(7, 2), kVertexBufferTypeTangent);
  CHECK_EQ(GetVertexBufferTypeAtIndex(9, 0), kVertexBufferTypePosition);
  CHECK_EQ(GetVertexBufferTypeAtIndex(9, 1), kVertexBufferTypeTexCoord0);
  CHECK_EQ(GetVertexBufferTypeAtIndex(10, 0), kVertexBufferTypeNormal);
  CHECK_EQ(GetVertexBufferTypeAtIndex(10, 1), kVertexBufferTypeTexCoord0);
  CHECK_EQ(GetVertexBufferTypeAtIndex(11, 0), kVertexBufferTypePosition);
  CHECK_EQ(GetVertexBufferTypeAtIndex(11, 1), kVertexBufferTypeNormal);
  CHECK_EQ(GetVertexBufferTypeAtIndex(11, 2), kVertexBufferTypeTexCoord0);
  CHECK_EQ(GetVertexBufferTypeAtIndex(12, 0), kVertexBufferTypeTangent);
  CHECK_EQ(GetVertexBufferTypeAtIndex(12, 1), kVertexBufferTypeTexCoord0);
  CHECK_EQ(GetVertexBufferTypeAtIndex(13, 0), kVertexBufferTypePosition);
  CHECK_EQ(GetVertexBufferTypeAtIndex(13, 1), kVertexBufferTypeTangent);
  CHECK_EQ(GetVertexBufferTypeAtIndex(13, 2), kVertexBufferTypeTexCoord0);
  CHECK_EQ(GetVertexBufferTypeAtIndex(14, 0), kVertexBufferTypeNormal);
  CHECK_EQ(GetVertexBufferTypeAtIndex(14, 1), kVertexBufferTypeTangent);
  CHECK_EQ(GetVertexBufferTypeAtIndex(14, 2), kVertexBufferTypeTexCoord0);
  CHECK_EQ(GetVertexBufferTypeAtIndex(15, 0), kVertexBufferTypePosition);
  CHECK_EQ(GetVertexBufferTypeAtIndex(15, 1), kVertexBufferTypeNormal);
  CHECK_EQ(GetVertexBufferTypeAtIndex(15, 2), kVertexBufferTypeTangent);
  CHECK_EQ(GetVertexBufferTypeAtIndex(15, 3), kVertexBufferTypeTexCoord0);
}
