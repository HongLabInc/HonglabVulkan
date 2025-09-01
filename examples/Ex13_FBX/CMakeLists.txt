cmake_minimum_required(VERSION 3.10)
project(Ex13_FBX LANGUAGES CXX)

add_executable(Ex13_FBX Ex13_FBX.cpp)
target_link_libraries(Ex13_FBX PRIVATE Engine)
target_include_directories(Ex13_FBX PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/include
)
set_target_properties(Ex13_FBX PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
