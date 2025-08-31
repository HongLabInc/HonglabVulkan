cmake_minimum_required(VERSION 3.10)
project(Ex02_Compute)

add_executable(Ex02_Compute Ex02_Compute.cpp)
target_link_libraries(Ex02_Compute PRIVATE Engine)
target_include_directories(Ex02_Compute PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/include
)

set_target_properties(Ex02_Compute PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
