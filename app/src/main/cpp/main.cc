#include <android/log.h>
#include <cassert>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <vector>
#include "vulkan_wrapper.hh"


// Android log function wrappers
static const char *kTAG = "Native_LearningVulkan";
#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...) \
  ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))

// Vulkan call wrapper
#define CALL_VK(func)                                                 \
  if (VK_SUCCESS != (func)) {                                         \
    __android_log_print(ANDROID_LOG_ERROR, "Tutorial ",               \
                        "Vulkan error. File[%s], line[%d]", __FILE__, \
                        __LINE__);                                    \
    assert(false);                                                    \
  }

// Global variable
static VkInstance gVkInstance;
static VkSurfaceKHR gVkSurfaceKHR;
static VkPhysicalDevice gGpuDevice;
static VkDevice gDevice;

static bool _initialized = false;

bool initialize(android_app *pApp);

void terminate();

void handleCmd(android_app *pApp, int32_t cmd);

void android_main(android_app *pApp) {
    pApp->onAppCmd = handleCmd;

    int events;
    android_poll_source *source;
    do {
        if (ALooper_pollAll(_initialized ? 1 : 0, nullptr, &events,
                            reinterpret_cast<void **>(&source)) >= 0) {
            if (source != nullptr) {
                source->process(pApp, source);
            }
        }
    } while (pApp->destroyRequested == 0);
}

bool initialize(android_app *pApp) {
    if (!InitVulkan()) {
        LOGE("Vulkan is unavailable, install vulkan and re-start");
        return false;
    }

    VkApplicationInfo appInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = "LearningVulkan",
            .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .pEngineName = "EVulkan",
            .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .apiVersion = VK_MAKE_API_VERSION(0, 1, 1, 0),
    };

    // prepare necessary extensions: Vulkan on Android need these to function
    std::vector<const char *> instanceExt, deviceExt;
    instanceExt.push_back("VK_KHR_surface");
    instanceExt.push_back("VK_KHR_android_surface");
    deviceExt.push_back("VK_KHR_swapchain");

    VkInstanceCreateInfo instanceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<uint32_t>(instanceExt.size()),
            .ppEnabledExtensionNames = instanceExt.data()
    };
    CALL_VK(vkCreateInstance(&instanceCreateInfo, nullptr, &gVkInstance))

    VkAndroidSurfaceCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .window = pApp->window
    };
    CALL_VK(vkCreateAndroidSurfaceKHR(gVkInstance, &createInfo, nullptr, &gVkSurfaceKHR))

    // Find one GPU to use
    // On Android, every GPU device is equal -- supporting
    // graphics/compute/present
    uint32_t gpuCount = 0;
    CALL_VK(vkEnumeratePhysicalDevices(gVkInstance, &gpuCount, nullptr))
    assert(gpuCount > 0);
    std::vector<VkPhysicalDevice> tmpGpus(gpuCount);
    CALL_VK(vkEnumeratePhysicalDevices(gVkInstance, &gpuCount, tmpGpus.data()))
    gGpuDevice = tmpGpus.front();

    // Check vulkan info on this gpu device
    VkPhysicalDeviceProperties gpuProperties;
    vkGetPhysicalDeviceProperties(gGpuDevice, &gpuProperties);
    LOGI("Vulkan physical device name: %s", gpuProperties.deviceName);
    LOGI("Vulkan Physical Device Info: apiVersion: %x \n\t driverVersion: %x",
         gpuProperties.apiVersion, gpuProperties.driverVersion);
    LOGI("API Version Supported: %d.%d.%d",
         VK_VERSION_MAJOR(gpuProperties.apiVersion),
         VK_VERSION_MINOR(gpuProperties.apiVersion),
         VK_VERSION_PATCH(gpuProperties.apiVersion));

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    CALL_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gGpuDevice, gVkSurfaceKHR,
                                                      &surfaceCapabilities))
    LOGI("Vulkan Surface Capabilities:\n");
    LOGI("\timage count: %u - %u\n", surfaceCapabilities.minImageCount,
         surfaceCapabilities.maxImageCount);
    LOGI("\tarray layers: %u\n", surfaceCapabilities.maxImageArrayLayers);
    LOGI("\timage size (now): %dx%d\n", surfaceCapabilities.currentExtent.width,
         surfaceCapabilities.currentExtent.height);
    LOGI("\timage size (extent): %dx%d - %dx%d\n",
         surfaceCapabilities.minImageExtent.width,
         surfaceCapabilities.minImageExtent.height,
         surfaceCapabilities.maxImageExtent.width,
         surfaceCapabilities.maxImageExtent.height);
    LOGI("\tusage: %x\n", surfaceCapabilities.supportedUsageFlags);
    LOGI("\tcurrent transform: %u\n", surfaceCapabilities.currentTransform);
    LOGI("\tallowed transforms: %x\n", surfaceCapabilities.supportedTransforms);
    LOGI("\tcomposite alpha flags: %u\n", surfaceCapabilities.currentTransform);

    // Find a GFX queue family
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(gGpuDevice, &queueFamilyCount, nullptr);
    assert(queueFamilyCount > 0);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(gGpuDevice, &queueFamilyCount,
                                             queueFamilyProperties.data());

    const uint32_t queueFamilyIndex = std::distance(queueFamilyProperties.begin(),
                                                    std::find_if(queueFamilyProperties.begin(),
                                                                 queueFamilyProperties.end(),
                                                                 [](const VkQueueFamilyProperties &property) {
                                                                     return (property.queueFlags &
                                                                             VK_QUEUE_GRAPHICS_BIT) !=
                                                                            0;
                                                                 }));
    assert(queueFamilyIndex < queueFamilyCount);

    // Create a logical device from GPU we picked
    float priorities[] = {
            1.0f
    };
    VkDeviceQueueCreateInfo queueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = queueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = priorities,
    };
    VkDeviceCreateInfo deviceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = nullptr,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<uint32_t>(deviceExt.size()),
            .ppEnabledExtensionNames = deviceExt.data(),
            .pEnabledFeatures = nullptr,
    };

    CALL_VK(vkCreateDevice(gGpuDevice, &deviceCreateInfo, nullptr, &gDevice))
    _initialized = true;
    return true;
}

void terminate() {
    vkDestroySurfaceKHR(gVkInstance, gVkSurfaceKHR, nullptr);
    vkDestroyDevice(gDevice, nullptr);
    vkDestroyInstance(gVkInstance, nullptr);

    _initialized = false;
}

void handleCmd(android_app *pApp, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            initialize(pApp);
            break;
        case APP_CMD_TERM_WINDOW:
            terminate();
            break;
        default:
            LOGI("event not handled: %d", cmd);
    }
}