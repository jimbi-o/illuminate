namespace illuminate {
namespace {
using namespace DirectX::SimpleMath;
void SetFloat(const float f, void* dst) {
  auto v = static_cast<float*>(dst);
  *v = f;
}
void SetCompactProjectionParam(const Matrix& projection_matrix, const uint32_t size_in_bytes, void* dst) {
  float val[] = {
    projection_matrix.m[0][0],
    projection_matrix.m[1][1],
    projection_matrix.m[3][2],
    projection_matrix.m[2][2],
  };
  memcpy(dst, val, size_in_bytes);
}
void SetLightOriginLocationInScreenSpace(const Vector3& light_direction_vs, const Matrix& projection_matrix, const uint32_t primarybuffer_width, const uint32_t primarybuffer_height, const uint32_t size_in_bytes, void* dst) {
  auto light_origin = Vector3::Transform(light_direction_vs * 100000.0f, projection_matrix);
  light_origin = (light_origin + Vector3::One) * 0.5f;
  int32_t light_origin_location[] = {
    static_cast<int32_t>(std::round(light_origin.x * primarybuffer_width)),
    static_cast<int32_t>(std::round(light_origin.y * primarybuffer_height))
  };
  memcpy(dst, light_origin_location, size_in_bytes);
}
auto GetAspectRatio(const Size2d& buffer_size) {
  return static_cast<float>(buffer_size.width) / buffer_size.height;
}
auto CalcViewMatrix(const RenderPassConfigDynamicData& dynamic_data) {
  using namespace DirectX::SimpleMath;
  return Matrix::CreateLookAt(Vector3(dynamic_data.camera_pos), Vector3(dynamic_data.camera_focus), Vector3::Up);
}
auto CalcProjectionMatrix(const RenderPassConfigDynamicData& dynamic_data, const float aspect_ratio) {
  using namespace DirectX::SimpleMath;
  return Matrix::CreatePerspectiveFieldOfView(ToRadian(dynamic_data.fov_vertical), aspect_ratio, dynamic_data.near_z, dynamic_data.far_z);
}
auto CalcLightVectorVs(const float light_direction[3], const Matrix& view_matrix) {
  auto light_direction_vs = Vector3::TransformNormal(-Vector3(light_direction), view_matrix);
  light_direction_vs.Normalize();
  return light_direction_vs;
}
auto FillShaderBoundCBuffers(const RenderPassConfigDynamicData& dynamic_data, const MainBufferSize& main_buffer_size, const ArrayOf<CBuffer>& cbuffer_list, void* const * cbuffer_src_list, const uint32_t* cbuffer_writable_size, void** cbuffer_dst_list) {
  const auto view_matrix = CalcViewMatrix(dynamic_data);
  const auto projection_matrix = CalcProjectionMatrix(dynamic_data, GetAspectRatio(main_buffer_size.primarybuffer));
  const auto light_direction_vs = CalcLightVectorVs(dynamic_data.light_direction, view_matrix);
  for (uint32_t i = 0; i < cbuffer_list.size; i++) {
    auto dst = cbuffer_src_list[i];
    for (uint32_t j = 0; j < cbuffer_list.array[i].params.size; j++) {
      auto& param = cbuffer_list.array[i].params.array[j];
      switch (param.name_hash) {
        case SID("view_matrix"): {
          memcpy(dst, view_matrix.m, param.size_in_bytes);
          break;
        }
        case SID("projection_matrix"): {
          memcpy(dst, projection_matrix.m, param.size_in_bytes);
          break;
        }
        case SID("compact_projection_param"): {
          SetCompactProjectionParam(projection_matrix, param.size_in_bytes, dst);
          break;
        }
        case SID("light vector"): {
          memcpy(dst, &light_direction_vs, param.size_in_bytes);
          break;
        }
        case SID("light_origin_location"): {
          SetLightOriginLocationInScreenSpace(light_direction_vs, projection_matrix, main_buffer_size.primarybuffer.width, main_buffer_size.primarybuffer.height, param.size_in_bytes, dst);
          break;
        }
        case SID("light_slope_zx"): {
          SetFloat(light_direction_vs.z / light_direction_vs.x, dst);
          break;
        }
        case SID("far_div_near"): {
          SetFloat(dynamic_data.far_z / dynamic_data.near_z, dst);
          break;
        }
      }
      dst = SucceedPtr(dst, param.size_in_bytes);
    }
    memcpy(cbuffer_dst_list[i], cbuffer_src_list[i], cbuffer_writable_size[i]);
  }
}
auto GetCBufferParamSizeInBytes(const StrHash& cbuffer_param_name) {
  switch (cbuffer_param_name) {
    case SID("view_matrix"):
    case SID("projection_matrix"): {
      return GetUint32(sizeof(float)) * 16U;
    }
    case SID("compact_projection_param"): {
      return GetUint32(sizeof(float)) * 4U;
    }
    case SID("light vector"): {
      return GetUint32(sizeof(float)) * 3U;
    }
    case SID("light_origin_location"): {
      return GetUint32(sizeof(float)) * 2U;
    }
    case SID("far_div_near"):
    case SID("light_slope_zx"): {
      return GetUint32(sizeof(float));
    }
  }
  assert(false && "no valid cbuffer param found");
  return 0U;
}
} // namespace
} // namespace illuminate
