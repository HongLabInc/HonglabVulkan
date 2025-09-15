#include "RenderGraph.h"
#include <iostream>
#include <algorithm>

namespace hlab {

void RenderGraph::writeToFile(const string& filename) const
{
    ofstream file(filename);
    if (!file.is_open()) {
        cerr << "Failed to open file for writing: " << filename << endl;
        return;
    }

    file << "{\n";
    file << "  \"renderNodes\": [\n";

    for (size_t i = 0; i < renderNodes_.size(); ++i) {
        const auto& node = renderNodes_[i];
        
        file << "    {\n";
        file << "      \"pipelineNames\": " << vectorToJsonArray(node.pipelineNames) << ",\n";
        file << "      \"colorAttachments\": " << vectorToJsonArray(node.colorAttachments) << ",\n";
        file << "      \"depthAttachment\": \"" << escapeJsonString(node.depthAttachment) << "\",\n";
        file << "      \"stencilAttachment\": \"" << escapeJsonString(node.stencilAttachment) << "\"\n";
        file << "    }";
        
        if (i < renderNodes_.size() - 1) {
            file << ",";
        }
        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";

    file.close();
}

bool RenderGraph::readFromFile(const string& filename)
{
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Failed to open file for reading: " << filename << endl;
        return false;
    }

    // Read entire file content
    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();
    file.close();

    // Clear existing render nodes
    renderNodes_.clear();

    try {
        // Simple JSON parsing for our specific format
        size_t renderNodesStart = content.find("\"renderNodes\"");
        if (renderNodesStart == string::npos) {
            cerr << "Invalid JSON format: missing renderNodes array" << endl;
            return false;
        }

        size_t arrayStart = content.find("[", renderNodesStart);
        size_t arrayEnd = content.find_last_of("]");
        
        if (arrayStart == string::npos || arrayEnd == string::npos) {
            cerr << "Invalid JSON format: malformed renderNodes array" << endl;
            return false;
        }

        string arrayContent = content.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
        
        // Parse individual render nodes
        size_t pos = 0;
        while (pos < arrayContent.length()) {
            size_t nodeStart = arrayContent.find("{", pos);
            if (nodeStart == string::npos) break;

            size_t nodeEnd = arrayContent.find("}", nodeStart);
            if (nodeEnd == string::npos) break;

            string nodeContent = arrayContent.substr(nodeStart + 1, nodeEnd - nodeStart - 1);
            
            RenderNode node;
            
            // Parse each field
            node.pipelineNames = parseJsonField(nodeContent, "pipelineNames");
            node.colorAttachments = parseJsonField(nodeContent, "colorAttachments");
            node.depthAttachment = parseJsonStringField(nodeContent, "depthAttachment");
            node.stencilAttachment = parseJsonStringField(nodeContent, "stencilAttachment");
            
            renderNodes_.push_back(node);
            
            pos = nodeEnd + 1;
        }

        return true;
    }
    catch (const exception& e) {
        cerr << "Error parsing JSON file: " << e.what() << endl;
        renderNodes_.clear();
        return false;
    }
}

string RenderGraph::vectorToJsonArray(const vector<string>& vec) const
{
    if (vec.empty()) {
        return "[]";
    }

    string result = "[";
    for (size_t i = 0; i < vec.size(); ++i) {
        result += "\"" + escapeJsonString(vec[i]) + "\"";
        if (i < vec.size() - 1) {
            result += ", ";
        }
    }
    result += "]";
    return result;
}

vector<string> RenderGraph::jsonArrayToVector(const string& jsonArray) const
{
    vector<string> result;
    
    if (jsonArray.empty() || jsonArray == "[]") {
        return result;
    }

    size_t start = jsonArray.find('[');
    size_t end = jsonArray.find_last_of(']');
    
    if (start == string::npos || end == string::npos) {
        return result;
    }

    string content = jsonArray.substr(start + 1, end - start - 1);
    
    size_t pos = 0;
    while (pos < content.length()) {
        size_t quoteStart = content.find('"', pos);
        if (quoteStart == string::npos) break;
        
        size_t quoteEnd = content.find('"', quoteStart + 1);
        if (quoteEnd == string::npos) break;
        
        string item = content.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
        result.push_back(unescapeJsonString(item));
        
        pos = quoteEnd + 1;
    }
    
    return result;
}

string RenderGraph::escapeJsonString(const string& str) const
{
    string result = str;
    
    // Replace special characters
    size_t pos = 0;
    while ((pos = result.find('\\', pos)) != string::npos) {
        result.replace(pos, 1, "\\\\");
        pos += 2;
    }
    
    pos = 0;
    while ((pos = result.find('"', pos)) != string::npos) {
        result.replace(pos, 1, "\\\"");
        pos += 2;
    }
    
    return result;
}

string RenderGraph::unescapeJsonString(const string& str) const
{
    string result = str;
    
    // Replace escaped characters
    size_t pos = 0;
    while ((pos = result.find("\\\"", pos)) != string::npos) {
        result.replace(pos, 2, "\"");
        pos += 1;
    }
    
    pos = 0;
    while ((pos = result.find("\\\\", pos)) != string::npos) {
        result.replace(pos, 2, "\\");
        pos += 1;
    }
    
    return result;
}

vector<string> RenderGraph::parseJsonField(const string& nodeContent, const string& fieldName) const
{
    string searchStr = "\"" + fieldName + "\"";
    size_t fieldPos = nodeContent.find(searchStr);
    if (fieldPos == string::npos) {
        return vector<string>();
    }
    
    size_t colonPos = nodeContent.find(":", fieldPos);
    if (colonPos == string::npos) {
        return vector<string>();
    }
    
    size_t arrayStart = nodeContent.find("[", colonPos);
    if (arrayStart == string::npos) {
        return vector<string>();
    }
    
    size_t arrayEnd = nodeContent.find("]", arrayStart);
    if (arrayEnd == string::npos) {
        return vector<string>();
    }
    
    string arrayStr = nodeContent.substr(arrayStart, arrayEnd - arrayStart + 1);
    return jsonArrayToVector(arrayStr);
}

string RenderGraph::parseJsonStringField(const string& nodeContent, const string& fieldName) const
{
    string searchStr = "\"" + fieldName + "\"";
    size_t fieldPos = nodeContent.find(searchStr);
    if (fieldPos == string::npos) {
        return "";
    }
    
    size_t colonPos = nodeContent.find(":", fieldPos);
    if (colonPos == string::npos) {
        return "";
    }
    
    size_t quoteStart = nodeContent.find("\"", colonPos);
    if (quoteStart == string::npos) {
        return "";
    }
    
    size_t quoteEnd = nodeContent.find("\"", quoteStart + 1);
    if (quoteEnd == string::npos) {
        return "";
    }
    
    string value = nodeContent.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
    return unescapeJsonString(value);
}

} // namespace hlab
