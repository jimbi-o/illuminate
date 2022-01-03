cmake_minimum_required(VERSION 3.15)
project(illuminate
  VERSION 1.0
  DESCRIPTION "render graph parser"
  LANGUAGES CXX
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT illuminate)

include(cmake/CPM.cmake)
CPMAddPackage("gh:onqtam/doctest#2.4.6")
CPMAddPackage("gh:gabime/spdlog@1.9.2")
CPMAddPackage(
  NAME dxc
  URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.6.2112/dxc_2021_12_08.zip
  VERSION v1.6.2112
  DOWNLOAD_ONLY yes
)
CPMAddPackage(
  NAME d3d12
  URL https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.700.10-preview
  VERSION v1.700.10-preview
  DOWNLOAD_ONLY yes
)

option(LIB_MODE "build static library." OFF)
option(BUILD_WITH_TEST "build project with test (not used when LIB_MODE=ON)." OFF)
if (NOT LIB_MODE AND CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  add_executable(${CMAKE_PROJECT_NAME})
  if (BUILD_WITH_TEST)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE BUILD_WITH_TEST DOCTEST_CONFIG_SUPER_FAST_ASSERTS)
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
target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM INTERFACE spdlog)
set_target_properties(spdlog PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:spdlog,INTERFACE_INCLUDE_DIRECTORIES>)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
  spdlog::spdlog
  $<$<CONFIG:Debug>:dxguid.lib>)


target_compile_features(${CMAKE_PROJECT_NAME} PUBLIC cxx_std_20)
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/MP>) # parallel build in MSVC
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
  $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
  $<$<CXX_COMPILER_ID:Clang>:-Weverything -Wno-c++98-c++11-c++14-compat -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-c++20-compat>
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
target_include_directories(${CMAKE_PROJECT_NAME}
  PRIVATE
  "include"
  "${dxc_SOURCE_DIR}/inc"
  "${d3d12_SOURCE_DIR}/build/native/include"
)
add_subdirectory(src)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/d3d12")
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  "${d3d12_SOURCE_DIR}/build/native/bin/x64/D3D12Core.dll"
  "$<$<CONFIG:Debug>:${d3d12_SOURCE_DIR}/build/native/bin/x64/D3D12Core.pdb>"
  "$<$<CONFIG:Debug>:${d3d12_SOURCE_DIR}/build/native/bin/x64/d3d12SDKLayers.dll>"
  "$<$<CONFIG:Debug>:${d3d12_SOURCE_DIR}/build/native/bin/x64/d3d12SDKLayers.pdb>"
  "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/d3d12")
