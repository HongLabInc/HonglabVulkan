#include "engine/Context.h"
#include "engine/Image2D.h"
#include "engine/CommandBuffer.h"
#include "engine/Pipeline.h"
#include "engine/PipelineConfig.h"
#include "engine/ShaderManager.h"
#include "engine/MappedBuffer.h"
#include <stb_image.h>
#include <stb_image_write.h>
#include <glm/glm.hpp>
#include <fstream>
#include <string>

using namespace hlab;
using namespace std;

int main()
{
    string assetsPath = "../../assets/";
    string outputImageFilename = "output.jpg";

    Context ctx({}, false);
    auto device = ctx.device();

    const uint32_t width = 640;
    const uint32_t height = 480;

    Image2D colorImage(ctx);
    colorImage.createImage(VK_FORMAT_R8G8B8A8_UNORM, width, height, VK_SAMPLE_COUNT_1_BIT,
                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                           VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 0, VK_IMAGE_VIEW_TYPE_2D);

    ShaderManager shaderManager(ctx, assetsPath + "shaders/",
                                {{"triangle", {"triangle.vert.spv", "triangle.frag.spv"}}});

    Pipeline trianglePipeline(ctx, shaderManager, PipelineConfig::createTriangle(),
                             VK_FORMAT_R8G8B8A8_UNORM, nullopt, VK_SAMPLE_COUNT_1_BIT);

    CommandBuffer renderCmd =
        ctx.createGraphicsCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    colorImage.transitionToColorAttachment(renderCmd.handle());

    VkRenderingAttachmentInfo colorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    colorAttachment.imageView = colorImage.view();
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {1.0f, 1.0f, 1.0f, 1.0f};

    VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderingInfo.renderArea = {{0, 0}, {width, height}};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(renderCmd.handle(), &renderingInfo);

    vkCmdBindPipeline(renderCmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                      trianglePipeline.pipeline());

    VkViewport viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height),
                        0.0f, 1.0f};
    vkCmdSetViewport(renderCmd.handle(), 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, {width, height}};
    vkCmdSetScissor(renderCmd.handle(), 0, 1, &scissor);

    vkCmdDraw(renderCmd.handle(), 3, 1, 0, 0);

    vkCmdEndRendering(renderCmd.handle());

    colorImage.transitionToTransferSrc(renderCmd.handle());

    renderCmd.submitAndWait();

    VkDeviceSize imageSize = width * height * 4;

    MappedBuffer stagingBuffer(ctx);
    stagingBuffer.createStagingBuffer(imageSize, nullptr);

    CommandBuffer copyCmd = ctx.createTransferCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkBufferImageCopy copyRegion{
        0, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {width, height, 1}};

    vkCmdCopyImageToBuffer(copyCmd.handle(), colorImage.image(),
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer.buffer(), 1,
                           &copyRegion);

    copyCmd.submitAndWait();

    unsigned char* pixelData = static_cast<unsigned char*>(stagingBuffer.mapped());

    if (!stbi_write_jpg(outputImageFilename.c_str(), width, height, 4, pixelData, 90)) {
        exitWithMessage("Failed to save output image: {}", outputImageFilename);
    }

    printLog("Successfully saved rendered triangle to: {}", outputImageFilename);

    return 0;
}
