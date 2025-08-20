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
 * @param spvFilename Path to the .spv file (SPIR-V binary)
 * @return Vector containing the raw shader bytecode
 */
vector<char> readSpvFile(const string& spvFilename)
{
    if (spvFilename.length() < 4 || spvFilename.substr(spvFilename.length() - 4) != ".spv") {
        exitWithMessage("Shader file does not have .spv extension: {}", spvFilename);
    }

    ifstream is(spvFilename, ios::binary | ios::in | ios::ate);
    if (!is.is_open()) {
        exitWithMessage("Could not open shader file: {}", spvFilename);
    }

    size_t shaderSize = static_cast<size_t>(is.tellg());
    if (shaderSize == 0 || shaderSize % 4 != 0) {
        exitWithMessage("Shader file size is invalid (must be >0 and multiple of 4): {}",
                        spvFilename);
    }

    is.seekg(0, ios::beg);
    vector<char> shaderCode(shaderSize);
    is.read(shaderCode.data(), shaderSize);
    is.close();

    return shaderCode;
}

/**
 * @brief Creates a Vulkan shader module from SPIR-V bytecode
 * @param device Vulkan logical device
 * @param shaderCode Raw SPIR-V bytecode as char vector
 * @return VkShaderModule handle that can be used in pipeline creation
 */
VkShaderModule createShaderModule(VkDevice device, const vector<char>& shaderCode)
{
    VkShaderModule shaderModule;

    VkShaderModuleCreateInfo shaderModuleCI{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shaderModuleCI.codeSize = shaderCode.size();
    shaderModuleCI.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

    check(vkCreateShaderModule(device, &shaderModuleCI, nullptr, &shaderModule));
    return shaderModule;
}

int main()
{
    string assetsPath = "../../assets/";
    string inputImageFilename = assetsPath + "image.jpg";
    string vertShaderFilename = assetsPath + "shaders/triangle.vert.spv";
    string fragShaderFilename = assetsPath + "shaders/triangle.frag.spv";
    string outputImageFilename = "output.jpg";

    // Initialize Vulkan context
    Context ctx({}, false);
    auto device = ctx.device();

    // Define output resolution
    const uint32_t width = 1024;
    const uint32_t height = 768;

    // ========================================================================
    // STEP 1: Create Color Image for Rendering (NO DEPTH IMAGE)
    // ========================================================================

    // Create color attachment image (render target)
    Image2D colorImage(ctx);
    colorImage.createImage(VK_FORMAT_R8G8B8A8_UNORM,             // 8-bit RGBA format
                           width, height,                        // Output resolution
                           VK_SAMPLE_COUNT_1_BIT,                // No multisampling
                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | // Used as color attachment
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT,  // Can be copied from
                           VK_IMAGE_ASPECT_COLOR_BIT,            // Color image
                           1, 1, 0,                              // 1 mip, 1 layer, no flags
                           VK_IMAGE_VIEW_TYPE_2D                 // 2D image view
    );

    // ========================================================================
    // STEP 2: Load Shaders and Create Pipeline for Dynamic Rendering
    // ========================================================================

    // Load vertex and fragment shaders
    vector<char> vertShaderCode = readSpvFile(vertShaderFilename);
    vector<char> fragShaderCode = readSpvFile(fragShaderFilename);

    VkShaderModule vertShaderModule = createShaderModule(device, vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(device, fragShaderCode);

    // Shader stage create infos
    VkPipelineShaderStageCreateInfo vertShaderStageCI{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vertShaderStageCI.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageCI.module = vertShaderModule;
    vertShaderStageCI.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageCI{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fragShaderStageCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageCI.module = fragShaderModule;
    fragShaderStageCI.pName = "main";

    vector<VkPipelineShaderStageCreateInfo> shaderStages = {vertShaderStageCI, fragShaderStageCI};

    // Vertex input state (no vertex buffer needed)
    VkPipelineVertexInputStateCreateInfo vertexInputCI{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputCI.vertexBindingDescriptionCount = 0; // No vertex bindings
    vertexInputCI.pVertexBindingDescriptions = nullptr;
    vertexInputCI.vertexAttributeDescriptionCount = 0; // No vertex attributes
    vertexInputCI.pVertexAttributeDescriptions = nullptr;

    // Input assembly state
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCI{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyCI.primitiveRestartEnable = VK_FALSE;

    // Viewport state
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {width, height};

    VkPipelineViewportStateCreateInfo viewportStateCI{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportStateCI.viewportCount = 1;
    viewportStateCI.pViewports = &viewport;
    viewportStateCI.scissorCount = 1;
    viewportStateCI.pScissors = &scissor;

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizationCI{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizationCI.depthClampEnable = VK_FALSE;
    rasterizationCI.rasterizerDiscardEnable = VK_FALSE;
    rasterizationCI.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationCI.lineWidth = 1.0f;
    rasterizationCI.cullMode = VK_CULL_MODE_NONE; // Disable culling for better visibility
    rasterizationCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationCI.depthBiasEnable = VK_FALSE;

    // Multisample state
    VkPipelineMultisampleStateCreateInfo multisampleCI{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleCI.sampleShadingEnable = VK_FALSE;
    multisampleCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth stencil state - DISABLED
    VkPipelineDepthStencilStateCreateInfo depthStencilCI{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencilCI.depthTestEnable = VK_FALSE;            // DISABLED depth testing
    depthStencilCI.depthWriteEnable = VK_FALSE;           // DISABLED depth writing
    depthStencilCI.depthCompareOp = VK_COMPARE_OP_ALWAYS; // Always pass (not used)
    depthStencilCI.depthBoundsTestEnable = VK_FALSE;
    depthStencilCI.stencilTestEnable = VK_FALSE;

    // Color blend state
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlendCI{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendCI.logicOpEnable = VK_FALSE;
    colorBlendCI.logicOp = VK_LOGIC_OP_COPY;
    colorBlendCI.attachmentCount = 1;
    colorBlendCI.pAttachments = &colorBlendAttachment;

    // Pipeline layout (no descriptors needed for this simple triangle)
    VkPipelineLayoutCreateInfo pipelineLayoutCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutCI.setLayoutCount = 0;
    pipelineLayoutCI.pSetLayouts = nullptr;
    pipelineLayoutCI.pushConstantRangeCount = 0;
    pipelineLayoutCI.pPushConstantRanges = nullptr;

    VkPipelineLayout pipelineLayout;
    check(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

    // VULKAN 1.3 Dynamic Rendering: Configure pipeline rendering formats (NO DEPTH)
    VkPipelineRenderingCreateInfo pipelineRenderingCI{
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    pipelineRenderingCI.colorAttachmentCount = 1;
    VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
    pipelineRenderingCI.pColorAttachmentFormats = &colorFormat;
    pipelineRenderingCI.depthAttachmentFormat = VK_FORMAT_UNDEFINED;   // NO DEPTH
    pipelineRenderingCI.stencilAttachmentFormat = VK_FORMAT_UNDEFINED; // NO STENCIL

    // Create graphics pipeline with dynamic rendering support
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
    pipelineCI.pDynamicState = nullptr;
    pipelineCI.layout = pipelineLayout;
    pipelineCI.renderPass = VK_NULL_HANDLE; // No render pass for dynamic rendering
    pipelineCI.subpass = 0;
    pipelineCI.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCI.basePipelineIndex = -1;

    VkPipeline graphicsPipeline;
    check(vkCreateGraphicsPipelines(device, ctx.pipelineCache(), 1, &pipelineCI, nullptr,
                                    &graphicsPipeline));

    // ========================================================================
    // STEP 3: Record and Execute Rendering Commands using Dynamic Rendering
    // ========================================================================

    CommandBuffer renderCmd =
        ctx.createGraphicsCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Transition color image to proper layout for rendering (NO DEPTH IMAGE)
    {
        VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_NONE;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = colorImage.image();
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(renderCmd.handle(), &dependencyInfo);
    }

    // Begin dynamic rendering (NO DEPTH ATTACHMENT)
    VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachment.imageView = colorImage.view();
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {1.0f, 1.0f, 1.0f, 1.0f}; // Dark gray background

    VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderingInfo.renderArea = {{0, 0}, {width, height}};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = nullptr;   // NO DEPTH ATTACHMENT
    renderingInfo.pStencilAttachment = nullptr; // NO STENCIL ATTACHMENT

    vkCmdBeginRendering(renderCmd.handle(), &renderingInfo);

    // Bind pipeline and draw triangle
    vkCmdBindPipeline(renderCmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    vkCmdDraw(renderCmd.handle(), 3, 1, 0, 0); // Draw 3 vertices (triangle)

    vkCmdEndRendering(renderCmd.handle());

    // Transition color image for transfer
    {
        VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = colorImage.image();
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        dependencyInfo.imageMemoryBarrierCount = 1;
        dependencyInfo.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(renderCmd.handle(), &dependencyInfo);
    }

    // Submit command buffer and wait
    renderCmd.submitAndWait();

    // ========================================================================
    // STEP 4: Copy Image Data and Save as JPEG
    // ========================================================================

    // Create staging buffer for image copy
    VkDeviceSize imageSize = width * height * 4; // RGBA8 = 4 bytes per pixel

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    check(vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer));

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex =
        ctx.getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    check(vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory));
    check(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0));

    // Copy image to buffer
    CommandBuffer copyCmd = ctx.createTransferCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageOffset = {0, 0, 0};
    copyRegion.imageExtent = {width, height, 1};

    vkCmdCopyImageToBuffer(copyCmd.handle(), colorImage.image(),
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &copyRegion);

    copyCmd.submitAndWait();

    // Map memory and save image
    void* mappedData;
    check(vkMapMemory(device, stagingMemory, 0, imageSize, 0, &mappedData));

    unsigned char* pixelData = static_cast<unsigned char*>(mappedData);

    if (!stbi_write_jpg(outputImageFilename.c_str(), width, height, 4, pixelData, 90)) {
        exitWithMessage("Failed to save output image: {}", outputImageFilename);
    }

    vkUnmapMemory(device, stagingMemory);

    printLog("Successfully saved rendered triangle to: {}", outputImageFilename);

    // ========================================================================
    // STEP 5: Cleanup Resources
    // ========================================================================

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);

    return 0;
}
