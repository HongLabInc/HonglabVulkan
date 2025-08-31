cmake_minimum_required(VERSION 3.10)
project(Ex10_Skybox)

add_executable(Ex10_Skybox 
    Ex10_Skybox.cpp
    Ex10_Example.cpp
)
target_link_libraries(Ex10_Skybox PRIVATE Engine)
target_include_directories(Ex10_Skybox PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/include
)
set_target_properties(Ex10_Skybox PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
