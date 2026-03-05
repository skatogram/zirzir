cmake_minimum_required(VERSION 3.10)
project(oxem_gg VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set directories
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Force Unicode
add_definitions(-DUNICODE -D_UNICODE)

# Disable console window in Release builds
if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
endif()

# ImGui sources - being very explicit
file(GLOB IMGUI_SOURCES 
    "${IMGUI_DIR}/imgui*.cpp"
)
list(APPEND IMGUI_SOURCES 
    "${IMGUI_DIR}/backends/imgui_impl_dx11.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
)

# Project sources
file(GLOB PROJECT_SOURCES 
    "${SRC_DIR}/*.cpp"
    "${SRC_DIR}/*.h"
)

add_executable(oxem_gg ${IMGUI_SOURCES} ${PROJECT_SOURCES})

# Include directories - added multiple variants to be safe
target_include_directories(oxem_gg PRIVATE 
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${SRC_DIR}
)

# Libraries
target_link_libraries(oxem_gg PRIVATE
    d3d11
    d3dcompiler
    dxgi
    dwmapi
    user32
    gdi32
)
