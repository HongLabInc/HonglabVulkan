# Engine static library
add_library(Engine STATIC
    Animation.cpp
    Animation.h
    Application.cpp
    Application.h
    BarrierHelper.cpp
    BarrierHelper.h
    Camera.cpp
    Camera.h
    CommandBuffer.cpp
    CommandBuffer.h
    Context.cpp
    Context.h
    DepthStencil.cpp
    DepthStencil.h
    DescriptorPool.cpp
    DescriptorPool.h
    DescriptorSet.cpp
    DescriptorSet.h
    GuiRenderer.cpp
    GuiRenderer.h
    Image2D.cpp
    Image2D.h
    Logger.cpp
    Logger.h
    MappedBuffer.cpp
    MappedBuffer.h
    Material.cpp
    Material.h
    Mesh.cpp
    Mesh.h
    Model.cpp
    Model.h
    ModelLoader.cpp
    ModelLoader.h
    ModelNode.cpp
    ModelNode.h
    Pipeline.cpp
    Pipeline.h
    PipelineCompute.cpp
    PipelineGui.cpp
    PipelinePbrDeferred.cpp
    PipelinePbrForward.cpp
    PipelinePost.cpp
    PipelineShadowMap.cpp
    PipelineSky.cpp
    PipelineSsao.cpp
    PipelineTriangle.cpp
    PushConstants.cpp
    PushConstants.h
    Renderer.cpp
    Renderer.h
    ResourceBinding.cpp
    ResourceBinding.h
    Sampler.cpp
    Sampler.h
    Shader.cpp
    Shader.h
    ShaderManager.cpp
    ShaderManager.h
    ShadowMap.cpp
    ShadowMap.h
    Skeleton.cpp
    Skeleton.h
    SkyTextures.cpp
    SkyTextures.h
    StorageBuffer.cpp
    StorageBuffer.h
    Swapchain.cpp
    Swapchain.h
    UniformBuffer.cpp
    UniformBuffer.h
    Vertex.cpp
    Vertex.h
    ViewFrustum.cpp
    ViewFrustum.h
    VulkanTools.cpp
    VulkanTools.h
    Window.cpp
    Window.h
)

# Set include directories
target_include_directories(Engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux/include
)

# Add compiler definitions to fix C++20 compatibility issues
target_compile_definitions(Engine PUBLIC
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLFW_INCLUDE_VULKAN
)

# Link required dependencies
target_link_libraries(Engine PUBLIC Vulkan::Vulkan)

# Link optional dependencies if found
if(TARGET glfw)
    target_link_libraries(Engine PUBLIC glfw)
elseif(TARGET glfw3::glfw3)
    target_link_libraries(Engine PUBLIC glfw3::glfw3)
endif()

if(TARGET glm::glm)
    target_link_libraries(Engine PUBLIC glm::glm)
endif()

if(TARGET assimp::assimp)
    target_link_libraries(Engine PUBLIC assimp::assimp)
elseif(ASSIMP_FOUND)
    target_include_directories(Engine PUBLIC ${ASSIMP_INCLUDE_DIRS})
    target_link_libraries(Engine PUBLIC ${ASSIMP_LIBRARIES})
endif()

if(TARGET imgui::imgui)
    target_link_libraries(Engine PUBLIC imgui::imgui)
endif()

# Handle spirv-reflect with multiple possible target names
if(TARGET unofficial::spirv-reflect)
    target_link_libraries(Engine PUBLIC unofficial::spirv-reflect)
    target_compile_definitions(Engine PUBLIC HAS_SPIRV_REFLECT)
elseif(TARGET spirv-reflect::spirv-reflect)
    target_link_libraries(Engine PUBLIC spirv-reflect::spirv-reflect)
    target_compile_definitions(Engine PUBLIC HAS_SPIRV_REFLECT)
endif()

# Handle KTX - prioritize manually built KTX over vcpkg
if(TARGET ktx::ktx)
    # Use manually built KTX from /mnt/d/UbuntuTestFolder/KTX-Software
    target_link_libraries(Engine PUBLIC ktx::ktx)
    target_compile_definitions(Engine PUBLIC HAS_KTX)
    message(STATUS "Engine: Using manually built KTX library from /mnt/d/UbuntuTestFolder/KTX-Software")
elseif(TARGET unofficial::ktx::ktx)
    # Fallback to vcpkg KTX if available
    target_link_libraries(Engine PUBLIC unofficial::ktx::ktx)
    target_compile_definitions(Engine PUBLIC HAS_KTX)
    message(STATUS "Engine: Using vcpkg KTX library")
else()
    message(STATUS "Engine: KTX library not found")
endif()

# Compiler-specific options (changed to C++20 to match main CMakeLists.txt)
target_compile_features(Engine PUBLIC cxx_std_20)

# Platform-specific settings
if(UNIX AND NOT APPLE)
    # Linux
    target_compile_definitions(Engine PUBLIC LINUX)
elseif(APPLE)
    # macOS
    target_compile_definitions(Engine PUBLIC MACOS)
    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(IOKIT_LIBRARY IOKit REQUIRED)
    find_library(COREVIDEO_LIBRARY CoreVideo REQUIRED)
    target_link_libraries(Engine PUBLIC ${COCOA_LIBRARY} ${IOKIT_LIBRARY} ${COREVIDEO_LIBRARY})
endif()
