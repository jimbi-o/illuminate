cmake_minimum_required(VERSION 3.15)
project(illuminate
  VERSION 1.0
  DESCRIPTION "render graph parser"
  LANGUAGES CXX
)

option(USE_D3D12_AGILITY_SDK "enable d3d12 agility SDK" OFF)
option(LIB_MODE "build static library." OFF)
option(BUILD_WITH_TEST "build project with test (not used when LIB_MODE=ON)." OFF)
option(USE_GRAPHICS_DEBUG_SCOPE "enable graphics scope name" ON)
option(OUTPUT_SHADER_DEBUG_INFO "output shader debug info on fly" ON)

if(BUILD_WITH_TEST)
  set(TEST_MODEL_NAME "Box" CACHE STRING "model to load")
endif()
if(OUTPUT_SHADER_DEBUG_INFO)
  set(SHADER_DEBUG_INFO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resource/shader_debug_info" CACHE STRING "shader debug info output path")
endif()

include(cmake/CPM.cmake)
CPMAddPackage("gh:onqtam/doctest#2.4.6")
CPMAddPackage(
  NAME spdlog
  GITHUB_REPOSITORY "gabime/spdlog"
  VERSION 1.9.2
  OPTIONS
  "SPDLOG_WCHAR_SUPPORT ON"
)
CPMAddPackage(
  NAME dxc
  URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.6.2112/dxc_2021_12_08.zip
  VERSION v1.6.2112
  DOWNLOAD_ONLY yes
)
if(USE_D3D12_AGILITY_SDK)
  CPMAddPackage(
    NAME d3d12
    URL https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.700.10-preview
    VERSION v1.700.10-preview
    DOWNLOAD_ONLY yes
  )
endif()
CPMAddPackage(
  NAME nlohmann_json
  GITHUB_REPOSITORY "nlohmann/json"
  VERSION 3.10.5
  OPTIONS
  "JSON_BuildTests OFF"
)
CPMAddPackage(
  NAME D3D12MemoryAllocator
  GITHUB_REPOSITORY "GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator"
  GIT_TAG "f78c4fbd38e309b36d7b0a7db3d44267d6ac23c0"
)
CPMAddPackage("gh:microsoft/DirectX-Headers#v1.700.10-preview")
CPMAddPackage(
  NAME tinygltf
  GITHUB_REPOSITORY "syoyo/tinygltf"
  VERSION 2.5.0
  DOWNLOAD_ONLY yes
)
CPMAddPackage("gh:ocornut/imgui#v1.87")
CPMAddPackage("gh:epezent/implot#v0.13")
CPMAddPackage(
  NAME WinPixEventRuntime
  URL https://www.nuget.org/api/v2/package/WinPixEventRuntime/1.0.220124001
  VERSION 1.0.220124001
  DOWNLOAD_ONLY yes
)
CPMAddPackage(
  NAME gltf_samples
  GITHUB_REPOSITORY "KhronosGroup/glTF-Sample-Models"
  GIT_TAG "b590f6f07cd6931deab8486b9b54e8174386dd49"
  DOWNLOAD_ONLY yes
)
CPMAddPackage(
  NAME DirectXTK12
  GITHUB_REPOSITORY "microsoft/DirectXTK12"
  GIT_TAG "3356e06c6cb7e4684bc1418b864799c098ab6704"
)
CPMAddPackage(
  NAME gfxminimath
  GITHUB_REPOSITORY "jimbi-o/gfxminimath"
  GIT_TAG "43e645d6b61b4f074660aee556264f9b9dfd35a8"
)
CPMAddPackage(
  NAME RenderPipelineShaders
  GITHUB_REPOSITORY "GPUOpen-LibrariesAndSDKs/RenderPipelineShaders"
  GIT_TAG "3a693036b72fc657b9542b631e34dfaf03033741"
  OPTIONS
  "RpsBuildTests OFF" "RpsEnableVulkan OFF" "RpsEnableImGui OFF"
)

if(BUILD_WITH_TEST AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resource/scenedata/${TEST_MODEL_NAME}")
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resource/scenedata")
  file(COPY "${gltf_samples_SOURCE_DIR}/2.0/${TEST_MODEL_NAME}" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/resource/scenedata")
endif()
if(NOT EXISTS "${SHADER_DEBUG_INFO_DIR}")
  file(MAKE_DIRECTORY "${SHADER_DEBUG_INFO_DIR}")
endif()

if(NOT LIB_MODE AND CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  add_executable(${CMAKE_PROJECT_NAME})
  if(BUILD_WITH_TEST)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE BUILD_WITH_TEST DOCTEST_CONFIG_SUPER_FAST_ASSERTS TEST_MODEL_PATH="scenedata/${TEST_MODEL_NAME}/glTF/${TEST_MODEL_NAME}.gltf")
    target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PUBLIC "${doctest_SOURCE_DIR}")
    add_subdirectory(tests)
  else()
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE DOCTEST_CONFIG_DISABLE)
    add_subdirectory(app)
  endif()
else()
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE DOCTEST_CONFIG_DISABLE)
  add_library(${CMAKE_PROJECT_NAME})
endif()
if(USE_GRAPHICS_DEBUG_SCOPE)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE USE_GRAPHICS_DEBUG_SCOPE)
endif()
if(OUTPUT_SHADER_DEBUG_INFO)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE SHADER_DEBUG_INFO_PATH="${SHADER_DEBUG_INFO_DIR}/")
endif()

target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM INTERFACE spdlog)
set_target_properties(spdlog PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:spdlog,INTERFACE_INCLUDE_DIRECTORIES>)

add_library(rendergraph)
target_sources(rendergraph
  PRIVATE
  resource/configs/default_rendergraph.rpsl.g.c
)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  spdlog
  D3D12MemoryAllocator
  "${WinPixEventRuntime_SOURCE_DIR}/bin/x64/WinPixEventRuntime.lib"
  DirectXTK12
  $<$<CONFIG:Debug>:dxguid.lib>
  gfxminimath
  rps_core
  rps_runtime
  rps_runtime_d3d12
  rendergraph
)

target_compile_features(${CMAKE_PROJECT_NAME} PUBLIC cxx_std_20)
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
  $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:Clang>:-Weverything -Wno-c++98-c++11-c++14-compat -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-c++20-compat>
  $<$<CXX_COMPILER_ID:MSVC>:/W4 /MP>
)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
  TINYGLTF_NO_STB_IMAGE
  TINYGLTF_NO_STB_IMAGE_WRITE
  TINYGLTF_NO_EXTERNAL_IMAGE
  TINYGLTF_NO_INCLUDE_JSON
  TINYGLTF_NO_INCLUDE_STB_IMAGE
  TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
  TINYGLTF_USE_CPP14
  $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>)
target_include_directories(${CMAKE_PROJECT_NAME}
  PRIVATE
  "include"
  "${dxc_SOURCE_DIR}/inc"
  "${nlohmann_json_SOURCE_DIR}/include"
  "${D3D12MemoryAllocator_SOURCE_DIR}/include"
  "${tinygltf_SOURCE_DIR}"
  "${imgui_SOURCE_DIR}"
  "${implot_SOURCE_DIR}"
  "${WinPixEventRuntime_SOURCE_DIR}/Include"
  "${DirectXTK12_SOURCE_DIR}/Inc"
  "${DirectX-Headers_SOURCE_DIR}/include/directx"
  "${gfxminimath_SOURCE_DIR}/include"
  "${RenderPipelineShaders_SOURCE_DIR}/include"
  "resource"
)
target_precompile_headers(${CMAKE_PROJECT_NAME}
  PRIVATE
  <cstdint>
  <type_traits>
  <cstring>
  <string>
  <string_view>
  <dxgi1_6.h>
  <directxmath.h>
  "${dxc_SOURCE_DIR}/inc/d3d12shader.h"
  "${dxc_SOURCE_DIR}/inc/dxcapi.h"
  "${nlohmann_json_SOURCE_DIR}/include/nlohmann/json.hpp"
  "${spdlog_SOURCE_DIR}/include/spdlog/spdlog.h"
  "${D3D12MemoryAllocator_SOURCE_DIR}/include/D3D12MemAlloc.h"
  "${tinygltf_SOURCE_DIR}/tiny_gltf.h"
  "${imgui_SOURCE_DIR}/imgui.h"
  "${implot_SOURCE_DIR}/implot.h"
  "${WinPixEventRuntime_SOURCE_DIR}/Include/WinPixEventRuntime/pix3.h"
  "${DirectXTK12_SOURCE_DIR}/Inc/DDSTextureLoader.h"
  "${DirectX-Headers_SOURCE_DIR}/include/directx/d3d12.h"
  "${DirectX-Headers_SOURCE_DIR}/include/directx/d3dx12.h"
  "${gfxminimath_SOURCE_DIR}/include/gfxminimath/gfxminimath.h"
  "${RenderPipelineShaders_SOURCE_DIR}/include/rps/rps.h"
)

if(USE_D3D12_AGILITY_SDK)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE USE_D3D12_AGILITY_SDK)
  target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${d3d12_SOURCE_DIR}/build/native/include")
  target_precompile_headers(${CMAKE_PROJECT_NAME} PRIVATE "${d3d12_SOURCE_DIR}/build/native/include/d3d12.h")
endif()

add_subdirectory(src)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  "${dxc_SOURCE_DIR}/bin/x64/dxil.dll"
  "${dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll"
  "${WinPixEventRuntime_SOURCE_DIR}/bin/x64/WinPixEventRuntime.dll"
  "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/"
)
if(USE_D3D12_AGILITY_SDK)
  add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/d3d12"
  )
  add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${d3d12_SOURCE_DIR}/build/native/bin/x64/D3D12Core.dll"
    "$<$<CONFIG:Debug>:${d3d12_SOURCE_DIR}/build/native/bin/x64/D3D12Core.pdb>"
    "$<$<CONFIG:Debug>:${d3d12_SOURCE_DIR}/build/native/bin/x64/d3d12SDKLayers.dll>"
    "$<$<CONFIG:Debug>:${d3d12_SOURCE_DIR}/build/native/bin/x64/d3d12SDKLayers.pdb>"
    "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/d3d12"
  )
endif()

set_directory_properties(PROPERTIES VS_STARTUP_PROJECT illuminate)
set_target_properties(illuminate PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resource")
