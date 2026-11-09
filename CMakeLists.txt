cmake_minimum_required(VERSION 3.16)
project(tidewalker CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(tidewalker
    src/main.cpp
    src/game.cpp
    src/world.cpp
    src/renderer.cpp
    src/mesh.cpp
    src/texgen.cpp
    src/font.cpp
    src/png.cpp
    src/platform_linux.cpp
    src/platform_win32.cpp
)

if(WIN32)
    target_link_libraries(tidewalker PRIVATE gdi32 user32)
    target_compile_definitions(tidewalker PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_link_libraries(tidewalker PRIVATE z dl)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tidewalker PRIVATE -O3 -Wall)
endif()

install(TARGETS tidewalker RUNTIME DESTINATION bin)
