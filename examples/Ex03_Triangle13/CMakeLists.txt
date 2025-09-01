cmake_minimum_required(VERSION 3.10)
project(Ex03_Triangle13 LANGUAGES CXX)

add_executable(Ex03_Triangle13 Ex03_Triangle13.cpp)
target_link_libraries(Ex03_Triangle13 PRIVATE Engine)
target_include_directories(Ex03_Triangle13 PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/include
)

set_target_properties(Ex03_Triangle13 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
