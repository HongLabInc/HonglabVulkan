#pragma once

#include "DescriptorSet.h"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

namespace hlab {

using namespace std;

class RenderGraph
{
    friend class Renderer;

  public:
    struct RenderNode // RenderPass
    {
        vector<string> pipelineNames;
        vector<string> msaaColorAttachments;
        string msaaDepthAttachment;
        string msaaStencilAttachment;
        vector<string> colorAttachments;
        string depthAttachment;
        string stencilAttachment;
    };

    void addRenderNode(RenderNode node)
    {
        renderNodes_.push_back(node);
    }

    void writeToFile(const string& filename) const;
    bool readFromFile(const string& filename);

  private:
    vector<RenderNode> renderNodes_;
    // 여기서는 파이프라인들이 하나씩 순서대로 실행되는 간단한 구조

    // Helper methods for JSON serialization
    string vectorToJsonArray(const vector<string>& vec) const;
    vector<string> jsonArrayToVector(const string& jsonArray) const;
    string escapeJsonString(const string& str) const;
    string unescapeJsonString(const string& str) const;
    vector<string> parseJsonField(const string& nodeContent, const string& fieldName) const;
    string parseJsonStringField(const string& nodeContent, const string& fieldName) const;
};

} // namespace hlab