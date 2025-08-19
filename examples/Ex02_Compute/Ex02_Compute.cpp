#include "engine/Context.h"
#include "engine/Image2D.h"
#include "engine/Pipeline.h"
#include "engine/ShaderManager.h"
#include "engine/CommandBuffer.h"
#include <stb_image.h>
#include <stb_image_write.h>
#include <glm/glm.hpp>

using namespace hlab;

int main()
{
    Context ctx({}, false);

    auto device = ctx.device();

    string assetsPath = "../../assets/";
    string inputImageFilename = assetsPath + "image.jpg";
    string computeShaderFilename = assetsPath + "shaders/test.comp.spv";
    string outputImageFilename = "output.jpg";

    // 1. Read an image from inputImageFilename
    int width, height, channels;
    unsigned char* inputPixels =
        stbi_load(inputImageFilename.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!inputPixels) {
        exitWithMessage("Failed to load input image: {} ({})", inputImageFilename,
                        stbi_failure_reason());
    }

    printLog("Loaded input image: {}x{} with {} channels", width, height, channels);

    // Create input image with storage usage for compute shader access
    Image2D inputImage(ctx);
    inputImage.updateUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT); // Set storage usage BEFORE creating
    inputImage.createFromPixelData(inputPixels, width, height, 4, false);

    // Create output image (write-only)
    Image2D outputImage(ctx);
    outputImage.createRGBA32F(static_cast<uint32_t>(width), static_cast<uint32_t>(height));

    // 2. Create a compute pipeline from the shader in computeShaderFilename
    ShaderManager shaderManager(ctx, assetsPath + "shaders/", {{"compute", {"test.comp"}}});

    // Create pipeline layout first
    Pipeline computePipeline(ctx, shaderManager, "compute", VK_FORMAT_UNDEFINED,
                             VK_FORMAT_UNDEFINED, VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM);

    // Get shader stage info
    vector<VkPipelineShaderStageCreateInfo> shaderStagesCI =
        shaderManager.createPipelineShaderStageCIs("compute");

    if (shaderStagesCI.empty()) {
        exitWithMessage("No compute shader stages found");
    }

    // Create compute pipeline manually
    VkComputePipelineCreateInfo pipelineCI{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineCI.layout = computePipeline.pipelineLayout();
    pipelineCI.stage = shaderStagesCI[0]; // Only one shader stage for compute
    pipelineCI.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCI.basePipelineIndex = -1;

    VkPipeline computePipelineHandle;
    check(vkCreateComputePipelines(device, ctx.pipelineCache(), 1, &pipelineCI, nullptr,
                                   &computePipelineHandle));

    // 3. Create descriptor sets for the compute shader
    auto& descriptorPool = ctx.descriptorPool();
    vector<VkDescriptorSetLayout> layouts = descriptorPool.layoutsForPipeline("compute");

    if (layouts.empty()) {
        exitWithMessage("No descriptor set layouts found for compute pipeline");
    }

    // Allocate descriptor set directly using the existing method
    VkDescriptorSet descriptorSet = descriptorPool.allocateDescriptorSet(layouts[0]);

    // Update descriptor set with input and output images
    VkDescriptorImageInfo inputImageInfo{};
    inputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    inputImageInfo.imageView = inputImage.view();
    inputImageInfo.sampler = VK_NULL_HANDLE;

    VkDescriptorImageInfo outputImageInfo{};
    outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputImageInfo.imageView = outputImage.view();
    outputImageInfo.sampler = VK_NULL_HANDLE;

    vector<VkWriteDescriptorSet> descriptorWrites;

    // Binding 0: Input image (readonly)
    VkWriteDescriptorSet inputImageWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    inputImageWrite.dstSet = descriptorSet;
    inputImageWrite.dstBinding = 0;
    inputImageWrite.dstArrayElement = 0;
    inputImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    inputImageWrite.descriptorCount = 1;
    inputImageWrite.pImageInfo = &inputImageInfo;
    descriptorWrites.push_back(inputImageWrite);

    // Binding 1: Output image (writeonly)
    VkWriteDescriptorSet outputImageWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    outputImageWrite.dstSet = descriptorSet;
    outputImageWrite.dstBinding = 1;
    outputImageWrite.dstArrayElement = 0;
    outputImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outputImageWrite.descriptorCount = 1;
    outputImageWrite.pImageInfo = &outputImageInfo;
    descriptorWrites.push_back(outputImageWrite);

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);

    // 4. Run the shader pipeline to process inputImage
    CommandBuffer computeCmd =
        ctx.createComputeCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Transition input image to general layout for compute shader access
    VkImageMemoryBarrier2 inputBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    inputBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    inputBarrier.srcAccessMask = 0;
    inputBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    inputBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    inputBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inputBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    inputBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    inputBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    inputBarrier.image = inputImage.image();
    inputBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    // Transition output image to general layout for compute shader access
    VkImageMemoryBarrier2 outputBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    outputBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    outputBarrier.srcAccessMask = 0;
    outputBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    outputBarrier.dstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    outputBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    outputBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outputBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    outputBarrier.image = outputImage.image();
    outputBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vector<VkImageMemoryBarrier2> imageBarriers = {inputBarrier, outputBarrier};

    VkDependencyInfo dependencyInfo{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size());
    dependencyInfo.pImageMemoryBarriers = imageBarriers.data();

    vkCmdPipelineBarrier2(computeCmd.handle(), &dependencyInfo);

    // Bind compute pipeline and descriptor sets (using our manually created pipeline)
    vkCmdBindPipeline(computeCmd.handle(), VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineHandle);
    vkCmdBindDescriptorSets(computeCmd.handle(), VK_PIPELINE_BIND_POINT_COMPUTE,
                            computePipeline.pipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

    // Dispatch compute shader (16x16 local work group size from shader)
    uint32_t groupCountX = (width + 15) / 16;
    uint32_t groupCountY = (height + 15) / 16;
    vkCmdDispatch(computeCmd.handle(), groupCountX, groupCountY, 1);

    // Transition output image back for transfer
    VkImageMemoryBarrier2 transferBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    transferBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    transferBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    transferBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    transferBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    transferBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    transferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    transferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transferBarrier.image = outputImage.image();
    transferBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkDependencyInfo transferDependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    transferDependency.imageMemoryBarrierCount = 1;
    transferDependency.pImageMemoryBarriers = &transferBarrier;

    vkCmdPipelineBarrier2(computeCmd.handle(), &transferDependency);

    computeCmd.submitAndWait();

    // 5. Save the result to outputImageFilename
    // Create staging buffer to copy image data back to CPU
    VkDeviceSize imageSize = width * height * 4 * sizeof(float); // RGBA32F

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = imageSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    check(vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer));

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo2{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo2.allocationSize = memReqs.size;
    allocInfo2.memoryTypeIndex =
        ctx.getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    check(vkAllocateMemory(device, &allocInfo2, nullptr, &stagingMemory));
    check(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0));

    // Copy image to staging buffer
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
    copyRegion.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

    vkCmdCopyImageToBuffer(copyCmd.handle(), outputImage.image(),
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &copyRegion);

    copyCmd.submitAndWait();

    // Map staging buffer and convert to 8-bit for saving
    void* mappedData;
    check(vkMapMemory(device, stagingMemory, 0, imageSize, 0, &mappedData));

    float* floatData = static_cast<float*>(mappedData);
    vector<unsigned char> outputPixels(width * height * 4);

    // Convert float data to 8-bit
    for (int i = 0; i < width * height * 4; ++i) {
        float value = floatData[i];
        value = glm::clamp(value, 0.0f, 1.0f);
        outputPixels[i] = static_cast<unsigned char>(value * 255.0f);
    }

    vkUnmapMemory(device, stagingMemory);

    // Save as JPEG
    if (!stbi_write_jpg(outputImageFilename.c_str(), width, height, 4, outputPixels.data(), 90)) {
        exitWithMessage("Failed to save output image: {}", outputImageFilename);
    }

    printLog("Successfully saved processed image to: {}", outputImageFilename);

    // Cleanup
    stbi_image_free(inputPixels);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    vkDestroyPipeline(device, computePipelineHandle, nullptr);

    return 0;
}