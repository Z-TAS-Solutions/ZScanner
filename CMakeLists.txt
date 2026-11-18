cmake_minimum_required(VERSION 3.18)
project(ZScanner LANGUAGES CXX)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/VertexShader.cso
  COMMAND
    fxc /T vs_5_0 /E VSMain /Fo ${CMAKE_CURRENT_BINARY_DIR}/VertexShader.cso /Fc
    vsshaderAssembly.txt ${CMAKE_CURRENT_SOURCE_DIR}/src/vertex_shader.hlsl
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/vertex_shader.hlsl
  VERBATIM)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/PixelShader.cso
  COMMAND
    fxc /T ps_5_0 /E PSMain /Fo ${CMAKE_CURRENT_BINARY_DIR}/PixelShader.cso /Fc
    psshaderAssembly.txt ${CMAKE_CURRENT_SOURCE_DIR}/src/pixel_shader.hlsl
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/pixel_shader.hlsl
  VERBATIM)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/debug)

add_custom_target(
  CompileShaders ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/VertexShader.cso
                             ${CMAKE_CURRENT_BINARY_DIR}/PixelShader.cso)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(architecture x64)
else()
  set(architecture x86)
  # set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/lib/x86/)
endif()

if(WIN32)
  set(SourceDir ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows)
else()
  set(SourceDir ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/unix)
endif()

include(FetchContent)

# ImGui setup
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG master)

FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_MakeAvailable(imgui)

  add_library(
    imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp)

  if(WIN32)
    target_sources(
      imgui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
                    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
    target_link_libraries(imgui PUBLIC d3d11 d3dcompiler dwmapi)

  elseif(UNIX)
    target_sources(
      imgui PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
                    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)
  endif()

  target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR}
                                          ${imgui_SOURCE_DIR}/backends)
endif()

# libssh2 setup with common libssh2 build flags
set(BUILD_SHARED_LIBS
    OFF
    CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES
    OFF
    CACHE BOOL "" FORCE)
set(BUILD_TESTING
    OFF
    CACHE BOOL "" FORCE)
set(ENABLE_ZLIB_COMPRESSION
    OFF
    CACHE BOOL "" FORCE)

if(WIN32)
  set(CRYPTO_BACKEND
      "WinCNG"
      CACHE STRING "" FORCE)
else()
  set(CRYPTO_BACKEND
      "OpenSSL"
      CACHE STRING "" FORCE)
endif()

FetchContent_Declare(
  libssh2
  GIT_REPOSITORY https://github.com/libssh2/libssh2.git
  GIT_TAG libssh2-1.11.1)

FetchContent_MakeAvailable(libssh2)

# OpenCV custom build (with GStreamer support)
FetchContent_Declare(
  opencv_custom
  URL "https://github.com/Z-TAS-Solutions/ZScanner/releases/download/v0.1.0/opencv-4.9.0-gst-x64.zip"
  URL_HASH
    SHA256=2E86023D6B71B8B9EEA0888A1896AF1FA91395FC819A0C0D7E99BA2A3101658D)

FetchContent_MakeAvailable(opencv_custom)

add_executable(
  ZScanner WIN32
  src/main.cpp
  src/RendererCore.cpp
  src/WinForge.cpp
  src/Renderer.cpp
  external/include/DirecXTK/DDSTextureLoader.cpp
  src/ZScanCore.cpp
  src/ZLogger.cpp
  src/ZScanOV.cpp
  src/SSHHandler.cpp
  src/ZCore.cpp)

target_include_directories(
  ZScanner
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/include
          ${CMAKE_CURRENT_SOURCE_DIR}/external/include/DirecXTK
          ${opencv_custom_SOURCE_DIR}/include
          ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
  ZScanner
  PRIVATE imgui
          libssh2::libssh2_static
          debug
          "${opencv_custom_SOURCE_DIR}/lib/Debug/opencv_world490d.lib"
          optimized
          "${opencv_custom_SOURCE_DIR}/lib/Release/opencv_world490.lib")

if(WIN32)
  target_link_libraries(ZScanner PRIVATE d3d11 d3dcompiler dwmapi dxgi ws2_32)
endif()

target_compile_definitions(
  ZScanner
  PUBLIC $<$<CONFIG:Debug>:ZSCAN_BUILD_DEBUG>
         $<$<CONFIG:Release>:ZSCAN_BUILD_RELEASE>
  PRIVATE WIN32_LEAN_AND_MEAN)

add_custom_command(
  TARGET ZScanner
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -E copy_directory
    "${opencv_custom_SOURCE_DIR}/dll/$<CONFIG>" $<TARGET_FILE_DIR:ZScanner>)

add_dependencies(ZScanner CompileShaders)

# Automatic compile commands to root for nvim. The End.
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  if(CMAKE_HOST_WIN32)
    file(
      WRITE "${CMAKE_SOURCE_DIR}/.clangd"
      "CompileFlags:
  CompilationDatabase: \"${CMAKE_BINARY_DIR}\"
")
  else()
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/compile_commands.json")
      execute_process(
        COMMAND
          ${CMAKE_COMMAND} -E create_symlink
          "${CMAKE_BINARY_DIR}/compile_commands.json"
          "${CMAKE_SOURCE_DIR}/compile_commands.json")
    endif()
  endif()
endif()
