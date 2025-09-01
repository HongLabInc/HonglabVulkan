cmake_minimum_required(VERSION 3.21)

project(HonglabVulkan VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Workaround for AppleClang 17.0.0 compatibility with CMake
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "17.0.0")
    # Set default language standards for AppleClang 17.0.0
    set(CMAKE_C_STANDARD_COMPUTED_DEFAULT "17")
    set(CMAKE_C_EXTENSIONS_COMPUTED_DEFAULT "ON")
    set(CMAKE_CXX_STANDARD_COMPUTED_DEFAULT "17")
    set(CMAKE_CXX_EXTENSIONS_COMPUTED_DEFAULT "ON")
endif()

# Disable C language support to avoid feature detection issues
set(CMAKE_C_COMPILER_LOADED FALSE)

# Find required packages
find_package(Vulkan REQUIRED)

# Find vcpkg packages
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(unofficial-spirv-reflect CONFIG REQUIRED)
find_package(Ktx CONFIG REQUIRED)
find_package(Stb REQUIRED)

# Add subdirectories
add_subdirectory(engine)
add_subdirectory(examples)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../lib)
