cmake_minimum_required(VERSION 3.21)

project(HonglabVulkan VERSION 1.0.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Linux: find KTX manually BEFORE vcpkg setup
if(UNIX)
    set(KTX_ROOT "/mnt/d/UbuntuTestFolder/KTX-Software")
    if(EXISTS "${KTX_ROOT}")
        find_path(KTX_INCLUDE_DIR
            NAMES ktx.h
            PATHS "${KTX_ROOT}/include"
            NO_DEFAULT_PATH
        )
        find_library(KTX_LIBRARY
            NAMES ktx libktx
            PATHS "${KTX_ROOT}/build/Release"
            NO_DEFAULT_PATH
        )
        if(KTX_INCLUDE_DIR AND KTX_LIBRARY)
            set(KTX_FOUND TRUE)
            message(STATUS "Found manually built KTX: ${KTX_LIBRARY}")
            add_library(ktx::ktx SHARED IMPORTED)
            set_target_properties(ktx::ktx PROPERTIES
                IMPORTED_LOCATION "${KTX_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${KTX_INCLUDE_DIR}"
            )
        endif()
    endif()
endif()

# Vcpkg toolchain (if not already set via preset or command line)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    elseif(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
    endif()
endif()

# Find required packages
find_package(Vulkan REQUIRED)

# Find packages through vcpkg
find_package(glfw3 REQUIRED CONFIG)
find_package(glm REQUIRED CONFIG)
find_package(assimp REQUIRED CONFIG)

find_package(imgui QUIET CONFIG)

find_package(unofficial-spirv-reflect QUIET CONFIG)

find_package(Stb QUIET CONFIG)

# KTX: on Windows use vcpkg, on Linux use manual build (already set above)
if(NOT TARGET ktx::ktx)
    find_package(Ktx QUIET CONFIG)
endif()

# Find Tracy profiler through vcpkg
find_package(Tracy QUIET CONFIG)
if(Tracy_FOUND)
    message(STATUS "Found Tracy profiler via vcpkg")
else()
    message(STATUS "Tracy profiler not found - will be disabled")
endif()

# Add subdirectories
add_subdirectory(engine)
add_subdirectory(examples)
