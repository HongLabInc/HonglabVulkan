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
    // VULKAN 1.1 INITIALIZATION
    // ========================================================================

    /**
     * Initialize Vulkan context - this sets up:
     * - VkInstance: Connection to Vulkan library (using Vulkan 1.1 API)
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
    // STEP 2: Create Render Pass (VULKAN 1.1 TRADITIONAL APPROACH)
    // ========================================================================

    /**
     * VULKAN 1.1 RENDER PASS CREATION
     *
     * Unlike Vulkan 1.3's dynamic rendering, Vulkan 1.1 requires explicit
     * render pass objects that describe:
     * - What attachments (color, depth) will be used
     * - How they should be loaded/stored
     * - How multisampling is handled
     * - Dependencies between subpasses
     *
     * This is more verbose but gives fine-grained control over rendering operations.
     */

    // Color attachment description
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;      // Must match our image format
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;        // No multisampling
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // Clear attachment at start
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store results after rendering
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;   // No stencil
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // No stencil
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Don't care about initial state
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Keep in color layout

    // Attachment reference for subpass
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0; // Index 0 in attachments array
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Layout during rendering

    // Subpass description
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // Graphics rendering
    subpass.colorAttachmentCount = 1;                            // One color output
    subpass.pColorAttachments = &colorAttachmentRef;             // Reference to color attachment

    // Subpass dependency for proper synchronization
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // External commands (before render pass)
    dependency.dstSubpass = 0;                   // Our subpass (index 0)
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // Wait for color output
    dependency.srcAccessMask = 0;                      // No prior access
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // Before color output
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;         // Will write to color

    // Create the render pass
    VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass renderPass;
    check(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));

    // ========================================================================
    // STEP 3: Create Framebuffer (VULKAN 1.1 REQUIREMENT)
    // ========================================================================

    /**
     * VULKAN 1.1 FRAMEBUFFER CREATION
     *
     * Framebuffers bind specific image views to the attachment points
     * defined in the render pass. This creates the actual render target
     * that the GPU will draw into.
     *
     * In Vulkan 1.3's dynamic rendering, this step is eliminated.
     */

    VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferInfo.renderPass = renderPass; // Must be compatible
    framebufferInfo.attachmentCount = 1;     // One attachment (color)
    VkImageView colorImageView = colorImage.view();
    framebufferInfo.pAttachments = &colorImageView; // Actual image view
    framebufferInfo.width = width;                  // Framebuffer dimensions
    framebufferInfo.height = height;
    framebufferInfo.layers = 1; // Single layer

    VkFramebuffer framebuffer;
    check(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer));

    // ========================================================================
    // STEP 4: Load Shaders and Create Graphics Pipeline
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
     * VULKAN 1.1 GRAPHICS PIPELINE CREATION
     *
     * In Vulkan 1.1, pipelines must be associated with a render pass.
     * This is different from Vulkan 1.3's dynamic rendering where
     * we could specify formats directly in VkPipelineRenderingCreateInfo.
     *
     * The pipeline describes the entire GPU rendering process from vertices to pixels.
     */
    VkGraphicsPipelineCreateInfo pipelineCI{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    // NOTE: No pNext chain in Vulkan 1.1 - no VkPipelineRenderingCreateInfo
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
    pipelineCI.renderPass = renderPass;             // REQUIRED in Vulkan 1.1
    pipelineCI.subpass = 0;                         // Subpass index within render pass
    pipelineCI.basePipelineHandle = VK_NULL_HANDLE; // Not deriving from another pipeline
    pipelineCI.basePipelineIndex = -1;

    VkPipeline graphicsPipeline;
    check(vkCreateGraphicsPipelines(device, ctx.pipelineCache(), 1, &pipelineCI, nullptr,
                                    &graphicsPipeline));

    // ========================================================================
    // STEP 5: Record and Execute Rendering Commands (VULKAN 1.1 STYLE)
    // ========================================================================

    /**
     * VULKAN 1.1 COMMAND RECORDING
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
     * VULKAN 1.1 RENDER PASS EXECUTION
     *
     * In Vulkan 1.1, we must use render passes and framebuffers.
     * This is more verbose than Vulkan 1.3's dynamic rendering but
     * provides the same functionality with explicit render pass objects.
     *
     * IMPORTANT: The render pass handles layout transitions automatically based
     * on the initialLayout and finalLayout specified in the attachment description.
     */

    // Clear values for attachments
    VkClearValue clearColor = {{{1.0f, 1.0f, 1.0f, 1.0f}}}; // White background

    VkRenderPassBeginInfo renderPassBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassBegin.renderPass = renderPass;             // Our render pass
    renderPassBegin.framebuffer = framebuffer;           // Our framebuffer
    renderPassBegin.renderArea.offset = {0, 0};          // Render area offset
    renderPassBegin.renderArea.extent = {width, height}; // Render area size
    renderPassBegin.clearValueCount = 1;                 // Number of clear values
    renderPassBegin.pClearValues = &clearColor;          // Clear value array

    // Begin the render pass
    // NOTE: The render pass automatically transitions the image from UNDEFINED
    // to COLOR_ATTACHMENT_OPTIMAL as specified in the render pass
    vkCmdBeginRenderPass(renderCmd.handle(), &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

    /**
     * DRAW THE TRIANGLE
     *
     * Bind our graphics pipeline and issue a draw command.
     * The vertex shader will run 3 times (once per vertex) and generate
     * the triangle vertices procedurally using gl_VertexIndex.
     */
    vkCmdBindPipeline(renderCmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    vkCmdDraw(renderCmd.handle(), 3, 1, 0, 0); // Draw 3 vertices, 1 instance, no offsets

    // End the render pass
    // NOTE: The render pass automatically transitions the image to the finalLayout
    // (COLOR_ATTACHMENT_OPTIMAL in our case) as specified in the render pass
    vkCmdEndRenderPass(renderCmd.handle());

    /**
     * PREPARE IMAGE FOR COPYING (VULKAN 1.1 STYLE)
     *
     * Since our render pass ends with the image in COLOR_ATTACHMENT_OPTIMAL layout,
     * we need to transition it to TRANSFER_SRC_OPTIMAL for copying.
     */
    {
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // Previous: color write
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;          // Next: transfer read
        barrier.oldLayout =
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Current layout after render pass
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // New layout for copying
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = colorImage.image();
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        // Execute the layout transition using Vulkan 1.1 API
        vkCmdPipelineBarrier(renderCmd.handle(),
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // Source stage mask
                             VK_PIPELINE_STAGE_TRANSFER_BIT, // Destination stage mask
                             0,                              // Dependency flags
                             0, nullptr,                     // Memory barriers
                             0, nullptr,                     // Buffer memory barriers
                             1, &barrier                     // Image memory barriers
        );
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
    // STEP 6: Copy Rendered Image to CPU and Save as JPEG
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
    // STEP 7: Cleanup Resources (VULKAN 1.1)
    // ========================================================================

    /**
     * VULKAN 1.1 RESOURCE MANAGEMENT
     *
     * Unlike garbage-collected languages, Vulkan requires explicit cleanup
     * of all resources. We must destroy objects in reverse order of creation
     * to avoid dependency issues.
     *
     * Note the additional cleanup for render pass and framebuffer objects
     * that don't exist in Vulkan 1.3's dynamic rendering.
     */
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyFramebuffer(device, framebuffer, nullptr); // Vulkan 1.1 specific
    vkDestroyRenderPass(device, renderPass, nullptr);   // Vulkan 1.1 specific
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);

    // Note: colorImage is automatically cleaned up by its destructor
    // Note: Context destructor handles cleanup of device, instance, etc.

    return 0;
}
