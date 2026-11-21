cmake_minimum_required(VERSION 3.20)
project(NodeWanderer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# One self-contained executable: raylib is built as a static library, and the MSVC
# runtime (C runtime and C++ standard library) is linked statically (/MT), so the
# exe needs no extra DLLs. It only uses parts of Windows itself (OpenGL, user32, ...).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Download and build raylib automatically (no manual install needed).
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(raylib)
set_property(TARGET raylib PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(NodeWanderer
    src/main.cpp
    src/Game.cpp
    src/GameScene.cpp
    src/GameUi.cpp
    src/Player.cpp
    src/Effects.cpp
    src/World.cpp
    src/ArtCommon.cpp
    src/ForestArt.cpp
    src/RuinsArt.cpp
    src/GardenArt.cpp
    src/EdgeArt.cpp
    src/SinglyLinkedList.cpp
    src/DoublyLinkedList.cpp
    src/CircularSinglyLinkedList.cpp
)
target_link_libraries(NodeWanderer PRIVATE raylib)

if (MSVC)
    target_compile_options(NodeWanderer PRIVATE /W4)
else()
    target_compile_options(NodeWanderer PRIVATE -Wall -Wextra)
endif()
