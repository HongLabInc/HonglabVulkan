#include "GpuTimer.h"
#include "VulkanTools.h"

namespace hlab {

GpuTimer::GpuTimer(Context& ctx, uint32_t maxFramesInFlight)
    : ctx_(ctx), maxFramesInFlight_(maxFramesInFlight), timestampPeriod_(0.0f),
      timestampSupported_(false)
{
    // Get physical device properties to check timestamp support and get timestamp period
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(ctx_.physicalDevice(), &deviceProperties);

    // Check if timestamp queries are supported
    timestampSupported_ = deviceProperties.limits.timestampComputeAndGraphics == VK_TRUE;

    if (timestampSupported_) {
        // Get timestamp period (nanoseconds per tick)
        timestampPeriod_ = deviceProperties.limits.timestampPeriod;

        // Initialize storage vectors
        queryPools_.resize(maxFramesInFlight_);
        gpuTimes_.resize(maxFramesInFlight_, 0.0f);
        resultsReady_.resize(maxFramesInFlight_, false);

        // Initialize query pools
        initializeQueryPools();
    }
}

GpuTimer::~GpuTimer()
{
    cleanup();
}

GpuTimer::GpuTimer(GpuTimer&& other) noexcept
    : ctx_(other.ctx_), maxFramesInFlight_(other.maxFramesInFlight_),
      queryPools_(std::move(other.queryPools_)), gpuTimes_(std::move(other.gpuTimes_)),
      resultsReady_(std::move(other.resultsReady_)), timestampPeriod_(other.timestampPeriod_),
      timestampSupported_(other.timestampSupported_)
{
    // Reset moved-from object
    other.maxFramesInFlight_ = 0;
    other.timestampPeriod_ = 0.0f;
    other.timestampSupported_ = false;
}

GpuTimer& GpuTimer::operator=(GpuTimer&& other) noexcept
{
    if (this != &other) {
        cleanup();
        
        // Note: ctx_ is a reference and cannot be reassigned
        // This means move assignment should only be used with compatible contexts
        queryPools_ = std::move(other.queryPools_);
        gpuTimes_ = std::move(other.gpuTimes_);
        resultsReady_ = std::move(other.resultsReady_);
        maxFramesInFlight_ = other.maxFramesInFlight_;
        timestampPeriod_ = other.timestampPeriod_;
        timestampSupported_ = other.timestampSupported_;
        
        // Reset moved-from object
        other.maxFramesInFlight_ = 0;
        other.timestampPeriod_ = 0.0f;
        other.timestampSupported_ = false;
    }
    return *this;
}

void GpuTimer::initializeQueryPools()
{
    if (!timestampSupported_) {
        return;
    }

    for (uint32_t i = 0; i < maxFramesInFlight_; ++i) {
        VkQueryPoolCreateInfo queryPoolCI{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        queryPoolCI.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolCI.queryCount = 2; // Begin and end timestamps

        check(vkCreateQueryPool(ctx_.device(), &queryPoolCI, nullptr, &queryPools_[i]));
    }
}

void GpuTimer::cleanup()
{
    if (timestampSupported_) {
        for (auto queryPool : queryPools_) {
            if (queryPool != VK_NULL_HANDLE) {
                vkDestroyQueryPool(ctx_.device(), queryPool, nullptr);
            }
        }
        queryPools_.clear();
    }
}

void GpuTimer::resetQueries(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (!timestampSupported_ || frameIndex >= maxFramesInFlight_) {
        return;
    }

    vkCmdResetQueryPool(cmd, queryPools_[frameIndex], 0, 2);
    resultsReady_[frameIndex] = false;
}

void GpuTimer::beginFrame(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (!timestampSupported_ || frameIndex >= maxFramesInFlight_) {
        return;
    }

    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPools_[frameIndex], 0);
}

void GpuTimer::endFrame(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (!timestampSupported_ || frameIndex >= maxFramesInFlight_) {
        return;
    }

    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPools_[frameIndex], 1);
}

float GpuTimer::getGpuTimeMs(uint32_t frameIndex) const
{
    if (!timestampSupported_ || frameIndex >= maxFramesInFlight_) {
        return 0.0f;
    }

    if (!resultsReady_[frameIndex]) {
        uint64_t timestamps[2];
        VkResult result =
            vkGetQueryPoolResults(ctx_.device(), queryPools_[frameIndex], 0, 2, sizeof(timestamps),
                                  timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

        if (result == VK_SUCCESS) {
            uint64_t timeDiff = timestamps[1] - timestamps[0];
            // Convert from nanoseconds to milliseconds
            gpuTimes_[frameIndex] = static_cast<float>(timeDiff) * timestampPeriod_ / 1000000.0f;
            resultsReady_[frameIndex] = true;
        } else if (result == VK_NOT_READY) {
            // Results not ready yet, return previous value
            return gpuTimes_[frameIndex];
        }
    }

    return gpuTimes_[frameIndex];
}

bool GpuTimer::isResultReady(uint32_t frameIndex) const
{
    if (!timestampSupported_ || frameIndex >= maxFramesInFlight_) {
        return false;
    }

    return resultsReady_[frameIndex];
}

bool GpuTimer::isTimestampSupported() const
{
    return timestampSupported_;
}

bool GpuTimer::hasAnyResultsReady() const
{
    if (!timestampSupported_) {
        return false;
    }
    
    for (uint32_t i = 0; i < maxFramesInFlight_; ++i) {
        if (isResultReady(i)) {
            return true;
        }
    }
    return false;
}

} // namespace hlab