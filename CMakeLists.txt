cmake_minimum_required(VERSION 3.28)
project(HWiNFO_Analysis)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(WINDOWS_WINDOW WIN32)
else()
    set(WINDOWS_WINDOW "")
endif ()

add_executable(${PROJECT_NAME} ${WINDOWS_WINDOW} main.cpp
        GUI/gui.cpp
        GUI/gui.h
        External/ImGui/imgui_widgets.cpp
        External/ImGui/imgui_tables.cpp
        External/ImGui/imgui_demo.cpp
        External/ImGui/imgui_draw.cpp
        External/ImGui/imgui.cpp
        External/ImGui/imgui_impl_dx12.cpp
        External/ImGui/imgui_impl_win32.cpp
        External/ImGui/implot_demo.cpp
        External/ImGui/implot_items.cpp
        External/ImGui/implot.cpp
        CSV_Helper.cpp
        CSV_Helper.h)

add_compile_options(-O3)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Ox")