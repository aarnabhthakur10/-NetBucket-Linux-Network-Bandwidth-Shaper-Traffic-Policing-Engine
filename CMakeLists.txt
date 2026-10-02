cmake_minimum_required(VERSION 3.20)

project(
    NetBucket
    VERSION 0.1.0
    DESCRIPTION "Token-Bucket Network Bandwidth Shaper & Traffic Policing Engine"
    LANGUAGES CXX
)

# ─── Language standard ────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)          # Use -std=c++20, not -std=gnu++20

# ─── Compiler warnings ────────────────────────────────────────────────────────
# Applied only to our own targets (not to fetched dependencies)
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

# ─── Build type default ───────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# ─── Dependencies via FetchContent ────────────────────────────────────────────
include(FetchContent)

# GoogleTest  ─────────────────────────────────────────────────────────────────
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
# Prevent GoogleTest from installing itself system-wide
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
# Prevent GoogleTest from overriding our compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# nlohmann/json  ──────────────────────────────────────────────────────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ─── Enable CTest ────────────────────────────────────────────────────────────
enable_testing()
include(GoogleTest)

# ─── Source tree ─────────────────────────────────────────────────────────────
add_subdirectory(src)

# ─── Tests ───────────────────────────────────────────────────────────────────
add_subdirectory(tests)
