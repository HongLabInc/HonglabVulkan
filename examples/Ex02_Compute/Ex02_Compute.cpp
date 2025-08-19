#include "engine/Context.h"
#include "engine/Image2D.h"
#include "engine/Pipeline.h"
#include "engine/ShaderManager.h"
#include "engine/CommandBuffer.h"
#include <stb_image.h>
#include <stb_image_write.h>
#include <glm/glm.hpp>
#include <fstream>

using namespace hlab;

// Helper function to read SPIR-V binary file
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

// Helper function to create VkShaderModule
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
    Context ctx({}, false);

    auto device = ctx.device();

    string assetsPath = "../../assets/";
    string inputImageFilename = assetsPath + "image.jpg";
    string computeShaderFilename = assetsPath + "shaders/test.comp.spv";
    string outputImageFilename = "output.jpg";

    Image2D inputImage(ctx);
    inputImage.updateUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT); // Set storage usage before creating
    inputImage.createTextureFromImage(inputImageFilename, false, false);

    uint32_t width = inputImage.width();
    uint32_t height = inputImage.height();

    Image2D outputImage(ctx);
    outputImage.createImage(VK_FORMAT_R32G32B32A32_SFLOAT, width, height, VK_SAMPLE_COUNT_1_BIT,
                            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                            VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 0, VK_IMAGE_VIEW_TYPE_2D);

    // 1. Load and create VkShaderModule from SPIR-V file
    vector<char> shaderCode = readSpvFile(computeShaderFilename);
    VkShaderModule computeShaderModule = createShaderModule(device, shaderCode);

    // 2. Create descriptor set layout manually for the compute shader
    // Based on shader: binding 0 = input image (readonly), binding 1 = output image (writeonly)
    vector<VkDescriptorSetLayoutBinding> bindings(2);

    // Binding 0: Input image (readonly storage image)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    // Binding 1: Output image (writeonly storage image)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    descriptorLayoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
    descriptorLayoutCI.pBindings = bindings.data();

    VkDescriptorSetLayout descriptorSetLayout;
    check(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &descriptorSetLayout));

    // 3. Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutCI{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutCI.setLayoutCount = 1;
    pipelineLayoutCI.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutCI.pushConstantRangeCount = 0;
    pipelineLayoutCI.pPushConstantRanges = nullptr;

    VkPipelineLayout pipelineLayout;
    check(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

    // 4. Create compute pipeline
    VkPipelineShaderStageCreateInfo shaderStageCI{
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    shaderStageCI.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageCI.module = computeShaderModule;
    shaderStageCI.pName = "main";
    shaderStageCI.pSpecializationInfo = nullptr;

    VkComputePipelineCreateInfo pipelineCI{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipelineCI.layout = pipelineLayout;
    pipelineCI.stage = shaderStageCI;
    pipelineCI.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCI.basePipelineIndex = -1;

    VkPipeline computePipelineHandle;
    check(vkCreateComputePipelines(device, ctx.pipelineCache(), 1, &pipelineCI, nullptr,
                                   &computePipelineHandle));

    // 5. Create descriptor pool and allocate descriptor set
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize.descriptorCount = 2; // 2 storage images

    VkDescriptorPoolCreateInfo poolCI{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolCI.poolSizeCount = 1;
    poolCI.pPoolSizes = &poolSize;
    poolCI.maxSets = 1;

    VkDescriptorPool descriptorPool;
    check(vkCreateDescriptorPool(device, &poolCI, nullptr, &descriptorPool));

    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorSet descriptorSet;
    check(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

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

    // 6. Run the shader pipeline to process inputImage
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

    // Bind compute pipeline and descriptor sets
    vkCmdBindPipeline(computeCmd.handle(), VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineHandle);
    vkCmdBindDescriptorSets(computeCmd.handle(), VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0,
                            1, &descriptorSet, 0, nullptr);

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

    // 7. Save the result to outputImageFilename
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
    copyRegion.imageExtent = {width, height, 1};

    vkCmdCopyImageToBuffer(copyCmd.handle(), outputImage.image(),
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &copyRegion);

    copyCmd.submitAndWait();

    // Map staging buffer and convert to 8-bit for saving
    void* mappedData;
    check(vkMapMemory(device, stagingMemory, 0, imageSize, 0, &mappedData));

    float* floatData = static_cast<float*>(mappedData);
    vector<unsigned char> outputPixels(width * height * 4);

    // Convert float data to 8-bit
    for (uint32_t i = 0; i < width * height * 4; ++i) {
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
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyPipeline(device, computePipelineHandle, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyShaderModule(device, computeShaderModule, nullptr);

    return 0;
}