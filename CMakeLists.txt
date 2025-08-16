cmake_minimum_required(VERSION 3.21)

project(HonglabVulkan VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# First, find KTX manually BEFORE vcpkg setup
set(KTX_ROOT "/mnt/d/UbuntuTestFolder/KTX-Software")
if(EXISTS "${KTX_ROOT}")
    # Look for KTX headers in include directory
    find_path(KTX_INCLUDE_DIR 
        NAMES ktx.h
        PATHS "${KTX_ROOT}/include"
        NO_DEFAULT_PATH
    )
    
    # Look for KTX library in build/Release directory
    find_library(KTX_LIBRARY
        NAMES ktx libktx
        PATHS "${KTX_ROOT}/build/Release"
        NO_DEFAULT_PATH
    )
    
    if(KTX_INCLUDE_DIR AND KTX_LIBRARY)
        set(KTX_FOUND TRUE)
        message(STATUS "Found manually built KTX: ${KTX_LIBRARY}")
        message(STATUS "KTX include directory: ${KTX_INCLUDE_DIR}")
        
        # Create imported target
        add_library(ktx::ktx SHARED IMPORTED)
        set_target_properties(ktx::ktx PROPERTIES
            IMPORTED_LOCATION "${KTX_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${KTX_INCLUDE_DIR}"
        )
        
        # Prevent vcpkg from overriding our KTX
        set(VCPKG_SKIP_FIND_PACKAGE_ktx ON)
        set(VCPKG_SKIP_FIND_PACKAGE_unofficial-ktx ON)
    endif()
endif()

# Now enable vcpkg for other packages (but not KTX)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" 
        CACHE STRING "Vcpkg toolchain file")
    message(STATUS "Using vcpkg toolchain: ${CMAKE_TOOLCHAIN_FILE}")
elseif(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
    message(STATUS "Using vcpkg from environment: ${CMAKE_TOOLCHAIN_FILE}")
endif()

# Find required packages
find_package(Vulkan REQUIRED)
find_package(PkgConfig)

# Find packages through vcpkg using correct package names
find_package(glfw3 REQUIRED CONFIG)
find_package(glm REQUIRED CONFIG)
find_package(assimp REQUIRED CONFIG)

# ImGui uses a different package name in vcpkg
find_package(imgui QUIET CONFIG)
if(NOT imgui_FOUND)
    # Try alternative names
    find_package(ImGui QUIET CONFIG)
endif()

# Find spirv-reflect (this should come from vcpkg)
find_package(spirv-reflect QUIET CONFIG)
if(NOT spirv-reflect_FOUND)
    find_package(unofficial-spirv-reflect QUIET CONFIG)
endif()

find_package(Stb QUIET CONFIG)

# Add subdirectories
add_subdirectory(engine)
add_subdirectory(examples)
