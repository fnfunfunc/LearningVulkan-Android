//
// Created by eternal on 2024/5/2.
//

#ifndef LEARNINGVULKAN_VULKANCOMMON_HH
#define LEARNINGVULKAN_VULKANCOMMON_HH

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <vector>
#include "vulkan_wrapper.hh"

namespace vulkan_common {
    VkSurfaceFormatKHR selectSurfaceFormat(VkPhysicalDevice gpu, VkSurfaceKHR surface,
                                           const std::vector<VkFormat> &preferredFormat = {
                                                   {VK_FORMAT_R8G8B8A8_SRGB,
                                                    VK_FORMAT_B8G8R8A8_SRGB,
                                                    VK_FORMAT_A8B8G8R8_SRGB_PACK32}});

    VkResult
    loadShaderFromFile(const android_app *androidAppCtx, VkDevice device, const char *filePath,
                       VkShaderModule *shaderOut);

    bool mapMemoryTypeToIndex(VkPhysicalDevice physicalDevice, uint32_t typeBits,
                              VkFlags requirementsMask, uint32_t *typeIndex);
}

#endif //LEARNINGVULKAN_VULKANCOMMON_HH
