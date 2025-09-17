#pragma once

#include "VulkanTools.h"

// Tracy Macros wrapper to handle cases where Tracy is disabled
// This header provides empty macro definitions when Tracy is not available

#ifdef TRACY_ENABLE
// If Tracy is enabled, include the actual Tracy header
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

// Map our custom macros to Tracy's actual macros
#define TRACY_CPU_SCOPE(name) ZoneScopedN(name)
#define TRACY_CPU_SCOPE_DYNAMIC(name) ZoneScopedN(name)
#define TRACY_PLOT(name, value) TracyPlot(name, value)
#define TRACY_MESSAGE(text, size) TracyMessage(text, size)
#define TRACY_MESSAGEL(text) TracyMessageL(text)
// Define TRACY_GPU_SCOPE for Tracy enabled builds
#define TRACY_GPU_SCOPE(profiler, cmd, name)                           \
    do {                                                               \
        if ((profiler).isTracySupported()) {                           \
            (profiler).beginGpuZone(cmd, name);                       \
        }                                                               \
    } while (0)

#else
// If Tracy is disabled, define empty macros to prevent compilation errors

#define TRACY_CPU_SCOPE(name)                                                                      \
    do {                                                                                           \
    } while (0)
#define TRACY_CPU_SCOPE_DYNAMIC(name)                                                              \
    do {                                                                                           \
    } while (0)
#define TRACY_GPU_SCOPE(profiler, cmd, name)                                                       \
    do {                                                                                           \
    } while (0)
#define TRACY_PLOT(name, value)                                                                    \
    do {                                                                                           \
    } while (0)
#define TRACY_MESSAGE(text, size)                                                                  \
    do {                                                                                           \
    } while (0)
#define TRACY_MESSAGEL(text)                                                                       \
    do {                                                                                           \
    } while (0)
#define FrameMark                                                                                  \
    do {                                                                                           \
    } while (0)

// Additional Tracy macros that might be used
#define TRACY_ZONE_SCOPED                                                                          \
    do {                                                                                           \
    } while (0)
#define TRACY_ZONE_SCOPED_N(name)                                                                  \
    do {                                                                                           \
    } while (0)
#define TRACY_ZONE_TEXT(text, size)                                                                \
    do {                                                                                           \
    } while (0)
#define TRACY_ZONE_VALUE(value)                                                                    \
    do {                                                                                           \
    } while (0)
#define TRACY_ZONE_COLOR(color)                                                                    \
    do {                                                                                           \
    } while (0)
#define TRACY_ALLOC(ptr, size)                                                                     \
    do {                                                                                           \
    } while (0)
#define TRACY_FREE(ptr)                                                                            \
    do {                                                                                           \
    } while (0)
#define TRACY_SECURE_ALLOC(ptr, size)                                                              \
    do {                                                                                           \
    } while (0)
#define TRACY_SECURE_FREE(ptr)                                                                     \
    do {                                                                                           \
    } while (0)

#endif

#include "Context.h"
#include <string>

namespace hlab {

class TracyProfiler
{
  public:
    TracyProfiler(Context& ctx, uint32_t maxFramesInFlight);
    ~TracyProfiler();

    // Enable move operations for flexibility (like GpuTimer)
    TracyProfiler(TracyProfiler&& other) noexcept;
    TracyProfiler(const TracyProfiler&) = delete;
    TracyProfiler& operator=(const TracyProfiler&) = delete;

    // Frame management (following GpuTimer pattern)
    void beginFrame(VkCommandBuffer cmd, uint32_t frameIndex);
    void endFrame();

    // GPU profiling zones
    void beginGpuZone(VkCommandBuffer cmd, const char* name);
    void endGpuZone(VkCommandBuffer cmd);

    // CPU profiling
    void beginCpuZone(const char* name);
    void endCpuZone();

    // Data tracking and plotting
    void plot(const char* name, float value);
    void message(const char* text);
    void messageL(const char* text);

    // Check if Tracy is available (similar to isTimestampSupported)
    bool isTracySupported() const;

    // RAII zone helpers (similar to your command buffer pattern)
    class GpuZone
    {
      public:
        GpuZone(TracyProfiler& profiler, VkCommandBuffer cmd, const char* name);
        ~GpuZone();

      private:
        TracyProfiler& profiler_;
        VkCommandBuffer cmd_;
        bool active_;
    };

    class CpuZone
    {
      public:
        explicit CpuZone(const char* name);
        ~CpuZone();

      private:
        bool active_;
        // Note: Removed tracy::ScopedZone member - use macros directly instead
    };

  private:
    Context& ctx_;
    uint32_t maxFramesInFlight_;
    bool tracySupported_;

#ifdef TRACY_ENABLE
    tracy::VkCtx* tracyContext_;
#endif

    void cleanup();
    void initializeTracy();
};

} // namespace hlab