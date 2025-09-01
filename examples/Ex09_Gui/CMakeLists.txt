cmake_minimum_required(VERSION 3.10)
project(Ex09_Gui LANGUAGES CXX)

add_executable(Ex09_Gui Ex09_Gui.cpp)
target_link_libraries(Ex09_Gui PRIVATE Engine)
target_include_directories(Ex09_Gui PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/include
)
set_target_properties(Ex09_Gui PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
