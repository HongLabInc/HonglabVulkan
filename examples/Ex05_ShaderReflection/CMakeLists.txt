cmake_minimum_required(VERSION 3.10)
project(Ex05_ShaderReflection LANGUAGES CXX)

add_executable(Ex05_ShaderReflection Ex05_ShaderReflection.cpp)
target_link_libraries(Ex05_ShaderReflection PRIVATE Engine)
target_include_directories(Ex05_ShaderReflection PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/include
)
set_target_properties(Ex05_ShaderReflection PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
