#include "engine/TracyProfiler.h"
#include "engine/Context.h"
#include "engine/Logger.h"
#include <thread>
#include <chrono>

using namespace hlab;

int main() {
    printLog("=== Tracy Test Application ===");
    
    try {
        // Initialize Vulkan context (minimal setup)
        Context ctx({}, false);
        
        // Create Tracy profiler
        TracyProfiler profiler(ctx, 1);
        
        printLog("Tracy profiler initialized. Status: {}", 
                 profiler.isTracySupported() ? "SUPPORTED" : "NOT SUPPORTED");
        
        if (profiler.isTracySupported()) {
            printLog("? Tracy is active!");
            printLog("? Tracy server should be running on localhost:8086");
            printLog("? You can connect with Tracy GUI client");
            
            // Send some test data
            for (int i = 0; i < 10; ++i) {
                TRACY_CPU_SCOPE("Test Loop Iteration");
                
                profiler.messageL("Test iteration starting");
                profiler.plot("Test Counter", static_cast<float>(i));
                
                // Simulate some work
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // Force frame mark
                profiler.endFrame();
                
                printLog("Sent test data point {}", i);
            }
            
            printLog("? Test data sent to Tracy");
            printLog("? Check Tracy GUI to see if data is being received");
            
        } else {
            printLog("? Tracy is not supported - check compilation flags");
        }
        
        printLog("Test completed. Keep this program running to maintain Tracy connection.");
        printLog("Press Ctrl+C to exit...");
        
        // Keep the program running to maintain Tracy connection
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            profiler.endFrame();
        }
        
    } catch (const std::exception& e) {
        printLog("Error: {}", e.what());
        return 1;
    }
    
    return 0;
}