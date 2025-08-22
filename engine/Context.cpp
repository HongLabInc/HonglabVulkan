#include "Context.h"
#include "Logger.h"
#include <cassert>
#include <algorithm>
#include <set>
#include <unordered_map>

namespace hlab {

using namespace std;

PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT{nullptr};
PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT{nullptr};
PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT{nullptr};

PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
VkDebugUtilsMessengerEXT debugUtilsMessenger;

VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessageCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    std::stringstream debugMessage;
    if (pCallbackData->pMessageIdName) {
        debugMessage << "[" << pCallbackData->messageIdNumber << "]["
                     << pCallbackData->pMessageIdName << "] : " << pCallbackData->pMessage;
    } else {
        debugMessage << "[" << pCallbackData->messageIdNumber << "] : " << pCallbackData->pMessage;
    }

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        printLog("[VERBOSE] {}", debugMessage.str());
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        printLog("[INFO] {}", debugMessage.str());
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        printLog("[WARNING] {}", debugMessage.str());
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        exitWithMessage("[ERROR] {}", debugMessage.str());
    }

    return VK_FALSE;
}

Context::Context(const vector<const char*>& requiredInstanceExtensions, bool useSwapchain)
    : descriptorPool_(device_)
{
    createInstance(requiredInstanceExtensions);
    selectPhysicalDevice();
    createLogicalDevice(useSwapchain);
    createQueues();
    createPipelineCache();
    determineDepthStencilFormat();
    descriptorPool_.createFromScript();
}

uint32_t Context::getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) const
{
    for (uint32_t i = 0; i < deviceMemoryProperties_.memoryTypeCount; i++) {
        if ((typeBits & 1) == 1) {
            if ((deviceMemoryProperties_.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        typeBits >>= 1;
    }

    exitWithMessage("Could not find a suitable memory type.");
    return uint32_t(-1);
}

[[nodiscard]]
string getPhysicalDeviceTypeString(VkPhysicalDeviceType type)
{
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        return "Other";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "Integrated GPU";
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "Discrete GPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return "Virtual GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return "CPU";
    default:
        return "Unknown";
    }
}

void Context::selectPhysicalDevice()
{
    uint32_t gpuCount = 0;
    check(vkEnumeratePhysicalDevices(instance_, &gpuCount, nullptr));
    if (gpuCount == 0) {
        exitWithMessage("gpuCount is 0");
    }

    vector<VkPhysicalDevice> physicalDevices(gpuCount);
    check(vkEnumeratePhysicalDevices(instance_, &gpuCount, physicalDevices.data()));

    printLog("\nAvailable physical devices: {}", gpuCount);
    for (size_t i = 0; i < gpuCount; ++i) {
        VkPhysicalDeviceProperties deviceProperties_;
        vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties_);
        printLog("  {} {} ({})", i, deviceProperties_.deviceName,
                 getPhysicalDeviceTypeString(deviceProperties_.deviceType));
    }

    uint32_t selectedDevice = 0;
    physicalDevice_ = physicalDevices[selectedDevice];

    vkGetPhysicalDeviceProperties(physicalDevice_, &deviceProperties_);

    printLog("Selected {} ({})", deviceProperties_.deviceName,
             getPhysicalDeviceTypeString(deviceProperties_.deviceType));
    printLog("  nonCoherentAtomSize: {}", deviceProperties_.limits.nonCoherentAtomSize);
    printLog("  Max UBO size: {} KBytes", deviceProperties_.limits.maxUniformBufferRange / 1024);
    printLog("  Max SSBO size: {} KBytes", deviceProperties_.limits.maxStorageBufferRange / 1024);
    printLog("  UBO offset alignment: {}",
             deviceProperties_.limits.minUniformBufferOffsetAlignment);
    printLog("  SSBO offset alignment: {}",
             deviceProperties_.limits.minStorageBufferOffsetAlignment);

    vkGetPhysicalDeviceFeatures(physicalDevice_, &deviceFeatures_);

    printLog("\nDevice Features:");
    printLog("  geometryShader: {}", deviceFeatures_.geometryShader ? "YES" : "NO");
    printLog("  tessellationShader: {}", deviceFeatures_.tessellationShader ? "YES" : "NO");
    // ... etc ...

    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &deviceMemoryProperties_);

    // Print device memory properties
    printLog("\nDevice Memory Properties:");
    printLog("  Memory Type Count: {}", deviceMemoryProperties_.memoryTypeCount);
    for (uint32_t i = 0; i < deviceMemoryProperties_.memoryTypeCount; ++i) {
        const auto& memType = deviceMemoryProperties_.memoryTypes[i];
        string propFlags;
        if (memType.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            propFlags += "DEVICE_LOCAL ";
        if (memType.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            propFlags += "HOST_VISIBLE ";
        if (memType.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
            propFlags += "HOST_COHERENT ";
        if (memType.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
            propFlags += "HOST_CACHED ";
        if (memType.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
            propFlags += "LAZILY_ALLOCATED ";
        if (memType.propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT)
            propFlags += "PROTECTED ";
        if (propFlags.empty())
            propFlags = "NONE ";

        printLog("    Memory Type {}: heap {}, flags: {}", i, memType.heapIndex, propFlags);
    }

    printLog("  Memory Heap Count: {}", deviceMemoryProperties_.memoryHeapCount);
    for (uint32_t i = 0; i < deviceMemoryProperties_.memoryHeapCount; ++i) {
        const auto& heap = deviceMemoryProperties_.memoryHeaps[i];
        string heapFlags;
        if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            heapFlags += "DEVICE_LOCAL ";
        if (heap.flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT)
            heapFlags += "MULTI_INSTANCE ";
        if (heapFlags.empty())
            heapFlags = "NONE ";

        printLog("    Memory Heap {}: {} MB, flags: {}", i, heap.size / (1024 * 1024), heapFlags);
    }

    // Find queue family properties
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, nullptr);
    assert(queueFamilyCount > 0);
    queueFamilyProperties_.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount,
                                             queueFamilyProperties_.data());

    printLog("\nQueue Family Properties: {}", queueFamilyCount);
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        const auto& props = queueFamilyProperties_[i];

        string queueFlagsStr;
        if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            queueFlagsStr += "GRAPHICS ";
        if (props.queueFlags & VK_QUEUE_COMPUTE_BIT)
            queueFlagsStr += "COMPUTE ";
        if (props.queueFlags & VK_QUEUE_TRANSFER_BIT)
            queueFlagsStr += "TRANSFER ";
        if (props.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
            queueFlagsStr += "SPARSE_BINDING ";
        if (props.queueFlags & VK_QUEUE_PROTECTED_BIT)
            queueFlagsStr += "PROTECTED ";
        if (props.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR)
            queueFlagsStr += "VIDEO_DECODE ";
        if (props.queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR)
            queueFlagsStr += "VIDEO_ENCODE ";
        if (props.queueFlags & VK_QUEUE_OPTICAL_FLOW_BIT_NV)
            queueFlagsStr += "OPTICAL_FLOW ";
        if (props.queueFlags & VK_QUEUE_DATA_GRAPH_BIT_ARM)
            queueFlagsStr += "DATA_GRAPH ";

        printLog("  Queue Family {}: {} queues, flags: {}", i, props.queueCount, queueFlagsStr);
    }

    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &extCount, nullptr);
    if (extCount > 0) {
        vector<VkExtensionProperties> extensions(extCount);
        if (vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &extCount,
                                                 &extensions.front()) == VK_SUCCESS) {
            for (auto& ext : extensions) {
                supportedExtensions_.push_back(ext.extensionName);
            }
        }
    }
}

VkSampleCountFlagBits Context::getMaxUsableSampleCount()
{
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice(), &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
                                physicalDeviceProperties.limits.framebufferDepthSampleCounts;

    if (counts & VK_SAMPLE_COUNT_64_BIT) {
        return VK_SAMPLE_COUNT_64_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_32_BIT) {
        return VK_SAMPLE_COUNT_32_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_16_BIT) {
        return VK_SAMPLE_COUNT_16_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_8_BIT) {
        return VK_SAMPLE_COUNT_8_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_4_BIT) {
        return VK_SAMPLE_COUNT_4_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_2_BIT) {
        return VK_SAMPLE_COUNT_2_BIT;
    }

    return VK_SAMPLE_COUNT_1_BIT;
}

void Context::createPipelineCache()
{
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    check(vkCreatePipelineCache(device_, &pipelineCacheCreateInfo, nullptr, &pipelineCache_));
}

void Context::createInstance(vector<const char*> requiredInstanceExtensions)
{
#ifdef NDEBUG
    bool useValidation = false; // Release build
#else
    bool useValidation = true; // Debug build
#endif

    const uint32_t apiVersion = VK_API_VERSION_1_3;
    const string name = "Vulkan Examples";

    vector<string> supportedInstanceExtensions;
    uint32_t extCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    if (extCount > 0) {
        vector<VkExtensionProperties> extensions(extCount);
        if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front()) ==
            VK_SUCCESS) {
            for (VkExtensionProperties& extension : extensions) {
                supportedInstanceExtensions.push_back(extension.extensionName);
            }
        }
    }

    // print instanceExtensions
    printLog("Supported Instance Extensions:");
    for (const string& extension : supportedInstanceExtensions) {
        printLog("  {}", extension);
    }

    // MoltenVK on macOS/iOS supported
    const char* portabilityExtension = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
    bool portabilityAlreadyAdded = false;
    for (const char* ext : requiredInstanceExtensions) {
        if (strcmp(ext, portabilityExtension) == 0) {
            portabilityAlreadyAdded = true;
            break;
        }
    }

    // Add VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME, "VK_KHR_xlib_surface" and
    // "VK_KHR_wayland_surface" for MacOS, Linux support if they are available in
    // supportedInstanceExtensions

    bool portabilitySupported =
        find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(),
             portabilityExtension) != supportedInstanceExtensions.end();

    if (!portabilityAlreadyAdded && portabilitySupported) {
        requiredInstanceExtensions.push_back(portabilityExtension);
        portabilityAlreadyAdded = true;
    }

    // Validate all required extensions are supported
    for (const char* requiredExtension : requiredInstanceExtensions) {
        if (find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(),
                 requiredExtension) == supportedInstanceExtensions.end()) {
            exitWithMessage("Required instance extension \"{}\" is not supported",
                            requiredExtension);
        }
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = name.c_str();
    appInfo.pEngineName = name.c_str();
    appInfo.apiVersion = apiVersion;

    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    if (portabilityAlreadyAdded) {
        instanceCreateInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        // Also need to enable the extension in ppEnabledExtensionNames
    }

    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
    if (useValidation) {
        debugUtilsMessengerCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugUtilsMessengerCI
            .messageSeverity = // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               //  VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                               //  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugUtilsMessengerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        // VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugUtilsMessengerCI.pfnUserCallback = debugUtilsMessageCallback;
        debugUtilsMessengerCI.pNext = instanceCreateInfo.pNext;
        instanceCreateInfo.pNext = &debugUtilsMessengerCI;

        const char* debugExtension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        if (find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(),
                 debugExtension) != supportedInstanceExtensions.end()) {
            requiredInstanceExtensions.push_back(debugExtension);
        } else {
            printLog("Debug utils extension not supported, debug features will be limited");
        }
    }

    // print instanceExtensions
    printLog("Required Instance Extensions:");
    for (const char* extension : requiredInstanceExtensions) {
        printLog("  {}", extension);
    }

    if (!requiredInstanceExtensions.empty()) {
        instanceCreateInfo.enabledExtensionCount = (uint32_t)requiredInstanceExtensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = requiredInstanceExtensions.data();
    }

    uint32_t instanceLayerCount;
    vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
    vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
    vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());

    // Sort layerProperties by layerName
    sort(instanceLayerProperties.begin(), instanceLayerProperties.end(),
         [](const VkLayerProperties& a, const VkLayerProperties& b) {
             return strcmp(a.layerName, b.layerName) < 0;
         });

    printLog("Available instance layers:");
    for (const VkLayerProperties& props : instanceLayerProperties) {
        printLog("  {}", props.layerName);
    }

    if (useValidation) {
        const char* validationLayerName = "VK_LAYER_KHRONOS_validation";

        // Validation layer check
        bool validationLayerPresent = false;
        for (VkLayerProperties& layer : instanceLayerProperties) {
            if (strcmp(layer.layerName, validationLayerName) == 0) {
                validationLayerPresent = true;
                break;
            }
        }

        if (validationLayerPresent) {
            instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
            instanceCreateInfo.enabledLayerCount = 1;
        } else {
            exitWithMessage("Validation layer VK_LAYER_KHRONOS_validation not present");
        }
    }

    check(vkCreateInstance(&instanceCreateInfo, nullptr, &instance_));

    if (find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(),
             VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != supportedInstanceExtensions.end()) {
        vkCmdBeginDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance_, "vkCmdBeginDebugUtilsLabelEXT"));
        vkCmdEndDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance_, "vkCmdEndDebugUtilsLabelEXT"));
        vkCmdInsertDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance_, "vkCmdInsertDebugUtilsLabelEXT"));
    }

    if (useValidation) {
        vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
        vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));

        VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
        debugUtilsMessengerCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugUtilsMessengerCI.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugUtilsMessengerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debugUtilsMessengerCI.pfnUserCallback = debugUtilsMessageCallback;
        check(vkCreateDebugUtilsMessengerEXT(instance_, &debugUtilsMessengerCI, nullptr,
                                             &debugUtilsMessenger));
    }
}

void Context::createLogicalDevice(bool useSwapChain)
{
    const VkQueueFlags requestedQueueTypes = VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT;

    VkPhysicalDeviceVulkan13Features enabledFeatures13{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    enabledFeatures13.dynamicRendering = VK_TRUE;
    enabledFeatures13.synchronization2 = VK_TRUE;

    vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

    const float defaultQueuePriority(0.0f);

    if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT) {
        queueFamilyIndices_.graphics = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamilyIndices_.graphics;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &defaultQueuePriority;
        queueCreateInfos.push_back(queueInfo);
    } else {
        queueFamilyIndices_.graphics = 0;
    }

    if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT) {
        queueFamilyIndices_.compute = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
        if (queueFamilyIndices_.compute != queueFamilyIndices_.graphics) {

            VkDeviceQueueCreateInfo queueInfo{};
            queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = queueFamilyIndices_.compute;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &defaultQueuePriority;
            queueCreateInfos.push_back(queueInfo);
        }
    } else {

        queueFamilyIndices_.compute = queueFamilyIndices_.graphics;
    }

    if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT) {
        queueFamilyIndices_.transfer = getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
        if ((queueFamilyIndices_.transfer != queueFamilyIndices_.graphics) &&
            (queueFamilyIndices_.transfer != queueFamilyIndices_.compute)) {

            VkDeviceQueueCreateInfo queueInfo{};
            queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = queueFamilyIndices_.transfer;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &defaultQueuePriority;
            queueCreateInfos.push_back(queueInfo);
        }
    } else {
        queueFamilyIndices_.transfer = queueFamilyIndices_.graphics;
    }

    vector<const char*> deviceExtensions(enabledDeviceExtensions_);
    if (useSwapChain) {
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    enabledFeatures_.samplerAnisotropy = deviceFeatures_.samplerAnisotropy;
    enabledFeatures_.depthClamp = deviceFeatures_.depthClamp;
    enabledFeatures_.depthBiasClamp = deviceFeatures_.depthBiasClamp;

    VkDeviceCreateInfo deviceCreateInfo = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = &enabledFeatures_;

    VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
    physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physicalDeviceFeatures2.features = enabledFeatures_;
    physicalDeviceFeatures2.pNext = &enabledFeatures13;
    deviceCreateInfo.pEnabledFeatures = nullptr;
    deviceCreateInfo.pNext = &physicalDeviceFeatures2;

    if (deviceExtensions.size() > 0) {
        for (const char* enabledExtension : deviceExtensions) {
            if (!extensionSupported(enabledExtension)) {
                printLog("Enabled device extension \"{}\" is not present at device level",
                         enabledExtension);
            }
        }

        deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
        deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    }

    check(vkCreateDevice(physicalDevice_, &deviceCreateInfo, nullptr, &device_));

    graphicsCommandPool_ = createCommandPool(queueFamilyIndices_.graphics);
    if (queueFamilyIndices_.compute != queueFamilyIndices_.graphics) {
        computeCommandPool_ = createCommandPool(queueFamilyIndices_.compute);
    } else {
        computeCommandPool_ = graphicsCommandPool_;
    }
    if (queueFamilyIndices_.transfer != queueFamilyIndices_.graphics &&
        queueFamilyIndices_.transfer != queueFamilyIndices_.compute) {
        transferCommandPool_ = createCommandPool(queueFamilyIndices_.transfer);
    } else if (queueFamilyIndices_.transfer == queueFamilyIndices_.compute) {
        transferCommandPool_ = computeCommandPool_;
    } else {
        transferCommandPool_ = graphicsCommandPool_;
    }
}

uint32_t Context::getQueueFamilyIndex(VkQueueFlags queueFlags) const
{
    if ((queueFlags & VK_QUEUE_COMPUTE_BIT) == queueFlags) {
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties_.size()); i++) {
            if ((queueFamilyProperties_[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
                ((queueFamilyProperties_[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)) {
                return i;
            }
        }
    }

    if ((queueFlags & VK_QUEUE_TRANSFER_BIT) == queueFlags) {
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties_.size()); i++) {
            if ((queueFamilyProperties_[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                ((queueFamilyProperties_[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) &&
                ((queueFamilyProperties_[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)) {
                return i;
            }
        }
    }

    for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties_.size()); i++) {
        if ((queueFamilyProperties_[i].queueFlags & queueFlags) == queueFlags) {
            return i;
        }
    }

    exitWithMessage("Could not find a queue family that supports the requested queue flags: {}",
                    to_string(queueFlags));

    return uint32_t(-1); // To avoid compiler warnings
}

bool Context::extensionSupported(string extension)
{
    return (find(supportedExtensions_.begin(), supportedExtensions_.end(), extension) !=
            supportedExtensions_.end());
}

[[nodiscard]]
VkCommandPool Context::createCommandPool(uint32_t queueFamilyIndex,
                                         VkCommandPoolCreateFlags createFlags)
{
    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
    cmdPoolInfo.flags = createFlags;

    VkCommandPool cmdPool;
    check(vkCreateCommandPool(device_, &cmdPoolInfo, nullptr, &cmdPool));
    return cmdPool;
}

VkPipelineCache Context::pipelineCache() const
{
    return pipelineCache_;
}

VkQueue Context::graphicsQueue() const
{
    return graphicsQueue_;
}

VkQueue Context::computeQueue() const
{
    return computeQueue_;
}

VkQueue Context::transferQueue() const
{
    return transferQueue_;
}

string Context::deviceName() const
{
    return string(deviceProperties_.deviceName);
}

VkCommandPool Context::graphicsCommandPool() const
{
    return graphicsCommandPool_;
}

VkCommandPool Context::computeCommandPool() const
{
    return computeCommandPool_;
}

VkCommandPool Context::transferCommandPool() const
{
    return transferCommandPool_;
}

[[nodiscard]]
auto Context::createGraphicsCommandBuffers(uint32_t numBuffers) -> vector<CommandBuffer>
{
    const VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    vector<VkCommandBuffer> handles(numBuffers);

    VkCommandBufferAllocateInfo cmdBufAllocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdBufAllocInfo.commandPool = graphicsCommandPool_;
    cmdBufAllocInfo.level = level;
    cmdBufAllocInfo.commandBufferCount = numBuffers;

    check(vkAllocateCommandBuffers(device_, &cmdBufAllocInfo, handles.data()));

    vector<CommandBuffer> commandBuffers;
    for (auto& handle : handles)
        commandBuffers.emplace_back(device_, handle, graphicsCommandPool_, graphicsQueue_, level);

    return commandBuffers;
}

[[nodiscard]]
CommandBuffer Context::createGraphicsCommandBuffer(VkCommandBufferLevel level, bool begin)
{
    return CommandBuffer(device_, graphicsCommandPool_, graphicsQueue_, level, begin);
}

[[nodiscard]]
CommandBuffer Context::createComputeCommandBuffer(VkCommandBufferLevel level, bool begin)
{
    return CommandBuffer(device_, computeCommandPool_, computeQueue_, level, begin);
}

[[nodiscard]]
CommandBuffer Context::createTransferCommandBuffer(VkCommandBufferLevel level, bool begin)
{
    return CommandBuffer(device_, transferCommandPool_, transferQueue_, level, begin);
}

Context::~Context()
{
    cleanup();
}

VkDevice Context::device()
{
    return device_;
}

VkInstance Context::instance()
{
    return instance_;
}

VkPhysicalDevice Context::physicalDevice()
{
    return physicalDevice_;
}

void Context::waitIdle()
{
    vkDeviceWaitIdle(device_);
}

void Context::waitGraphicsQueueIdle() const
{
    vkQueueWaitIdle(graphicsQueue());
}

VkFormat Context::depthFormat() const
{
    assert(depthFormat_ != VK_FORMAT_UNDEFINED);

    return depthFormat_;
}

void Context::determineDepthStencilFormat()
{
    std::vector<VkFormat> formatList = {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT,
    };

    for (auto& format : formatList) {
        VkFormatProperties formatProps;
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &formatProps);
        if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            depthFormat_ = format;
            break;
        }
    }

    assert(depthFormat_ != VK_FORMAT_UNDEFINED);
}

void Context::cleanup()
{
    descriptorPool_.cleanup();

    // Wait for all operations to complete before cleanup
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }

    if (pipelineCache_ != VK_NULL_HANDLE) {
        vkDestroyPipelineCache(device_, pipelineCache_, nullptr);
        pipelineCache_ = VK_NULL_HANDLE;
    }

    std::set<VkCommandPool> uniquePools;
    if (graphicsCommandPool_ != VK_NULL_HANDLE) {
        uniquePools.insert(graphicsCommandPool_);
    }
    if (computeCommandPool_ != VK_NULL_HANDLE) {
        uniquePools.insert(computeCommandPool_);
    }
    if (transferCommandPool_ != VK_NULL_HANDLE) {
        uniquePools.insert(transferCommandPool_);
    }

    for (VkCommandPool pool : uniquePools) {
        vkDestroyCommandPool(device_, pool, nullptr);
    }

    // Reset all pool handles
    graphicsCommandPool_ = VK_NULL_HANDLE;
    computeCommandPool_ = VK_NULL_HANDLE;
    transferCommandPool_ = VK_NULL_HANDLE;

    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (debugUtilsMessenger != VK_NULL_HANDLE && vkDestroyDebugUtilsMessengerEXT) {
        vkDestroyDebugUtilsMessengerEXT(instance_, debugUtilsMessenger, nullptr);
        debugUtilsMessenger = VK_NULL_HANDLE;
    }

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

void Context::createQueues()
{
    // Validate queue family indices before getting queues
    if (queueFamilyIndices_.graphics == uint32_t(-1)) {
        exitWithMessage("Graphics queue family index is invalid");
    }

    vkGetDeviceQueue(device_, queueFamilyIndices_.graphics, 0, &graphicsQueue_);

    // For compute: either get from dedicated family or use graphics queue
    if (queueFamilyIndices_.compute != queueFamilyIndices_.graphics &&
        queueFamilyIndices_.compute != uint32_t(-1)) {
        vkGetDeviceQueue(device_, queueFamilyIndices_.compute, 0, &computeQueue_);
    } else {
        computeQueue_ = graphicsQueue_;
    }

    // For transfer: either get from dedicated family or use appropriate fallback
    if (queueFamilyIndices_.transfer != queueFamilyIndices_.graphics &&
        queueFamilyIndices_.transfer != queueFamilyIndices_.compute &&
        queueFamilyIndices_.transfer != uint32_t(-1)) {
        vkGetDeviceQueue(device_, queueFamilyIndices_.transfer, 0, &transferQueue_);
    } else if (queueFamilyIndices_.transfer == queueFamilyIndices_.compute) {
        transferQueue_ = computeQueue_;
    } else {
        transferQueue_ = graphicsQueue_;
    }

    // Validate all queues were obtained
    if (graphicsQueue_ == VK_NULL_HANDLE) {
        exitWithMessage("Failed to get graphics queue");
    }
    if (computeQueue_ == VK_NULL_HANDLE) {
        exitWithMessage("Failed to get compute queue");
    }
    if (transferQueue_ == VK_NULL_HANDLE) {
        exitWithMessage("Failed to get transfer queue");
    }
}

} // namespace hlab