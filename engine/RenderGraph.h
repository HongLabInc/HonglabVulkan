#pragma once

#include "DescriptorSet.h"
#include <string>
#include <vector>

namespace hlab {

using namespace std;

class RenderGraph
{
  public:
    struct RenderNode // RenderPass
    {
        string pipeline;
        vector<string> msaaColorAttachments;
        vector<string> msaaDepthAttachments;
        vector<string> msaaStencilAttachments;
        vector<string> colorAttachments;
        vector<string> depthAttachments;
        vector<string> stencilAttachments;
        vector<string> descriptorSets;
    };

    void addRenderNode(RenderNode node)
    {
        renderNodes_.push_back(node);
    }

  private:
    vector<RenderNode> renderNodes_;
    // 여기서는 파이프라인들이 하나씩 순서대로 실행되는 간단한 구조
};

} // namespace hlab