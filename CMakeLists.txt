cmake_minimum_required(VERSION 3.20)
project(
    NetBucket
    VERSION 0.1.0
    DESCRIPTION "Token-Bucket Network Bandwidth Shaper & Traffic Policing Engine"
    LANGUAGES CXX
)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)          # Use -std=c++20, not -std=gnu++20
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wold-style-cast
)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
find_package(GTest REQUIRED)
find_package(nlohmann_json REQUIRED)
enable_testing()
include(GoogleTest)
add_subdirectory(src)
add_subdirectory(tests)
