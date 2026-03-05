cmake_minimum_required(VERSION 3.10)
project(oxem_gg VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Disable console window in Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

# ImGui sources
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui)
file(GLOB IMGUI_SOURCES 
    ${IMGUI_DIR}/*.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
)

# Our sources
file(GLOB PROJECT_SOURCES src/*.cpp src/*.h)

add_executable(oxem_gg ${IMGUI_SOURCES} ${PROJECT_SOURCES})

target_include_directories(oxem_gg PRIVATE 
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(oxem_gg PRIVATE
    d3d11
    d3dcompiler
    dxgi
    dwmapi
    user32
    gdi32
)
