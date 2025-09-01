cmake_minimum_required(VERSION 3.10)
project(Ex08_Swapchain LANGUAGES CXX)

add_executable(Ex08_Swapchain Ex08_Swapchain.cpp)
target_link_libraries(Ex08_Swapchain PRIVATE Engine)
target_include_directories(Ex08_Swapchain PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/include
)
set_target_properties(Ex08_Swapchain PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
