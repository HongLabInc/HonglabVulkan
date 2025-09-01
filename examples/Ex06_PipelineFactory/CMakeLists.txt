cmake_minimum_required(VERSION 3.10)
project(Ex06_PipelineFactory LANGUAGES CXX)

add_executable(Ex06_PipelineFactory Ex06_PipelineFactory.cpp)
target_link_libraries(Ex06_PipelineFactory PRIVATE Engine)
target_include_directories(Ex06_PipelineFactory PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/include
)
set_target_properties(Ex06_PipelineFactory PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
