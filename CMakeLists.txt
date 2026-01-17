cmake_minimum_required(VERSION 3.20)
project(ITCH_REPLAY LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -O3 -march=native -g")
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(DPDK REQUIRED libdpdk)

add_executable(run
    src/main.cpp
    src/spsc_buffer.cpp
    src/mold_udp_64.cpp
)

target_include_directories(run PRIVATE include)
target_link_libraries(run PRIVATE ${DPDK_LIBRARIES})
target_compile_options(run PRIVATE ${DPDK_CFLAGS_OTHER})

