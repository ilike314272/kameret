cmake_minimum_required(VERSION 3.25)
project(kameret VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── vcpkg deps ──────────────────────────────────────────────────────────────
find_package(OpenCV REQUIRED)
find_package(fmt    REQUIRED)
find_package(Eigen3 REQUIRED NO_MODULE)

# ── Platform detection ──────────────────────────────────────────────────────
if(WIN32)
    set(KAMERET_PLATFORM windows)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(KAMERET_PLATFORM linux_arm64)
else()
    set(KAMERET_PLATFORM linux_x86_64)
endif()
message(STATUS "kameret platform: ${KAMERET_PLATFORM}")

# ── Subprojects ─────────────────────────────────────────────────────────────
add_subdirectory(core)
add_subdirectory(platforms/${KAMERET_PLATFORM})
add_subdirectory(adapters/mediapipe)
add_subdirectory(apps/demo)
