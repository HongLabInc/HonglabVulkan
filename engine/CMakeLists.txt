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
    DescriptorPool.cpp
    DescriptorPool.h
    DescriptorSet.cpp
    DescriptorSet.h
    GpuTimer.cpp
    GpuTimer.h
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
    BindingInfo.h
    ShadowMapPipeline.h
    PbrDeferredPipeline.h
    SkyPipeline.h
    DeferredLightingPipeline.h
    PostPipeline.h
    GuiPipeline.h
    Pipeline.cpp
    Pipeline.h
    PipelineCompute.cpp
    PipelineConfig.cpp
    PipelineConfig.h
    PushConstants.cpp
    PushConstants.h
    RenderGraph.cpp
    RenderGraph.h
    Renderer.cpp
    Renderer.h
    Resource.cpp
    Resource.h
    ResourceBinding.cpp
    ResourceBinding.h
    Sampler.cpp
    Sampler.h
    Shader.cpp
    Shader.h
    ShaderManager.cpp
    ShaderManager.h
    Skeleton.cpp
    Skeleton.h
    StorageBuffer.cpp
    StorageBuffer.h
    Swapchain.cpp
    Swapchain.h
    TextureManager.cpp
    TextureManager.h
    TracyProfiler.cpp
    TracyProfiler.h
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
)

# Compiler definitions
target_compile_definitions(Engine PUBLIC
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLFW_INCLUDE_VULKAN
)

if(MSVC)
    target_compile_definitions(Engine PUBLIC NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

# Tracy profiler support - Only enable if Tracy is actually available
find_package(Tracy QUIET CONFIG)
if(TARGET Tracy::TracyClient)
    target_compile_definitions(Engine PUBLIC
        $<$<CONFIG:Debug,RelWithDebInfo>:TRACY_ENABLE>
        $<$<CONFIG:Debug,RelWithDebInfo>:TRACY_ON_DEMAND>
    )
    target_link_libraries(Engine PUBLIC Tracy::TracyClient)
    message(STATUS "Engine: Tracy profiler available (enabled in Debug/RelWithDebInfo)")
else()
    message(STATUS "Engine: Tracy profiler not available")
endif()

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

# Handle KTX
if(TARGET ktx::ktx)
    target_link_libraries(Engine PUBLIC ktx::ktx)
    target_compile_definitions(Engine PUBLIC HAS_KTX)
    message(STATUS "Engine: Using KTX library (manual build)")
elseif(TARGET KTX::ktx)
    target_link_libraries(Engine PUBLIC KTX::ktx)
    target_compile_definitions(Engine PUBLIC HAS_KTX)
    message(STATUS "Engine: Using KTX library (vcpkg)")
else()
    message(STATUS "Engine: KTX library not found")
endif()

# C++ standard
target_compile_features(Engine PUBLIC cxx_std_20)

# MSVC options
if(MSVC)
    target_compile_options(Engine PUBLIC /MP)
endif()

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
