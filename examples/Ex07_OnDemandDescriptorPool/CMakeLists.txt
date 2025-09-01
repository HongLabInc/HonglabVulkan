cmake_minimum_required(VERSION 3.10)
project(Ex07_OnDemandDescriptorPool LANGUAGES CXX)

add_executable(Ex07_OnDemandDescriptorPool Ex07_OnDemandDescriptorPool.cpp)
target_link_libraries(Ex07_OnDemandDescriptorPool PRIVATE Engine)
target_include_directories(Ex07_OnDemandDescriptorPool PRIVATE 
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/include
)
set_target_properties(Ex07_OnDemandDescriptorPool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
