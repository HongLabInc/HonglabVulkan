#include "GuiRenderer.h"
#include "PipelineConfig.h"

namespace hlab {

GuiRenderer::GuiRenderer(Context& ctx, ShaderManager& shaderManager, VkFormat colorFormat)
    : ctx_(ctx), shaderManager_(shaderManager), 
      frameData_{FrameData(ctx), FrameData(ctx)}, // Initialize frame data array
      fontImage_(make_unique<Image2D>(ctx)), fontSampler_(ctx), pushConsts_(ctx), 
      guiPipeline_(ctx, shaderManager_, PipelineConfig::createGui(), colorFormat)
{
    pushConsts_.setStageFlags(VK_SHADER_STAGE_VERTEX_BIT);

    // ImGui 초기화, 스타일 설정
    ImGui::CreateContext();
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
    style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.ScaleAllSizes(scale_);
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = scale_;

    {
        // const string fontFileName = "../../assets/Roboto-Medium.ttf"; // English font
        const string fontFileName =
            "../../assets/Noto_Sans_KR/static/NotoSansKR-SemiBold.ttf"; // Korean Font

        unsigned char* fontData = nullptr;
        int texWidth, texHeight;
        ImGuiIO& io = ImGui::GetIO();

        ImFontConfig config;
        config.MergeMode = false;

        io.Fonts->AddFontFromFileTTF(fontFileName.c_str(), 16.0f * scale_, &config,
                                     io.Fonts->GetGlyphRangesDefault());

        config.MergeMode = true;

        io.Fonts->AddFontFromFileTTF(fontFileName.c_str(), 16.0f * scale_, &config,
                                     io.Fonts->GetGlyphRangesKorean());

        io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
        if (!fontData) {
            exitWithMessage("Failed to load font data from: {}", fontFileName);
        }

        fontImage_->createFromPixelData(fontData, texWidth, texHeight, 4, false);
    }

    fontSampler_.createAnisoRepeat();

    fontImage_->setSampler(fontSampler_.handle()); // VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER

    fontSet_.create(ctx_, {*fontImage_});
}

GuiRenderer::~GuiRenderer()
{
    // Clean up ImGui context first
    if (ImGui::GetCurrentContext()) {
        ImGui::DestroyContext();
    }

    // No manual cleanup required in current architecture.
}

auto GuiRenderer::imguiPipeline() -> Pipeline&
{
    return guiPipeline_;
}

bool GuiRenderer::update(uint32_t frameIndex)
{
    ImDrawData* imDrawData = ImGui::GetDrawData();
    
    if (!imDrawData || imDrawData->TotalVtxCount == 0 || imDrawData->TotalIdxCount == 0) {
        return false;
    }

    auto& frame = frameData_[frameIndex % kMaxFramesInFlight];
    bool updateCmdBuffers = false;

    VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
    VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

    // Update current frame counts every frame (represents actual data size)
    vertexCount_ = imDrawData->TotalVtxCount;
    indexCount_ = imDrawData->TotalIdxCount;

    // Use MappedBuffer's allocatedSize() for capacity checking - no GPU stalls!
    if ((frame.vertexBuffer.buffer() == VK_NULL_HANDLE) || 
        (vertexBufferSize > frame.vertexBuffer.allocatedSize())) {
        
        // Calculate new capacity with growth factor to reduce frequent reallocations
        VkDeviceSize newCapacity = std::max(
            static_cast<VkDeviceSize>(vertexBufferSize * 1.5f), 
            static_cast<VkDeviceSize>(512 * sizeof(ImDrawVert)) // Minimum capacity
        );
        
        frame.vertexBuffer.createVertexBuffer(newCapacity, nullptr);
        updateCmdBuffers = true;
    }

    if ((frame.indexBuffer.buffer() == VK_NULL_HANDLE) || 
        (indexBufferSize > frame.indexBuffer.allocatedSize())) {
        
        // Calculate new capacity with growth factor to reduce frequent reallocations
        VkDeviceSize newCapacity = std::max(
            static_cast<VkDeviceSize>(indexBufferSize * 1.5f), 
            static_cast<VkDeviceSize>(1024 * sizeof(ImDrawIdx)) // Minimum capacity
        );
        
        frame.indexBuffer.createIndexBuffer(newCapacity, nullptr);
        updateCmdBuffers = true;
    }

    // Copy ImGui data to this frame's buffers
    ImDrawVert* vtxDst = (ImDrawVert*)frame.vertexBuffer.mapped();
    ImDrawIdx* idxDst = (ImDrawIdx*)frame.indexBuffer.mapped();

    for (int n = 0; n < imDrawData->CmdListsCount; n++) {
        const ImDrawList* cmd_list = imDrawData->CmdLists[n];
        memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmd_list->VtxBuffer.Size;
        idxDst += cmd_list->IdxBuffer.Size;
    }

    // Flush to ensure GPU visibility (synchronous)
    frame.vertexBuffer.flush();
    frame.indexBuffer.flush();

    return updateCmdBuffers;
}

void GuiRenderer::draw(const VkCommandBuffer cmd, VkImageView swapchainImageView,
                       VkViewport viewport, uint32_t frameIndex)
{
    VkRenderingAttachmentInfo swapchainColorAttachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    swapchainColorAttachment.imageView = swapchainImageView;
    swapchainColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    swapchainColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // Preserve previous content
    swapchainColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo colorOnlyRenderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
    colorOnlyRenderingInfo.renderArea = {0, 0, uint32_t(viewport.width), uint32_t(viewport.height)};
    colorOnlyRenderingInfo.layerCount = 1;
    colorOnlyRenderingInfo.colorAttachmentCount = 1;
    colorOnlyRenderingInfo.pColorAttachments = &swapchainColorAttachment;

    ImDrawData* imDrawData = ImGui::GetDrawData();
    if ((!imDrawData) || (imDrawData->CmdListsCount == 0)) {
        return;
    }

    // Get this frame's buffers
    auto& frame = frameData_[frameIndex % kMaxFramesInFlight];

    vkCmdBeginRendering(cmd, &colorOnlyRenderingInfo);

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    const auto descriptorSet = fontSet_.handle();
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, guiPipeline_.pipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, guiPipeline_.pipelineLayout(), 0,
                            1, &descriptorSet, 0, nullptr);

    ImGuiIO& io = ImGui::GetIO();
    auto& pc = pushConsts_.data();
    pc.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
    pc.translate = glm::vec2(-1.0f);
    pushConsts_.push(cmd, guiPipeline_.pipelineLayout());

    // Bind this frame's vertex and index buffers
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &frame.vertexBuffer.buffer(), offsets);
    vkCmdBindIndexBuffer(cmd, frame.indexBuffer.buffer(), 0, VK_INDEX_TYPE_UINT16);

    // Render ImGui draw commands with fixed vertex offset bug
    int32_t vertexOffset = 0;
    int32_t indexOffset = 0;

    for (int32_t i = 0; i < imDrawData->CmdListsCount; i++) {
        const ImDrawList* cmd_list = imDrawData->CmdLists[i];
        for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++) {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
            VkRect2D scissorRect;
            scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
            scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
            scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
            scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
            vkCmdSetScissor(cmd, 0, 1, &scissorRect);
            vkCmdDrawIndexed(cmd, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
            indexOffset += pcmd->ElemCount;
        }

        vertexOffset += cmd_list->VtxBuffer.Size;
    }

    vkCmdEndRendering(cmd);
}

void GuiRenderer::resize(uint32_t width, uint32_t height)
{
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)(width), (float)(height));
}

} // namespace hlab