cmake_minimum_required(VERSION 3.10)
project(Ex12_PBR)

add_executable(Ex12_PBR Ex12_PBR.cpp)
target_link_libraries(Ex12_PBR PRIVATE Engine)
target_include_directories(Ex12_PBR PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/include
)
set_target_properties(Ex12_PBR PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
