cmake_minimum_required(VERSION 3.10)
project(Ex04_Triangle11 LANGUAGES CXX)

add_executable(Ex04_Triangle11 Ex04_Triangle11.cpp)
target_link_libraries(Ex04_Triangle11 PRIVATE Engine)
target_include_directories(Ex04_Triangle11 PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/include
)
set_target_properties(Ex04_Triangle11 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
