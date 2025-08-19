#include "engine/Context.h"
#include "engine/Image2D.h"
#include "engine/CommandBuffer.h"
#include <stb_image.h>
#include <stb_image_write.h>
#include <glm/glm.hpp>
#include <fstream>

using namespace hlab;

/**
 * @brief Reads a compiled SPIR-V shader binary file from disk
 * @param spvFilename Path to the .spv file (SPIR-V binary)
 * @return Vector containing the raw shader bytecode
 *
 * SPIR-V files must be multiples of 4 bytes since they contain 32-bit words.
 * The file is read in binary mode with the cursor positioned at the end (ios::ate)
 * to determine file size efficiently.
 */
vector<char> readSpvFile(const string& spvFilename)
{
    // Validate file extension - SPIR-V files must have .spv extension
    if (spvFilename.length() < 4 || spvFilename.substr(spvFilename.length() - 4) != ".spv") {
        exitWithMessage("Shader file does not have .spv extension: {}", spvFilename);
    }

    // Open file in binary mode, positioned at end for size calculation
    ifstream is(spvFilename, ios::binary | ios::in | ios::ate);
    if (!is.is_open()) {
        exitWithMessage("Could not open shader file: {}", spvFilename);
    }

    // Get file size and validate it's a valid SPIR-V file
    size_t shaderSize = static_cast<size_t>(is.tellg());
    if (shaderSize == 0 || shaderSize % 4 != 0) {
        exitWithMessage("Shader file size is invalid (must be >0 and multiple of 4): {}",
                        spvFilename);
    }

    // Reset to beginning and read entire file
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
 *
 * A shader module is a wrapper around SPIR-V code that Vulkan can understand.
 * The bytecode is cast to uint32_t* since SPIR-V is composed of 32-bit words.
 */
VkShaderModule createShaderModule(VkDevice device, const vector<char>& shaderCode)
{
    VkShaderModule shaderModule;

    // Structure describing how to create the shader module
    VkShaderModuleCreateInfo shaderModuleCI{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shaderModuleCI.codeSize = shaderCode.size();
    // SPIR-V bytecode must be interpreted as 32-bit words, not 8-bit chars
    shaderModuleCI.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

    check(vkCreateShaderModule(device, &shaderModuleCI, nullptr, &shaderModule));
    return shaderModule;
}

int main()
{
    // Initialize Vulkan context - this sets up instance, device, queues, etc.
    Context ctx({}, false);
    auto device = ctx.device();

    // File paths for our compute shader example
    string assetsPath = "../../assets/";
    string inputImageFilename = assetsPath + "image.jpg";
    string computeShaderFilename = assetsPath + "shaders/test.comp.spv";
    string outputImageFilename = "output.jpg";

    // ========================================================================
    // STEP 1: Create Input Image
    // ========================================================================

    // Create input image from a JPEG/PNG file
    // The image will be used as readonly storage in our compute shader
    Image2D inputImage(ctx);
    inputImage.updateUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT); // Enable storage image access
    inputImage.createTextureFromImage(inputImageFilename, false, false);

    uint32_t width = inputImage.width();
    uint32_t height = inputImage.height();

    // Create output image with floating-point format
    // This will store the processed result from our compute shader
    Image2D outputImage(ctx);
    outputImage.createImage(VK_FORMAT_R32G32B32A32_SFLOAT, // 32-bit float per channel (RGBA)
                            width, height,                 // Same dimensions as input
                            VK_SAMPLE_COUNT_1_BIT,         // No multisampling
                            VK_IMAGE_USAGE_STORAGE_BIT |   // Can be written to by compute shader
                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // Can be copied from for saving
                            VK_IMAGE_ASPECT_COLOR_BIT,           // Color image (not depth/stencil)
                            1, 1, 0,              // 1 mip level, 1 array layer, no flags
                            VK_IMAGE_VIEW_TYPE_2D // Standard 2D image view
    );

    // ========================================================================
    // STEP 2: Load and Create Shader Module
    // ========================================================================

    // Load compiled SPIR-V shader from disk
    vector<char> shaderCode = readSpvFile(computeShaderFilename);
    VkShaderModule computeShaderModule = createShaderModule(device, shaderCode);

    // ========================================================================
    // STEP 3: Create Descriptor Set Layout
    // ========================================================================

    // Descriptor sets define how shaders access resources (images, buffers, samplers)
    // Our compute shader has two storage images: input (binding 0) and output (binding 1)
    vector<VkDescriptorSetLayoutBinding> bindings(2);

    // Binding 0: Input image (readonly storage image)
    // Corresponds to: layout(set = 0, binding = 0, rgba8) uniform readonly image2D inputImage;
    bindings[0].binding = 0;                                       // Matches shader binding = 0
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; // Storage image type
    bindings[0].descriptorCount = 1;                               // Single image
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;          // Used in compute stage
    bindings[0].pImmutableSamplers = nullptr;                      // No samplers needed

    // Binding 1: Output image (writeonly storage image)
    // Corresponds to: layout(set = 0, binding = 1, rgba32f) uniform writeonly image2D outputImage;
    bindings[1].binding = 1;                                       // Matches shader binding = 1
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; // Storage image type
    bindings[1].descriptorCount = 1;                               // Single image
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;          // Used in compute stage
    bindings[1].pImmutableSamplers = nullptr;                      // No samplers needed

    // Create the descriptor set layout from our bindings
    VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    descriptorLayoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
    descriptorLayoutCI.pBindings = bindings.data();

    VkDescriptorSetLayout descriptorSetLayout;
    check(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &descriptorSetLayout));

    // ========================================================================
    // STEP 4: Create Pipeline Layout
    // ========================================================================

    // Pipeline layout defines the overall resource layout for the entire pipeline
    // It can include multiple descriptor set layouts and push constant ranges
    VkPipelineLayoutCreateInfo pipelineLayoutCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutCI.setLayoutCount = 1;                 // We have one descriptor set (set = 0)
    pipelineLayoutCI.pSetLayouts = &descriptorSetLayout; // Our image bindings
    pipelineLayoutCI.pushConstantRangeCount = 0;         // No push constants in this example
    pipelineLayoutCI.pPushConstantRanges = nullptr;

    VkPipelineLayout pipelineLayout;
    check(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

    // ========================================================================
    // STEP 5: Create Compute Pipeline
    // ========================================================================

    // Define the shader stage for our compute pipeline
    VkPipelineShaderStageCreateInfo shaderStageCI{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    shaderStageCI.stage = VK_SHADER_STAGE_COMPUTE_BIT; // This is a compute shader
    shaderStageCI.module = computeShaderModule;        // Our loaded shader module
    shaderStageCI.pName = "main";                      // Entry point function name
    shaderStageCI.pSpecializationInfo = nullptr;       // No shader specialization

    // Create the compute pipeline
    // Unlike graphics pipelines, compute pipelines are much simpler - just one shader stage
    VkComputePipelineCreateInfo pipelineCI{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineCI.layout = pipelineLayout;             // Resource layout
    pipelineCI.stage = shaderStageCI;               // Single compute shader stage
    pipelineCI.basePipelineHandle = VK_NULL_HANDLE; // No pipeline derivation
    pipelineCI.basePipelineIndex = -1;

    VkPipeline computePipelineHandle;
    check(vkCreateComputePipelines(device, ctx.pipelineCache(), 1, &pipelineCI, nullptr,
                                   &computePipelineHandle));

    // ========================================================================
    // STEP 6: Create Descriptor Pool and Allocate Descriptor Set
    // ========================================================================

    // Descriptor pools manage memory for descriptor sets
    // We need storage for 2 storage images (input + output)
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; // Storage image descriptors
    poolSize.descriptorCount = 2;                     // 2 storage images total

    VkDescriptorPoolCreateInfo poolCI{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolCI.poolSizeCount = 1;      // One type of descriptor
    poolCI.pPoolSizes = &poolSize; // Our storage image pool
    poolCI.maxSets = 1;            // We'll allocate 1 descriptor set

    VkDescriptorPool descriptorPool;
    check(vkCreateDescriptorPool(device, &poolCI, nullptr, &descriptorPool));

    // Allocate a descriptor set from the pool
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = descriptorPool;    // Pool to allocate from
    allocInfo.descriptorSetCount = 1;             // Number of sets to allocate
    allocInfo.pSetLayouts = &descriptorSetLayout; // Layout for the set

    VkDescriptorSet descriptorSet;
    check(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

    // ========================================================================
    // STEP 7: Update Descriptor Set with Actual Images
    // ========================================================================

    // Descriptor sets need to be updated with actual resource handles (images, buffers, etc.)
    // Each VkDescriptorImageInfo describes one image resource

    // Input image info - will be read by the compute shader
    VkDescriptorImageInfo inputImageInfo{};
    inputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // General layout for storage images
    inputImageInfo.imageView = inputImage.view();         // The actual input image view
    inputImageInfo.sampler = VK_NULL_HANDLE;              // No sampler needed for storage images

    // Output image info - will be written by the compute shader
    VkDescriptorImageInfo outputImageInfo{};
    outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // General layout for storage images
    outputImageInfo.imageView = outputImage.view();        // The actual output image view
    outputImageInfo.sampler = VK_NULL_HANDLE;              // No sampler needed for storage images

    // Prepare descriptor writes - these tell Vulkan which resources to bind to which bindings
    vector<VkWriteDescriptorSet> descriptorWrites;

    // Write for binding 0 (input image)
    VkWriteDescriptorSet inputImageWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    inputImageWrite.dstSet = descriptorSet; // Target descriptor set
    inputImageWrite.dstBinding = 0;         // Binding index in shader
    inputImageWrite.dstArrayElement = 0;    // Array index (0 for non-arrays)
    inputImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; // Must match layout
    inputImageWrite.descriptorCount = 1;                               // Number of descriptors
    inputImageWrite.pImageInfo = &inputImageInfo;                      // Actual image info
    descriptorWrites.push_back(inputImageWrite);

    // Write for binding 1 (output image)
    VkWriteDescriptorSet outputImageWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    outputImageWrite.dstSet = descriptorSet; // Target descriptor set
    outputImageWrite.dstBinding = 1;         // Binding index in shader
    outputImageWrite.dstArrayElement = 0;    // Array index (0 for non-arrays)
    outputImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; // Must match layout
    outputImageWrite.descriptorCount = 1;                               // Number of descriptors
    outputImageWrite.pImageInfo = &outputImageInfo;                     // Actual image info
    descriptorWrites.push_back(outputImageWrite);

    // Apply all descriptor writes at once
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);

    // ========================================================================
    // STEP 8: Record and Execute Compute Commands
    // ========================================================================

    // Create a command buffer for recording our compute operations
    CommandBuffer computeCmd =
        ctx.createComputeCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Before using images in compute shaders, we need to transition their layouts
    // This ensures proper synchronization and optimal access patterns

    // Transition input image from shader-read-only to general layout
    // (Images created from files start in shader-read-only layout)
    VkImageMemoryBarrier2 inputBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    inputBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT; // Source: start of pipeline
    inputBarrier.srcAccessMask = 0;                                  // No prior access
    inputBarrier.dstStageMask =
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;                        // Destination: compute stage
    inputBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;          // Will be read in shader
    inputBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Current layout
    inputBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;           // Required for storage images
    inputBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // No queue transfer
    inputBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // No queue transfer
    inputBarrier.image = inputImage.image();                    // Target image
    inputBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}; // All mips/layers

    // Transition output image from undefined to general layout
    // (New images start in undefined layout)
    VkImageMemoryBarrier2 outputBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    outputBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT; // Source: start of pipeline
    outputBarrier.srcAccessMask = 0;                                  // No prior access
    outputBarrier.dstStageMask =
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;                  // Destination: compute stage
    outputBarrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;  // Will be written in shader
    outputBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;         // Don't care about contents
    outputBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;           // Required for storage images
    outputBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // No queue transfer
    outputBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // No queue transfer
    outputBarrier.image = outputImage.image();                   // Target image
    outputBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}; // All mips/layers

    // Execute both layout transitions
    vector<VkImageMemoryBarrier2> imageBarriers = {inputBarrier, outputBarrier};
    VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size());
    dependencyInfo.pImageMemoryBarriers = imageBarriers.data();
    vkCmdPipelineBarrier2(computeCmd.handle(), &dependencyInfo);

    // Bind the compute pipeline - this tells GPU which shader to execute
    vkCmdBindPipeline(computeCmd.handle(), VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineHandle);

    // Bind descriptor sets - this connects our images to the shader bindings
    vkCmdBindDescriptorSets(computeCmd.handle(),            // Command buffer
                            VK_PIPELINE_BIND_POINT_COMPUTE, // Pipeline type
                            pipelineLayout,                 // Pipeline layout
                            0,                              // First set (set = 0 in shader)
                            1,                              // Number of sets
                            &descriptorSet,                 // Array of descriptor sets
                            0, nullptr                      // No dynamic offsets
    );

    // Dispatch the compute shader
    // Our shader uses local workgroup size of 16x16 (defined in shader)
    // We need to calculate how many workgroups to cover the entire image
    uint32_t groupCountX = (width + 15) / 32;  // Round up division for X
    uint32_t groupCountY = (height + 15) / 32; // Round up division for Y
    vkCmdDispatch(computeCmd.handle(), groupCountX, groupCountY, 1);

    // After compute shader writes, transition output image for transfer operations
    // We need to copy the image to CPU memory for saving as JPEG
    VkImageMemoryBarrier2 transferBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    transferBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT; // Source: compute stage
    transferBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;          // Previous: shader write
    transferBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;  // Destination: transfer ops
    transferBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;    // Next: transfer read
    transferBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;              // Current layout
    transferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // Optimal for reading
    transferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;    // No queue transfer
    transferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;    // No queue transfer
    transferBarrier.image = outputImage.image();                      // Target image
    transferBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}; // All mips/layers

    VkDependencyInfo transferDependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    transferDependency.imageMemoryBarrierCount = 1;
    transferDependency.pImageMemoryBarriers = &transferBarrier;
    vkCmdPipelineBarrier2(computeCmd.handle(), &transferDependency);

    // Submit command buffer and wait for completion
    computeCmd.submitAndWait();

    // ========================================================================
    // STEP 9: Copy Image Data Back to CPU and Save as JPEG
    // ========================================================================

    // Create a staging buffer to copy GPU image data to CPU-accessible memory
    VkDeviceSize imageSize = width * height * 4 * sizeof(float); // RGBA32F = 4 floats per pixel

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    // Create buffer for staging - must be accessible by both GPU (write) and CPU (read)
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = imageSize;                         // Size in bytes
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT; // Can be written to by transfers
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // Used by one queue family

    check(vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer));

    // Allocate memory for the staging buffer
    // This memory must be host-visible so CPU can read it
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo2{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo2.allocationSize = memReqs.size;
    allocInfo2.memoryTypeIndex =
        ctx.getMemoryTypeIndex(memReqs.memoryTypeBits,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |    // CPU can access
                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT // No manual sync needed
        );

    check(vkAllocateMemory(device, &allocInfo2, nullptr, &stagingMemory));
    check(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0));

    // Copy image data from GPU to staging buffer
    CommandBuffer copyCmd = ctx.createTransferCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;                                        // Start at buffer beginning
    copyRegion.bufferRowLength = 0;                                     // Tightly packed rows
    copyRegion.bufferImageHeight = 0;                                   // Tightly packed image
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Color data
    copyRegion.imageSubresource.mipLevel = 0;                           // Base mip level
    copyRegion.imageSubresource.baseArrayLayer = 0;                     // First array layer
    copyRegion.imageSubresource.layerCount = 1;                         // Single layer
    copyRegion.imageOffset = {0, 0, 0};                                 // Start at image origin
    copyRegion.imageExtent = {width, height, 1};                        // Full image dimensions

    vkCmdCopyImageToBuffer(copyCmd.handle(), outputImage.image(),
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &copyRegion);

    copyCmd.submitAndWait();

    // Map staging buffer memory to CPU address space and convert to 8-bit
    void* mappedData;
    check(vkMapMemory(device, stagingMemory, 0, imageSize, 0, &mappedData));

    float* floatData = static_cast<float*>(mappedData);     // Cast to float array
    vector<unsigned char> outputPixels(width * height * 4); // 8-bit RGBA output

    // Convert from 32-bit float [0.0, 1.0] to 8-bit integer [0, 255]
    for (uint32_t i = 0; i < width * height * 4; ++i) {
        float value = floatData[i];
        value = glm::clamp(value, 0.0f, 1.0f);                        // Ensure valid range
        outputPixels[i] = static_cast<unsigned char>(value * 255.0f); // Scale to 8-bit
    }

    vkUnmapMemory(device, stagingMemory);

    // Save the processed image as JPEG using stb_image_write
    if (!stbi_write_jpg(outputImageFilename.c_str(), width, height, 4, outputPixels.data(), 90)) {
        exitWithMessage("Failed to save output image: {}", outputImageFilename);
    }

    printLog("Successfully saved processed image to: {}", outputImageFilename);

    // ========================================================================
    // STEP 10: Cleanup Resources
    // ========================================================================

    // Clean up all Vulkan resources in reverse order of creation
    // This prevents dependency issues during destruction
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr); // Also frees descriptor sets
    vkDestroyPipeline(device, computePipelineHandle, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyShaderModule(device, computeShaderModule, nullptr);

    return 0;
}