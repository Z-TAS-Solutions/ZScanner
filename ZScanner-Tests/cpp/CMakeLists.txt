cmake_minimum_required(VERSION 3.18)
project(ZScanner LANGUAGES CXX)


set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/debug/VertexShader.cso
    COMMAND fxc /T vs_5_0 /E VSMain /Fo ${CMAKE_CURRENT_BINARY_DIR}/debug/VertexShader.cso /Fc vsshaderAssembly.txt ${CMAKE_CURRENT_SOURCE_DIR}/src/vertex_shader.hlsl
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/vertex_shader.hlsl
    VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/debug/PixelShader.cso
    COMMAND fxc /T ps_5_0 /E PSMain /Fo ${CMAKE_CURRENT_BINARY_DIR}/debug/PixelShader.cso /Fc psshaderAssembly.txt ${CMAKE_CURRENT_SOURCE_DIR}/src/pixel_shader.hlsl
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/pixel_shader.hlsl
    VERBATIM

)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/debug)

add_custom_target(CompileShaders ALL
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/debug/VertexShader.cso
        ${CMAKE_CURRENT_BINARY_DIR}/debug/PixelShader.cso
)


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include/opencv2)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include/imgui)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/include/imgui/backends)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})




if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(architecture x64)
else()
    set(architecture x86)
    #set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/lib/x86/)
endif()




if(WIN32)
    set(SourceDir ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/windows)
else()
    set(SourceDir ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/unix)
endif()

add_executable(main WIN32 
    src/main.cpp
    src/RendererCore.cpp
    src/WinForge.cpp
    src/Renderer.cpp
    external/include/imgui/imgui.cpp
    external/include//imgui/imgui_draw.cpp
    external/include/imgui/imgui_tables.cpp
    external/include/imgui/imgui_widgets.cpp
    external/include/imgui/imgui_demo.cpp
    external/include/imgui/backends/imgui_impl_win32.cpp
    external/include/imgui/backends/imgui_impl_dx11.cpp
    src/ZScanCore.cpp
    src/ZLogger.cpp
    src/ZScanOV.cpp
)


file(GLOB EXTERNAL_DLIBS
    "${CMAKE_CURRENT_SOURCE_DIR}/external/lib/Debug/${architecture}/*.lib"
)

file(GLOB EXTERNAL_LIBS
    "${CMAKE_CURRENT_SOURCE_DIR}/external/lib/Release/${architecture}/*.lib"
)

file(GLOB EXTERNAL_ULIBS
    "${CMAKE_CURRENT_SOURCE_DIR}/external/lib/Universal/${architecture}/*.lib"
)


target_link_libraries(main PRIVATE 
    ${EXTERNAL_ULIBS}
    debug ${EXTERNAL_DLIBS}
    optimized ${EXTERNAL_LIBS}
)


target_compile_definitions(main PUBLIC 
    $<$<CONFIG:Debug>:ZSCAN_BUILD_DEBUG>
    $<$<CONFIG:Release>:ZSCAN_BUILD_RELEASE>
    
)

set(DLL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/dll/$<CONFIG>/${architecture}")

add_custom_command(
    TARGET main 
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${DLL_PATH}"
        $<TARGET_FILE_DIR:main>
)

add_dependencies(main CompileShaders)


