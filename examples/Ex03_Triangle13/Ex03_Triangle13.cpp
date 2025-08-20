#include "engine/Context.h"
#include "engine/Image2D.h"
#include "engine/CommandBuffer.h"
#include <stb_image.h>
#include <stb_image_write.h>
#include <glm/glm.hpp>
#include <fstream>
#include <string>

using namespace hlab;
using namespace std;

/**
 * @brief Reads a compiled SPIR-V shader binary file from disk
 *
 * SPIR-V (Standard Portable Intermediate Representation - Vulkan) is the bytecode format
 * that Vulkan uses for shaders. Unlike OpenGL which accepts GLSL source code directly,
 * Vulkan requires shaders to be pre-compiled into SPIR-V binary format.
 *
 * @param spvFilename Path to the .spv file (SPIR-V binary)
 * @return Vector containing the raw shader bytecode
 *
 * Why SPIR-V?
 * - Faster loading: No compilation at runtime
 * - Cross-platform: Same binary works on different drivers
 * - Optimization: Can be optimized offline
 * - Language agnostic: Can be generated from GLSL, HLSL, etc.
 */
vector<char> readSpvFile(const string& spvFilename)
{
    // Validate file extension - SPIR-V files must have .spv extension
    if (spvFilename.length() < 4 || spvFilename.substr(spvFilename.length() - 4) != ".spv") {
        exitWithMessage("Shader file does not have .spv extension: {}", spvFilename);
    }

    // Open file in binary mode with cursor at end (ios::ate) to get file size
    ifstream is(spvFilename, ios::binary | ios::in | ios::ate);
    if (!is.is_open()) {
        exitWithMessage("Could not open shader file: {}", spvFilename);
    }

    // Get file size and validate it's a valid SPIR-V file
    // SPIR-V files must be multiples of 4 bytes since they contain 32-bit words
    size_t shaderSize = static_cast<size_t>(is.tellg());
    if (shaderSize == 0 || shaderSize % 4 != 0) {
        exitWithMessage("Shader file size is invalid (must be >0 and multiple of 4): {}",
                        spvFilename);
    }

    // Reset cursor to beginning and read entire file into memory
    is.seekg(0, ios::beg);
    vector<char> shaderCode(shaderSize);
    is.read(shaderCode.data(), shaderSize);
    is.close();

    return shaderCode;
}

/**
 * @brief Creates a Vulkan shader module from SPIR-V bytecode
 *
 * A VkShaderModule is Vulkan's wrapper around SPIR-V bytecode. It's essentially
 * a handle that represents the shader code loaded into the GPU driver.
 *
 * Shader modules are:
 * - Immutable once created
 * - Lightweight objects that can be shared between pipelines
 * - Used to create pipeline shader stages
 *
 * @param device Vulkan logical device handle
 * @param shaderCode Raw SPIR-V bytecode as char vector
 * @return VkShaderModule handle that can be used in pipeline creation
 */
VkShaderModule createShaderModule(VkDevice device, const vector<char>& shaderCode)
{
    VkShaderModule shaderModule;

    // Create info structure describing how to create the shader module
    VkShaderModuleCreateInfo shaderModuleCI{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shaderModuleCI.codeSize = shaderCode.size();
    // Cast char* to uint32_t* because SPIR-V is composed of 32-bit words
    shaderModuleCI.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

    // Create the shader module - this loads the bytecode into the driver
    check(vkCreateShaderModule(device, &shaderModuleCI, nullptr, &shaderModule));
    return shaderModule;
}

int main()
{
    // File paths for our triangle rendering example
    string assetsPath = "../../assets/";
    string vertShaderFilename = assetsPath + "shaders/triangle.vert.spv";
    string fragShaderFilename = assetsPath + "shaders/triangle.frag.spv";
    string outputImageFilename = "output.jpg";

    // ========================================================================
    // VULKAN INITIALIZATION
    // ========================================================================

    /**
     * Initialize Vulkan context - this sets up:
     * - VkInstance: Connection to Vulkan library
     * - VkPhysicalDevice: Represents GPU hardware
     * - VkDevice: Logical device for GPU operations
     * - VkQueue: Command submission interface
     * - Memory allocators and other core components
     */
    Context ctx({}, false);
    auto device = ctx.device();

    // Define our render target resolution
    const uint32_t width = 1024;
    const uint32_t height = 768;

    // ========================================================================
    // STEP 1: Create Render Target Images
    // ========================================================================

    /**
     * In Vulkan, we need to explicitly create and manage all images.
     * Unlike OpenGL where the framebuffer is created automatically,
     * in Vulkan we must create our own render targets.
     *
     * We're creating a color image to render our triangle into.
     * No depth buffer is needed for this simple 2D triangle example.
     */

    // Create color attachment image (our main render target)
    Image2D colorImage(ctx);
    colorImage.createImage(
        VK_FORMAT_R8G8B8A8_UNORM,             // 8-bit per channel RGBA format (standard)
        width, height,                        // Image dimensions
        VK_SAMPLE_COUNT_1_BIT,                // No multisampling (1 sample per pixel)
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | // Can be used as color output target
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT,  // Can be copied from (for saving to file)
        VK_IMAGE_ASPECT_COLOR_BIT,            // This is a color image (not depth/stencil)
        1, 1, 0,                              // 1 mip level, 1 array layer, no create flags
        VK_IMAGE_VIEW_TYPE_2D                 // Standard 2D image view
    );

    // ========================================================================
    // STEP 2: Load Shaders and Create Graphics Pipeline
    // ========================================================================

    /**
     * Graphics pipelines in Vulkan define the entire rendering state:
     * - Which shaders to use
     * - How vertices are assembled into primitives
     * - How primitives are rasterized
     * - How fragments are processed and blended
     *
     * Unlike OpenGL's state machine, Vulkan pipelines are immutable objects
     * that contain all rendering state. This allows for better optimization
     * and more predictable performance.
     */

    // Load pre-compiled shader bytecode from disk
    vector<char> vertShaderCode = readSpvFile(vertShaderFilename);
    vector<char> fragShaderCode = readSpvFile(fragShaderFilename);

    // Create shader modules from the bytecode
    VkShaderModule vertShaderModule = createShaderModule(device, vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(device, fragShaderCode);

    // Configure shader stages for the pipeline
    // Each shader stage tells Vulkan which shader module to use for which stage
    VkPipelineShaderStageCreateInfo vertShaderStageCI{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vertShaderStageCI.stage = VK_SHADER_STAGE_VERTEX_BIT; // This is a vertex shader
    vertShaderStageCI.module = vertShaderModule;          // Use our vertex shader module
    vertShaderStageCI.pName = "main";                     // Entry point function name

    VkPipelineShaderStageCreateInfo fragShaderStageCI{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fragShaderStageCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT; // This is a fragment shader
    fragShaderStageCI.module = fragShaderModule;            // Use our fragment shader module
    fragShaderStageCI.pName = "main";                       // Entry point function name

    vector<VkPipelineShaderStageCreateInfo> shaderStages = {vertShaderStageCI, fragShaderStageCI};

    /**
     * VERTEX INPUT STATE
     *
     * Describes how vertex data is fed into the vertex shader.
     * Our triangle shader generates vertices procedurally using gl_VertexIndex,
     * so we don't need any vertex buffers or input attributes.
     *
     * This is different from typical rendering where you'd specify:
     * - Vertex buffer bindings (stride, input rate)
     * - Vertex attributes (position, color, texture coordinates)
     */
    VkPipelineVertexInputStateCreateInfo vertexInputCI{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputCI.vertexBindingDescriptionCount = 0; // No vertex buffer bindings
    vertexInputCI.pVertexBindingDescriptions = nullptr;
    vertexInputCI.vertexAttributeDescriptionCount = 0; // No vertex attributes
    vertexInputCI.pVertexAttributeDescriptions = nullptr;

    /**
     * INPUT ASSEMBLY STATE
     *
     * Defines how vertices are assembled into primitives (points, lines, triangles).
     * For our triangle, we use TRIANGLE_LIST which means every 3 vertices form a triangle.
     *
     * Other common topologies:
     * - POINT_LIST: Each vertex is a point
     * - LINE_LIST: Every 2 vertices form a line
     * - TRIANGLE_STRIP: Vertices form a strip of connected triangles
     */
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyCI.primitiveRestartEnable = VK_FALSE; // Don't enable primitive restart

    /**
     * VIEWPORT STATE
     *
     * Defines the transformation from normalized device coordinates to screen coordinates.
     * The viewport maps the [-1,1] coordinate space to actual pixel coordinates.
     *
     * Scissor rectangle can be used to limit rendering to a sub-region.
     */
    VkViewport viewport{};
    viewport.x = 0.0f;                            // Start at left edge
    viewport.y = 0.0f;                            // Start at top edge
    viewport.width = static_cast<float>(width);   // Full width
    viewport.height = static_cast<float>(height); // Full height
    viewport.minDepth = 0.0f;                     // Near clipping plane
    viewport.maxDepth = 1.0f;                     // Far clipping plane

    VkRect2D scissor{};
    scissor.offset = {0, 0};          // No offset
    scissor.extent = {width, height}; // Full image size

    VkPipelineViewportStateCreateInfo viewportStateCI{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportStateCI.viewportCount = 1;
    viewportStateCI.pViewports = &viewport;
    viewportStateCI.scissorCount = 1;
    viewportStateCI.pScissors = &scissor;

    /**
     * RASTERIZATION STATE
     *
     * Controls how primitives are converted into fragments (pixels).
     * This stage determines which pixels are covered by each triangle.
     *
     * Key settings:
     * - Polygon mode: FILL renders solid triangles, LINE renders wireframe
     * - Cull mode: Which faces to discard (front, back, or none)
     * - Front face: Which winding order is considered front-facing
     */
    VkPipelineRasterizationStateCreateInfo rasterizationCI{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizationCI.depthClampEnable = VK_FALSE;                 // Don't clamp depth values
    rasterizationCI.rasterizerDiscardEnable = VK_FALSE;          // Don't discard primitives
    rasterizationCI.polygonMode = VK_POLYGON_MODE_FILL;          // Fill triangles (not wireframe)
    rasterizationCI.lineWidth = 1.0f;                            // Line width for wireframe mode
    rasterizationCI.cullMode = VK_CULL_MODE_NONE;                // Don't cull any faces
    rasterizationCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // CCW = front face
    rasterizationCI.depthBiasEnable = VK_FALSE;                  // No depth bias

    /**
     * MULTISAMPLE STATE
     *
     * Controls anti-aliasing through multisampling.
     * We're using 1 sample per pixel (no anti-aliasing) for simplicity.
     *
     * Multisampling renders at higher resolution and then downsamples
     * to reduce jagged edges on triangle boundaries.
     */
    VkPipelineMultisampleStateCreateInfo multisampleCI{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleCI.sampleShadingEnable = VK_FALSE;               // No per-sample shading
    multisampleCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // 1 sample per pixel

    /**
     * DEPTH STENCIL STATE
     *
     * Controls depth testing and stencil testing.
     * Since we're rendering a simple 2D triangle without depth,
     * we disable both depth and stencil testing.
     *
     * In 3D applications, depth testing prevents far objects
     * from drawing over near objects.
     */
    VkPipelineDepthStencilStateCreateInfo depthStencilCI{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilCI.depthTestEnable = VK_FALSE;            // No depth testing
    depthStencilCI.depthWriteEnable = VK_FALSE;           // Don't write to depth buffer
    depthStencilCI.depthCompareOp = VK_COMPARE_OP_ALWAYS; // Always pass (not used)
    depthStencilCI.depthBoundsTestEnable = VK_FALSE;      // No depth bounds testing
    depthStencilCI.stencilTestEnable = VK_FALSE;          // No stencil testing

    /**
     * COLOR BLEND STATE
     *
     * Controls how new fragment colors are combined with existing colors
     * in the framebuffer. We're using simple replacement (no blending).
     *
     * Blending is commonly used for:
     * - Transparency effects (alpha blending)
     * - Additive effects (fire, explosions)
     * - UI overlays
     */
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    // Enable writing to all color channels (RGBA)
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE; // No blending, just replace

    VkPipelineColorBlendStateCreateInfo colorBlendCI{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendCI.logicOpEnable = VK_FALSE;   // No logical operations
    colorBlendCI.logicOp = VK_LOGIC_OP_COPY; // Simple copy operation
    colorBlendCI.attachmentCount = 1;        // One color attachment
    colorBlendCI.pAttachments = &colorBlendAttachment;

    /**
     * PIPELINE LAYOUT
     *
     * Defines the interface between shaders and resources (textures, buffers, etc.).
     * Our simple triangle doesn't use any external resources, so the layout is empty.
     *
     * In more complex applications, this would describe:
     * - Descriptor set layouts (for textures, uniform buffers)
     * - Push constant ranges (for small amounts of data)
     */
    VkPipelineLayoutCreateInfo pipelineLayoutCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutCI.setLayoutCount = 0; // No descriptor sets
    pipelineLayoutCI.pSetLayouts = nullptr;
    pipelineLayoutCI.pushConstantRangeCount = 0; // No push constants
    pipelineLayoutCI.pPushConstantRanges = nullptr;

    VkPipelineLayout pipelineLayout;
    check(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

    /**
     * VULKAN 1.3 DYNAMIC RENDERING SETUP
     *
     * Dynamic rendering (introduced in Vulkan 1.3) eliminates the need for
     * render passes and framebuffers. Instead, we specify render target formats
     * directly in the pipeline and begin rendering with just image views.
     *
     * Benefits:
     * - Simpler API (no render pass/framebuffer objects)
     * - More flexible (can change render targets easily)
     * - Better performance on some drivers
     */
    VkPipelineRenderingCreateInfo pipelineRenderingCI{
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    pipelineRenderingCI.colorAttachmentCount = 1;
    VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    pipelineRenderingCI.pColorAttachmentFormats = &colorFormat;
    pipelineRenderingCI.depthAttachmentFormat = VK_FORMAT_UNDEFINED;   // No depth buffer
    pipelineRenderingCI.stencilAttachmentFormat = VK_FORMAT_UNDEFINED; // No stencil buffer

    /**
     * GRAPHICS PIPELINE CREATION
     *
     * This combines all the above state into a single, immutable pipeline object.
     * Creating pipelines is expensive, so it's typically done during initialization.
     *
     * The pipeline describes the entire GPU rendering process from vertices to pixels.
     */
    VkGraphicsPipelineCreateInfo pipelineCI{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineCI.pNext = &pipelineRenderingCI; // Chain the dynamic rendering info
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = &vertexInputCI;
    pipelineCI.pInputAssemblyState = &inputAssemblyCI;
    pipelineCI.pViewportState = &viewportStateCI;
    pipelineCI.pRasterizationState = &rasterizationCI;
    pipelineCI.pMultisampleState = &multisampleCI;
    pipelineCI.pDepthStencilState = &depthStencilCI;
    pipelineCI.pColorBlendState = &colorBlendCI;
    pipelineCI.pDynamicState = nullptr; // No dynamic state
    pipelineCI.layout = pipelineLayout;
    pipelineCI.renderPass = VK_NULL_HANDLE; // No render pass for dynamic rendering
    pipelineCI.subpass = 0;
    pipelineCI.basePipelineHandle = VK_NULL_HANDLE; // Not deriving from another pipeline
    pipelineCI.basePipelineIndex = -1;

    VkPipeline graphicsPipeline;
    check(vkCreateGraphicsPipelines(device, ctx.pipelineCache(), 1, &pipelineCI, nullptr,
                                    &graphicsPipeline));

    // ========================================================================
    // STEP 3: Record and Execute Rendering Commands
    // ========================================================================

    /**
     * VULKAN COMMAND RECORDING
     *
     * In Vulkan, all GPU work is recorded into command buffers and then submitted
     * to queues for execution. This is different from OpenGL's immediate mode
     * where commands are executed as soon as they're called.
     *
     * Benefits of command buffers:
     * - Better CPU parallelization (record commands on multiple threads)
     * - More efficient GPU execution (commands can be optimized together)
     * - Explicit control over when GPU work happens
     */

    CommandBuffer renderCmd =
        ctx.createGraphicsCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    /**
     * IMAGE LAYOUT TRANSITIONS
     *
     * Vulkan images have different layouts optimized for different operations:
     * - UNDEFINED: Initial state, contents undefined
     * - COLOR_ATTACHMENT_OPTIMAL: Optimized for rendering
     * - TRANSFER_SRC_OPTIMAL: Optimized for copying from
     *
     * We must explicitly transition between layouts using memory barriers.
     * This tells the GPU when it's safe to change how the image is stored in memory.
     */
    {
        VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        // Pipeline stages: when this transition happens in the GPU pipeline
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT; // After: nothing
        barrier.srcAccessMask = VK_ACCESS_2_NONE;                   // Previous access: none
        barrier.dstStageMask =
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;            // Before: color output
        barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT; // Next access: color write
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;                  // Current layout
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;   // New layout
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;          // No queue transfer
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = colorImage.image();                                 // Target image
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}; // Entire image

        // Execute the layout transition
        VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(renderCmd.handle(), &dependencyInfo);
    }

    /**
     * BEGIN DYNAMIC RENDERING
     *
     * Configure and start the rendering pass. In dynamic rendering, we specify
     * render targets and their properties directly, without needing render pass objects.
     */
    VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachment.imageView = colorImage.view();                          // Target image view
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Expected layout
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;                   // Clear on start
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;                 // Keep results
    colorAttachment.clearValue.color = {1.0f, 1.0f, 1.0f, 1.0f};            // White background

    VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderingInfo.renderArea = {{0, 0}, {width, height}}; // Full image area
    renderingInfo.layerCount = 1;                         // Single layer
    renderingInfo.colorAttachmentCount = 1;               // One color target
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = nullptr;   // No depth attachment
    renderingInfo.pStencilAttachment = nullptr; // No stencil attachment

    // Start rendering - this clears the image with our white background
    vkCmdBeginRendering(renderCmd.handle(), &renderingInfo);

    /**
     * DRAW THE TRIANGLE
     *
     * Bind our graphics pipeline and issue a draw command.
     * The vertex shader will run 3 times (once per vertex) and generate
     * the triangle vertices procedurally using gl_VertexIndex.
     */
    vkCmdBindPipeline(renderCmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    vkCmdDraw(renderCmd.handle(), 3, 1, 0, 0); // Draw 3 vertices, 1 instance, no offsets

    // End the rendering pass
    vkCmdEndRendering(renderCmd.handle());

    /**
     * PREPARE IMAGE FOR COPYING
     *
     * Transition the color image from color attachment layout to transfer source layout.
     * This optimizes the image's memory layout for copying operations.
     */
    {
        VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barrier.srcStageMask =
            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;            // After: color output
        barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT; // Previous: color write
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;        // Before: transfer ops
        barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;          // Next: transfer read
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;   // Current layout
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;       // New layout
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = colorImage.image();
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(renderCmd.handle(), &dependencyInfo);
    }

    /**
     * SUBMIT COMMANDS TO GPU
     *
     * Execute all recorded commands on the GPU and wait for completion.
     * In a real application, you'd typically not wait immediately but
     * use fences or semaphores for more sophisticated synchronization.
     */
    renderCmd.submitAndWait();

    // ========================================================================
    // STEP 4: Copy Rendered Image to CPU and Save as JPEG
    // ========================================================================

    /**
     * GPU-TO-CPU MEMORY TRANSFER
     *
     * GPU memory is optimized for GPU access but CPU can't directly read it.
     * We need to copy the image data to CPU-accessible "staging" memory.
     *
     * This process involves:
     * 1. Create a buffer in host-visible memory
     * 2. Copy image data from GPU image to CPU buffer
     * 3. Map the buffer to get a CPU pointer
     * 4. Read the data and save to file
     */

    // Calculate image size in bytes (RGBA8 = 4 bytes per pixel)
    VkDeviceSize imageSize = width * height * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    // Create a buffer that can be written to by transfer operations
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT; // Can be transfer destination
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // Used by one queue family

    check(vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer));

    // Allocate memory for the buffer
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReqs.size;
    // Find memory type that is host-visible (CPU accessible) and coherent (no manual sync)
    allocInfo.memoryTypeIndex =
        ctx.getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    check(vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory));
    check(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0));

    /**
     * COPY IMAGE TO BUFFER
     *
     * Use a transfer command to copy the image data to our staging buffer.
     * This happens asynchronously on the GPU.
     */
    CommandBuffer copyCmd = ctx.createTransferCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;                                        // Start at buffer beginning
    copyRegion.bufferRowLength = 0;                                     // Tightly packed rows
    copyRegion.bufferImageHeight = 0;                                   // Tightly packed image
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Copy color data
    copyRegion.imageSubresource.mipLevel = 0;                           // Base mip level
    copyRegion.imageSubresource.baseArrayLayer = 0;                     // First array layer
    copyRegion.imageSubresource.layerCount = 1;                         // Single layer
    copyRegion.imageOffset = {0, 0, 0};                                 // Start at image origin
    copyRegion.imageExtent = {width, height, 1};                        // Full image size

    vkCmdCopyImageToBuffer(copyCmd.handle(), colorImage.image(),
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &copyRegion);

    copyCmd.submitAndWait();

    /**
     * READ DATA AND SAVE TO FILE
     *
     * Map the staging buffer memory to get a CPU pointer, then save the
     * image data as a JPEG file using the stb_image_write library.
     */
    void* mappedData;
    check(vkMapMemory(device, stagingMemory, 0, imageSize, 0, &mappedData));

    unsigned char* pixelData = static_cast<unsigned char*>(mappedData);

    // Save as JPEG with 90% quality
    if (!stbi_write_jpg(outputImageFilename.c_str(), width, height, 4, pixelData, 90)) {
        exitWithMessage("Failed to save output image: {}", outputImageFilename);
    }

    vkUnmapMemory(device, stagingMemory);

    printLog("Successfully saved rendered triangle to: {}", outputImageFilename);

    // ========================================================================
    // STEP 5: Cleanup Resources
    // ========================================================================

    /**
     * VULKAN RESOURCE MANAGEMENT
     *
     * Unlike garbage-collected languages, Vulkan requires explicit cleanup
     * of all resources. We must destroy objects in reverse order of creation
     * to avoid dependency issues.
     *
     * In a real application, you'd typically use RAII wrappers or smart
     * pointers to automate this cleanup.
     */
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);

    // Note: colorImage is automatically cleaned up by its destructor
    // Note: Context destructor handles cleanup of device, instance, etc.

    return 0;
}
