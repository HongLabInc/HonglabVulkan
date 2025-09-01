#include "DescriptorPool.h"
#include "Logger.h"
#include <unordered_map>
#include <iostream>

namespace hlab {

DescriptorPool::DescriptorPool(VkDevice& device) : device_(device)
{
}

DescriptorPool::~DescriptorPool()
{
    cleanup();
}

void DescriptorPool::createFromScript()
{
    // if "DescriptorPoolSize.txt" exists, read it and createNewPool
    std::ifstream file(kScriptFilename_);

    if (file.is_open()) {
        printLog("Found DescriptorPoolSize.txt, loading previous statistics...");

        string line;
        vector<VkDescriptorPoolSize> poolSizes;
        uint32_t numSets = 0;

        while (std::getline(file, line)) {
            std::istringstream iss(line);
            string typeStr;
            uint32_t count;

            if (iss >> typeStr >> count) {
                if (typeStr == "NumSets") {
                    numSets = count;
                } else {
                    // Convert string to VkDescriptorType using VulkanTools function
                    VkDescriptorType type = stringToDescriptorType(typeStr);
                    if (type != VK_DESCRIPTOR_TYPE_MAX_ENUM) {
                        VkDescriptorPoolSize poolSize{};
                        poolSize.type = type;
                        poolSize.descriptorCount = count;
                        poolSizes.push_back(poolSize);
                    }
                }
            }
        }
        file.close();

        // Create pool with loaded statistics if we have valid data
        if (numSets > 0 && !poolSizes.empty()) {
            createNewPool(poolSizes, numSets);
            printLog("Created initial pool with {} sets and {} descriptor types", numSets,
                     poolSizes.size());
        }
    } else {
        printLog("DescriptorPoolSize.txt not found, will create pools on-demand");
    }
}

bool DescriptorPool::canAllocateFromRemaining(
    const unordered_map<VkDescriptorType, uint32_t>& requiredTypeCounts,
    uint32_t numRequiredSets) const
{
    if (descriptorPools_.empty()) {
        return false;
    }

    if (remainingSets_ < numRequiredSets) {
        return false;
    }

    for (const auto& [type, required] : requiredTypeCounts) {
        auto it = remainingTypeCounts_.find(type);
        if (it == remainingTypeCounts_.end()) {
            return false;
        }
        if (it->second < required) {
            return false;
        }
    }

    return true;
}

void DescriptorPool::createNewPool(const vector<VkDescriptorPoolSize>& typeCounts, uint32_t maxSets)
{
    // Create descriptor pool directly using the provided VkDescriptorPoolSize vector
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.flags = 0;
    poolInfo.maxSets = maxSets;
    poolInfo.poolSizeCount = static_cast<uint32_t>(typeCounts.size());
    poolInfo.pPoolSizes = typeCounts.data();

    VkDescriptorPool newPool = VK_NULL_HANDLE;
    check(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &newPool));

    descriptorPools_.push_back(newPool);

    remainingSets_ += maxSets;
    for (const auto& poolSize : typeCounts) {
        remainingTypeCounts_[poolSize.type] += poolSize.descriptorCount;
    }
}

void DescriptorPool::updateRemainingCapacity(const vector<VkDescriptorSetLayoutBinding>& bindings,
                                             uint32_t numSets)
{
    // Update remaining sets
    remainingSets_ -= numSets;

    // Update remaining type counts
    for (const auto& binding : bindings) {
        remainingTypeCounts_[binding.descriptorType] -= binding.descriptorCount * numSets;
    }
}

VkDescriptorSet
DescriptorPool::allocateDescriptorSet(const VkDescriptorSetLayout& descriptorSetLayout)
{
    // 1. retrieve binding from descriptorSetLayouts
    const auto& bindings = layoutToBindings(descriptorSetLayout);

    // Calculate required descriptor counts for this allocation
    unordered_map<VkDescriptorType, uint32_t> requiredTypeCounts;
    for (const auto& binding : bindings) {
        requiredTypeCounts[binding.descriptorType] += binding.descriptorCount;
    }

    // 2. check if remainingSet and remainingTypeCounts are enough
    if (!canAllocateFromRemaining(requiredTypeCounts, 1)) {
        // 3. Convert to VkDescriptorPoolSize vector for exact pool creation
        vector<VkDescriptorPoolSize> poolSizes;
        poolSizes.reserve(requiredTypeCounts.size());

        for (const auto& [type, count] : requiredTypeCounts) {
            VkDescriptorPoolSize poolSize{};
            poolSize.type = type;
            poolSize.descriptorCount = count;
            poolSizes.push_back(poolSize);
        }

        createNewPool(poolSizes, 1);
    }

    // 4. allocate using the last pool
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = descriptorPools_.back();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
    VkResult res = vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet);
    if (res == VK_ERROR_OUT_OF_POOL_MEMORY) {
        exitWithMessage("Unexpected VK_ERROR_OUT_OF_POOL_MEMORY after pool creation");
    }
    check(res);

    // 5. update remainingSet, remainingTypeCounts, allocatedSets, allocatedTypeCounts
    updateRemainingCapacity(bindings, 1);
    allocatedSets_++;

    for (const auto& binding : bindings) {
        allocatedTypeCounts_[binding.descriptorType] += binding.descriptorCount;
    }

    return descriptorSet;
}

auto DescriptorPool::allocateDescriptorSets(
    const vector<VkDescriptorSetLayout>& descriptorSetLayouts) -> vector<VkDescriptorSet>
{
    // Calculate total required descriptor counts for all layouts
    unordered_map<VkDescriptorType, uint32_t> totalRequiredTypeCounts;
    for (const auto& layout : descriptorSetLayouts) {
        const auto& bindings = layoutToBindings(layout);
        for (const auto& binding : bindings) {
            totalRequiredTypeCounts[binding.descriptorType] += binding.descriptorCount;
        }
    }

    // Check if we can allocate all sets from remaining capacity
    uint32_t numRequiredSets = static_cast<uint32_t>(descriptorSetLayouts.size());
    if (!canAllocateFromRemaining(totalRequiredTypeCounts, numRequiredSets)) {
        // Convert to VkDescriptorPoolSize vector for exact pool creation
        vector<VkDescriptorPoolSize> poolSizes;
        poolSizes.reserve(totalRequiredTypeCounts.size());

        for (const auto& [type, count] : totalRequiredTypeCounts) {
            VkDescriptorPoolSize poolSize{};
            poolSize.type = type;
            poolSize.descriptorCount = count;
            poolSizes.push_back(poolSize);
        }

        createNewPool(poolSizes, numRequiredSets);
    }

    // Allocate using the last pool
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = descriptorPools_.back();
    allocInfo.descriptorSetCount = numRequiredSets;
    allocInfo.pSetLayouts = descriptorSetLayouts.data();

    vector<VkDescriptorSet> descriptorSets(allocInfo.descriptorSetCount, VK_NULL_HANDLE);
    VkResult res = vkAllocateDescriptorSets(device_, &allocInfo, descriptorSets.data());
    if (res == VK_ERROR_OUT_OF_POOL_MEMORY) {
        exitWithMessage("Unexpected VK_ERROR_OUT_OF_POOL_MEMORY after pool creation");
    }
    check(res);

    // Update remaining capacity
    for (const auto& layout : descriptorSetLayouts) {
        const auto& bindings = layoutToBindings(layout);
        updateRemainingCapacity(bindings, 1);
    }

    // Track allocated sets
    allocatedSets_ += numRequiredSets;

    // Track descriptor types used in these layouts
    for (const auto& layout : descriptorSetLayouts) {
        const auto& bindings = layoutToBindings(layout);
        for (const auto& binding : bindings) {
            allocatedTypeCounts_[binding.descriptorType] += binding.descriptorCount;
        }
    }

    return descriptorSets;
}

void DescriptorPool::createLayouts(const vector<LayoutInfo>& layoutInfos)
{
    for (const auto& l : layoutInfos) {
        VkDescriptorSetLayoutCreateInfo createInfo{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        createInfo.bindingCount = static_cast<uint32_t>(l.bindings_.size());
        createInfo.pBindings = l.bindings_.data();
        VkDescriptorSetLayout layout{VK_NULL_HANDLE};
        check(vkCreateDescriptorSetLayout(device_, &createInfo, nullptr, &layout));

        layoutsAndInfos_.push_back({layout, l});
    }

    // Print debug information
    printLog("ShaderManager: Created {} unique layout(s)", layoutInfos.size());
    for (size_t i = 0; i < layoutInfos.size(); ++i) {
        const auto& info = layoutInfos[i];
        const auto& layout = std::get<0>(layoutsAndInfos_[i]);
        printLog("  Layout {} (0x{:x}): {} binding(s), used by:", i,
                 reinterpret_cast<uintptr_t>(layout), info.bindings_.size());
        for (const auto& [pipelineName, setNumber] : info.pipelineNamesAndSetNumbers_) {
            printLog("    - Pipeline '{}', Set {}", pipelineName, setNumber);
        }

        // Print binding details
        for (size_t j = 0; j < info.bindings_.size(); ++j) {
            const auto& binding = info.bindings_[j];
            printLog("    Binding {}: type={}, count={}, stages={}", binding.binding,
                     descriptorTypeToString(binding.descriptorType), binding.descriptorCount,
                     shaderStageFlagsToString(binding.stageFlags));
        }
    }
}

auto DescriptorPool::descriptorSetLayout(const vector<VkDescriptorSetLayoutBinding>& bindings)
    -> const VkDescriptorSetLayout&
{
    // Search for bindings in layoutsAndInfos_
    for (const auto& [layout, layoutInfo] : layoutsAndInfos_) {
        if (BindingEqual{}(layoutInfo.bindings_,
                           bindings)) { // explicitly use operator defined in VulkanTools.h
            return layout;
        }
    }

    if (bindings.size() == 0) {
        printLog("Empty bindings provided.");
    }

    for (const auto& binding : bindings) {
        printLog("    Binding {}: type={}, count={}, stages={}", binding.binding,
                 descriptorTypeToString(binding.descriptorType), binding.descriptorCount,
                 shaderStageFlagsToString(binding.stageFlags));
    }

    exitWithMessage(
        "Failed to find descriptor set layout for the given bindings in layoutsAndInfos_");

    // This should never be reached, but needed for compiler
    static const VkDescriptorSetLayout empty = VK_NULL_HANDLE;
    return empty;
}

auto DescriptorPool::layoutToBindings(const VkDescriptorSetLayout& layout)
    -> const vector<VkDescriptorSetLayoutBinding>&
{
    // Search for layout in layoutAndInfos_
    for (const auto& [storedLayout, layoutInfo] : layoutsAndInfos_) {
        if (storedLayout == layout) {
            return layoutInfo.bindings_;
        }
    }

    printLog("Failed to find descriptor set layout in layoutAndInfos_: {}",
             reinterpret_cast<uintptr_t>(layout));

    exitWithMessage("Failed to find descriptor set layout in layoutAndInfos_");

    static vector<VkDescriptorSetLayoutBinding> empty;
    return empty;
}

void DescriptorPool::printAllocatedStatistics() const
{
    printLog("\n=== DescriptorPool Allocated Statistics ===");

    printLog("Total Pools Created: {}", descriptorPools_.size());
    printLog("Total Allocated Sets: {}", allocatedSets_);

    if (!allocatedTypeCounts_.empty()) {
        for (const auto& [type, count] : allocatedTypeCounts_) {
            printLog("  {}: {}", descriptorTypeToString(type), count);
        }
    } else {
        printLog("\nNo descriptor types allocated.");
    }

    printLog("============================================\n");
}

void DescriptorPool::cleanup()
{
    if (descriptorPools_.size() > 0) {
        std::ofstream file(kScriptFilename_);
        if (file.is_open()) {
            // Write number of sets first (no indent)
            file << "NumSets " << allocatedSets_ << "\n";

            // Write each allocated descriptor type and count (no indent)
            for (const auto& [type, count] : allocatedTypeCounts_) {
                file << descriptorTypeToString(type) << " " << count << "\n";
            }

            file.close();
            printLog("Saved descriptor pool statistics to DescriptorPoolSize.txt");
        } else {
            printLog("Warning: Could not write to DescriptorPoolSize.txt");
        }
        printAllocatedStatistics();
    }

    for (auto& p : descriptorPools_) {
        if (p != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_, p, nullptr);
        }
    }
    descriptorPools_.clear();

    // Destroy layouts in layoutAndBindings_
    for (auto& [layout, bindings] : layoutsAndInfos_) {
        if (layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device_, layout, nullptr);
        }
    }
    layoutsAndInfos_.clear();
}

} // namespace hlab