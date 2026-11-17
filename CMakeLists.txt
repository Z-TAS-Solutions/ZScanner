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
          ${CMAKE_CURRENT_SOURCE_DIR}/external/include/libssh2-1.11.1
          ${CMAKE_CURRENT_SOURCE_DIR}/external/include/DirecXTK
          ${CMAKE_CURRENT_SOURCE_DIR}/external/include/opencv2
          ${CMAKE_CURRENT_SOURCE_DIR}/src
          ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB EXTERNAL_DLIBS
     "${CMAKE_CURRENT_SOURCE_DIR}/external/lib/Debug/${architecture}/*.lib")

file(GLOB EXTERNAL_LIBS
     "${CMAKE_CURRENT_SOURCE_DIR}/external/lib/Release/${architecture}/*.lib")

file(GLOB EXTERNAL_ULIBS
     "${CMAKE_CURRENT_SOURCE_DIR}/external/lib/Universal/${architecture}/*.lib")

message(STATUS "DEBUG LIBS FOUND: ${EXTERNAL_DLIBS}")
message(STATUS "RELEASE LIBS FOUND: ${EXTERNAL_LIBS}")

target_link_libraries(
  ZScanner PRIVATE imgui ${EXTERNAL_ULIBS} debug ${EXTERNAL_DLIBS} optimized
                   ${EXTERNAL_LIBS})

target_compile_definitions(
  ZScanner
  PUBLIC $<$<CONFIG:Debug>:ZSCAN_BUILD_DEBUG>
         $<$<CONFIG:Release>:ZSCAN_BUILD_RELEASE>
  PRIVATE WIN32_LEAN_AND_MEAN)

set(DLL_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/external/dll/$<CONFIG>/${architecture}")

add_custom_command(
  TARGET ZScanner
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${DLL_PATH}"
          $<TARGET_FILE_DIR:ZScanner>)

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
