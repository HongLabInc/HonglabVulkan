add_executable(Ex14_Bistro
    Ex14_Bistro.cpp
)

# Link against the engine
target_link_libraries(Ex14_Bistro PRIVATE Engine)

# Set output directory
set_target_properties(Ex14_Bistro PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
