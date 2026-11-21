cmake_minimum_required(VERSION 3.20)
project(NodeWanderer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

add_executable(NodeWanderer
    src/main.cpp
    src/Game.cpp
    src/GameScene.cpp
    src/Player.cpp
    src/World.cpp
    src/Scenery.cpp
    src/ForestArt.cpp
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
