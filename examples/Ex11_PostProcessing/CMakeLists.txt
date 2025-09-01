cmake_minimum_required(VERSION 3.10)
project(Ex11_PostProcessing LANGUAGES CXX)

add_executable(Ex11_PostProcessing 
    Ex11_PostProcessing.cpp
    Ex11_PostProcessingExample.cpp
)
target_link_libraries(Ex11_PostProcessing PRIVATE Engine)
target_include_directories(Ex11_PostProcessing PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/include
)
set_target_properties(Ex11_PostProcessing PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
