cmake_minimum_required(VERSION 3.14)
project(SilentHillLevelEditor CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure FetchContent to place source in 'libs' directory
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/libs)

# 1. Fetch Raylib
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG master # Or a specific version tag
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/libs/raylib
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# 2. Fetch ImGui (docking branch)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking # Use the docking branch
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/libs/imgui
)
FetchContent_MakeAvailable(imgui)

# 3. Fetch rlImGui
FetchContent_Declare(
    rlImGui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG main
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/libs/rlImGui
)
FetchContent_MakeAvailable(rlImGui)


# Core Source Files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")

if(WIN32)
    set(RESOURCE_FILE "${CMAKE_SOURCE_DIR}/res/resources.rc")

    list(APPEND SOURCES ${RESOURCE_FILE})
    set_source_files_properties(${RESOURCE_FILE}
        PROPERTIES
        INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/res"
    )
endif()

# Define our executable
add_executable(${PROJECT_NAME} ${SOURCES})

# ImGui source files (ImGui doesn't have a native CMakeLists, so we add its sources to our target)
target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/libs/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/libs/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/libs/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/libs/imgui/imgui_widgets.cpp
    # rlImGui sources
    ${CMAKE_SOURCE_DIR}/libs/rlImGui/rlImGui.cpp
)

# Includes
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/libs/imgui
    ${CMAKE_SOURCE_DIR}/libs/rlImGui
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

# MinGW/Windows specific linking (to avoid console window in release, if desired)
if (WIN32)
    # Ensure correct subsystem and link required Windows libraries
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm)
endif()

if (MINGW)
    target_link_options(${PROJECT_NAME} PRIVATE -static -static-libgcc -static-libstdc++)
endif()
