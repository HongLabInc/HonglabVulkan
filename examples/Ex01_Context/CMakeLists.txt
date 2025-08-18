add_executable(Ex01_Context
    Ex01_Context.cpp
)

# Link against the engine
target_link_libraries(Ex01_Context PRIVATE Engine)

# Set output directory
set_target_properties(Ex01_Context PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
