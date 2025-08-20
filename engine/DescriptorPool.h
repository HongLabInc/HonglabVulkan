#pragma once

#include "VulkanTools.h"
#include "Logger.h"
#include <vector>
#include <unordered_map>
#include <map>

namespace hlab {

using namespace std;

// 안내: On-demand DescriptorPool
// 여기서의 목표는 필요로 하는 정확한 최대 크기를 확인하는 것입니다.
// 그 다음에 실행시킬때 최대 크기로 바로 초기화를 하는 것이 목표입니다.

struct LayoutInfo
{
    vector<VkDescriptorSetLayoutBinding> bindings_{};
    vector<tuple<string, uint32_t>> pipelineNamesAndSetNumbers_;
};

class DescriptorPool
{
    friend class Context;

  public:
    DescriptorPool(VkDevice& device);
    ~DescriptorPool();

    void createFromScript();
    void createNewPool(const vector<VkDescriptorPoolSize>& typeCounts, uint32_t maxSets);
    void printAllocatedStatistics() const;
    void updateRemainingCapacity(const vector<VkDescriptorSetLayoutBinding>& bindings,
                                 uint32_t numSets);
    auto
    canAllocateFromRemaining(const unordered_map<VkDescriptorType, uint32_t>& requiredTypeCounts,
                             uint32_t numRequiredSets) const -> bool;
    auto allocateDescriptorSet(const VkDescriptorSetLayout& descriptorSetLayouts)
        -> VkDescriptorSet;
    auto allocateDescriptorSets(const vector<VkDescriptorSetLayout>& descriptorSetLayouts)
        -> vector<VkDescriptorSet>;
    void createLayouts(const vector<LayoutInfo>& layoutInfos);

    vector<VkDescriptorSetLayout> layoutsForPipeline(string pipelineName)
    {
        vector<VkDescriptorSetLayout> layouts;

        for (const auto& [layout, layoutInfo] : layoutsAndInfos_) {
            // Check if this layout is used by the specified pipeline
            for (const auto& [storedPipelineName, setNumber] :
                 layoutInfo.pipelineNamesAndSetNumbers_) {
                if (storedPipelineName == pipelineName) {
                    // cout << pipelineName << " " << setNumber << endl;
                    if (layouts.size() < setNumber + 1) {
                        layouts.resize(setNumber + 1);
                    }
                    layouts[setNumber] = layout;
                }
            }
        }

        return layouts;
    }

    auto descriptorSetLayout(const vector<VkDescriptorSetLayoutBinding>& bindings)
        -> const VkDescriptorSetLayout&;
    auto layoutToBindings(const VkDescriptorSetLayout& layout)
        -> const vector<VkDescriptorSetLayoutBinding>&;

  private:
    const string kScriptFilename_ = "DescriptorPoolSize.txt";

    VkDevice& device_;
    vector<VkDescriptorPool> descriptorPools_{};

    // Usage tracking
    unordered_map<VkDescriptorType, uint32_t> allocatedTypeCounts_{};
    unordered_map<VkDescriptorType, uint32_t> remainingTypeCounts_{};
    uint32_t allocatedSets_ = 0;
    uint32_t remainingSets_ = 0;

    void cleanup();

    vector<tuple<VkDescriptorSetLayout, LayoutInfo>> layoutsAndInfos_{};
};

} // namespace hlab
