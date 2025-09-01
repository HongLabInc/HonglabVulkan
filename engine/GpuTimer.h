#pragma once

#include "Context.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace hlab {

class GpuTimer
{
  public:
    GpuTimer(Context& ctx, uint32_t maxFramesInFlight);
    ~GpuTimer();

    // Disable copy operations to avoid double-cleanup of Vulkan resources
    GpuTimer(const GpuTimer&) = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;

    // Enable move operations for flexibility
    GpuTimer(GpuTimer&& other) noexcept;
    GpuTimer& operator=(GpuTimer&& other) noexcept;

    // Begin/end timing for a frame
    void beginFrame(VkCommandBuffer cmd, uint32_t frameIndex);
    void endFrame(VkCommandBuffer cmd, uint32_t frameIndex);

    // Get latest GPU time in milliseconds
    float getGpuTimeMs(uint32_t frameIndex) const;
    bool isResultReady(uint32_t frameIndex) const;

    // Reset queries for new frame (must be called before beginFrame)
    void resetQueries(VkCommandBuffer cmd, uint32_t frameIndex);

    // Check if timestamp queries are supported
    bool isTimestampSupported() const;

    // Check if any frame has results ready
    bool hasAnyResultsReady() const;

  private:
    Context& ctx_; // Changed from Context* to Context&
    uint32_t maxFramesInFlight_;

    // Query pools for timestamp queries (one per frame in flight)
    std::vector<VkQueryPool> queryPools_;

    // Cached results in milliseconds
    mutable std::vector<float> gpuTimes_;
    mutable std::vector<bool> resultsReady_;

    // GPU timestamp frequency for time conversion (nanoseconds per tick)
    float timestampPeriod_;

    // Timestamp support flag
    bool timestampSupported_;

    // Helper methods
    void cleanup();
    void initializeQueryPools();
};

} // namespace hlab