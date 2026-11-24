cmake_minimum_required(VERSION 3.20)
project(NodeWanderer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# No extra DLLs: raylib is built as a static library, and the MSVC runtime (C runtime and
# C++ standard library) is linked statically (/MT). The exe only uses parts of Windows itself.
# Its music and sounds live in assets/audio, which the build copies beside the exe.
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
    src/GameSound.cpp
    src/Audio.cpp
    src/Player.cpp
    src/Effects.cpp
    src/World.cpp
    src/ArtCommon.cpp
    src/Paint.cpp
    src/Lighting.cpp
    src/ForestArt.cpp
    src/RuinsArt.cpp
    src/GardenArt.cpp
    src/EdgeArt.cpp
    src/SinglyLinkedList.cpp
    src/DoublyLinkedList.cpp
    src/CircularSinglyLinkedList.cpp
)
target_link_libraries(NodeWanderer PRIVATE raylib)

# The music and sounds are files (assets/audio, made by tools/audio/build_audio.py). Every build
# copies them beside the exe, also when only a sound changed, so build/NodeWanderer.exe runs with
# sound from any folder. The game looks for assets/audio next to the exe first, then in the
# working folder.
add_custom_target(NodeWandererAudio ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/audio" "$<TARGET_FILE_DIR:NodeWanderer>/assets/audio"
    COMMENT "Copying assets/audio beside the executable"
    VERBATIM)
add_dependencies(NodeWandererAudio NodeWanderer)

# A folder to share the game (the exe and assets/audio only, without raylib's own headers and
# library): cmake --install build --prefix dist --component game
install(TARGETS NodeWanderer RUNTIME DESTINATION . COMPONENT game)
install(DIRECTORY assets/audio DESTINATION assets COMPONENT game)

if (MSVC)
    target_compile_options(NodeWanderer PRIVATE /W4)
else()
    target_compile_options(NodeWanderer PRIVATE -Wall -Wextra)
endif()
