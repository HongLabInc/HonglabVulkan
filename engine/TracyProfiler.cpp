#include "TracyProfiler.h"
#include "VulkanTools.h"
#include <cstring> // for strlen

namespace hlab {

TracyProfiler::TracyProfiler(Context& ctx, uint32_t maxFramesInFlight)
    : ctx_(ctx), maxFramesInFlight_(maxFramesInFlight), tracySupported_(false)
#ifdef TRACY_ENABLE
      ,
      tracyContext_(nullptr)
#endif
{
#ifdef TRACY_ENABLE
    initializeTracy();
    printLog("Tracy is ENABLED - Attempting to start profiler and web server");
#else
    tracySupported_ = false;
    printLog("TracyProfiler created but Tracy is DISABLED at compile time");
#endif
}

TracyProfiler::~TracyProfiler()
{
    cleanup();
}

TracyProfiler::TracyProfiler(TracyProfiler&& other) noexcept
    : ctx_(other.ctx_), maxFramesInFlight_(other.maxFramesInFlight_),
      tracySupported_(other.tracySupported_)
#ifdef TRACY_ENABLE
      ,
      tracyContext_(other.tracyContext_)
#endif
{
#ifdef TRACY_ENABLE
    other.tracyContext_ = nullptr;
#endif
    other.tracySupported_ = false;
    other.maxFramesInFlight_ = 0;
}

void TracyProfiler::initializeTracy()
{
#ifdef TRACY_ENABLE
    try {
        printLog("Initializing Tracy profiler...");

        // Send initial message to trigger Tracy startup
        TracyMessageL("Tracy profiler initializing...");

        // Create a temporary command buffer for Tracy initialization.
        // TracyVkContext will use, submit, and wait for this command buffer.
        auto cmd = ctx_.createGraphicsCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);

        // Initialize Tracy Vulkan context. This is a synchronous operation.
        tracyContext_ = TracyVkContext(ctx_.physicalDevice(), ctx_.device(), ctx_.graphicsQueue(),
                                       cmd.handle());

        // The command buffer is now used and can be destroyed.
        // The destructor of 'cmd' will handle cleanup.

        if (tracyContext_) {
            tracySupported_ = true;
            printLog("✓ Tracy Vulkan context created successfully");
            printLog("✓ Tracy profiler is now active and collecting data");
            printLog("→ Tracy server should be starting on port 8086");
            printLog("→ Connect Tracy GUI client to view profiling data");
            printLog("→ Download Tracy from: https://github.com/wolfpld/tracy/releases");

            // Send startup confirmation message
            TracyMessageL("Tracy Vulkan profiler ready - GPU timing active");

        } else {
            tracySupported_ = false;
            printLog("✗ Failed to create Tracy Vulkan context");
        }

        // Force a frame mark to start the profiler
        FrameMark;

    } catch (const std::exception& e) {
        tracySupported_ = false;
        printLog("✗ Tracy initialization failed: {}", e.what());
    } catch (...) {
        tracySupported_ = false;
        printLog("✗ Tracy initialization failed with unknown error");
    }
#else
    tracySupported_ = false;
    printLog("Tracy is disabled at compile time - no initialization");
#endif
}

void TracyProfiler::cleanup()
{
#ifdef TRACY_ENABLE
    if (tracyContext_) {
        printLog("Cleaning up Tracy context...");
        TracyVkDestroy(tracyContext_);
        tracyContext_ = nullptr;
    }
#endif
    tracySupported_ = false;
}

void TracyProfiler::beginFrame(VkCommandBuffer cmd, uint32_t frameIndex)
{
#ifdef TRACY_ENABLE
    if (tracySupported_ && tracyContext_) {
        TracyVkCollect(tracyContext_, cmd);
    }
#endif
}

void TracyProfiler::endFrame()
{
#ifdef TRACY_ENABLE
    if (tracySupported_) {
        FrameMark;
    }
#endif
}

void TracyProfiler::beginGpuZone(VkCommandBuffer cmd, const char* name)
{
#ifdef TRACY_ENABLE
    if (tracySupported_ && tracyContext_) {
        // For manual GPU zone creation, we need to use the underlying Tracy API
        // TracyVkZone macro is meant for automatic scoped zones
        // We'll implement manual zone tracking if needed
    }
#endif
}

void TracyProfiler::endGpuZone(VkCommandBuffer cmd)
{
#ifdef TRACY_ENABLE
    // Note: TracyVkZone is a scope-based macro, so manual end is not needed
    // This function is kept for API consistency but does nothing
#endif
}

void TracyProfiler::beginCpuZone(const char* name)
{
#ifdef TRACY_ENABLE
    // CPU zones are handled by macros, not this function
#endif
}

void TracyProfiler::endCpuZone()
{
#ifdef TRACY_ENABLE
    // CPU zones are handled by macros, not this function
#endif
}

void TracyProfiler::plot(const char* name, float value)
{
#ifdef TRACY_ENABLE
    if (tracySupported_) {
        TracyPlot(name, value);
    }
#endif
}

void TracyProfiler::message(const char* text)
{
#ifdef TRACY_ENABLE
    if (tracySupported_) {
        TracyMessage(text, std::strlen(text));
    }
#endif
}

void TracyProfiler::messageL(const char* text)
{
#ifdef TRACY_ENABLE
    if (tracySupported_) {
        TracyMessageL(text);
    }
#endif
}

bool TracyProfiler::isTracySupported() const
{
    return tracySupported_;
}

// RAII GpuZone implementation
TracyProfiler::GpuZone::GpuZone(TracyProfiler& profiler, VkCommandBuffer cmd, const char* name)
    : profiler_(profiler), cmd_(cmd), active_(false)
{
#ifdef TRACY_ENABLE
    if (profiler_.isTracySupported() && profiler_.tracyContext_) {
        // Tracy GPU zones are handled by the TracyVkZone macro automatically
        // This RAII wrapper just ensures proper scoping
        active_ = true;
    }
#endif
}

TracyProfiler::GpuZone::~GpuZone()
{
#ifdef TRACY_ENABLE
    if (active_) {
        // TracyVkZone automatically handles cleanup when scope ends
        // No manual cleanup needed
    }
#endif
}

// RAII CpuZone implementation
TracyProfiler::CpuZone::CpuZone(const char* name) : active_(false)
{
#ifdef TRACY_ENABLE
    // CPU zones use Tracy macros directly
    active_ = true;
#endif
}

TracyProfiler::CpuZone::~CpuZone()
{
#ifdef TRACY_ENABLE
    // CPU zones are automatically handled by Tracy macros
#endif
}

} // namespace hlab