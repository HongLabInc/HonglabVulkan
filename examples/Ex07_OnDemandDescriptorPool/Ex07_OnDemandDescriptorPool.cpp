#include "engine/Context.h"
#include "engine/Image2D.h"
#include "engine/CommandBuffer.h"
#include "engine/Pipeline.h"
#include "engine/ShaderManager.h"
#include "engine/DescriptorSet.h"
#include "engine/MappedBuffer.h"
#include <stb_image.h>
#include <stb_image_write.h>
#include <glm/glm.hpp>
#include <fstream>

using namespace hlab;

int main()
{
    Context ctx({}, false);
    auto device = ctx.device();

    string assetsPath = "../../assets/";
    string inputImageFilename = assetsPath + "image.jpg";
    string outputImageFilename = "output.jpg";

    Image2D inputImage(ctx);
    inputImage.updateUsageFlags(VK_IMAGE_USAGE_STORAGE_BIT);
    inputImage.createTextureFromImage(inputImageFilename, false, false);

    uint32_t width = inputImage.width();
    uint32_t height = inputImage.height();

    Image2D outputImage(ctx);
    outputImage.createImage(VK_FORMAT_R32G32B32A32_SFLOAT, width, height, VK_SAMPLE_COUNT_1_BIT,
                            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                            VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 0, VK_IMAGE_VIEW_TYPE_2D);

    ShaderManager shaderManager(ctx, assetsPath + "shaders/", {{"compute", {"test.comp.spv"}}});

    Pipeline computePipeline(ctx, shaderManager);
    computePipeline.createByName("compute");

    CommandBuffer cmd = ctx.createGraphicsCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    inputImage.transitionTo(cmd.handle(), VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    outputImage.transitionTo(cmd.handle(), VK_ACCESS_2_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);

    DescriptorSet descriptorSet;
    descriptorSet.create(ctx, {inputImage.resourceBinding(), outputImage.resourceBinding()});

    vkCmdBindPipeline(cmd.handle(), VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pipeline());

    vkCmdBindDescriptorSets(cmd.handle(), VK_PIPELINE_BIND_POINT_COMPUTE,
                            computePipeline.pipelineLayout(), 0, 1, &descriptorSet.handle(), 0,
                            nullptr);

    uint32_t groupCountX = (width + 31) / 32;
    uint32_t groupCountY = (height + 31) / 32;
    vkCmdDispatch(cmd.handle(), groupCountX, groupCountY, 1);

    outputImage.transitionTo(cmd.handle(), VK_ACCESS_2_TRANSFER_READ_BIT,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             VK_PIPELINE_STAGE_2_TRANSFER_BIT);

    VkDeviceSize imageSize = width * height * 4 * sizeof(float);

    MappedBuffer stagingBuffer(ctx);
    stagingBuffer.createStagingBuffer(imageSize, nullptr);

    VkBufferImageCopy copyRegion{
        0, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {width, height, 1}};

    vkCmdCopyImageToBuffer(cmd.handle(), outputImage.image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           stagingBuffer.buffer(), 1, &copyRegion);

    cmd.submitAndWait();

    float* floatData = static_cast<float*>(stagingBuffer.mapped());
    vector<unsigned char> outputPixels(width * height * 4);

    for (uint32_t i = 0; i < width * height * 4; ++i) {
        float value = glm::clamp(floatData[i], 0.0f, 1.0f);
        outputPixels[i] = static_cast<unsigned char>(value * 255.0f);
    }

    if (!stbi_write_jpg(outputImageFilename.c_str(), width, height, 4, outputPixels.data(), 90)) {
        exitWithMessage("Failed to save output image: {}", outputImageFilename);
    }

    printLog("Successfully saved processed image to: {}", outputImageFilename);

    return 0;
}