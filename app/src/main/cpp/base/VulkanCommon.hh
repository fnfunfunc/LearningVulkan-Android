//
// Created by eternal on 2024/5/2.
//

#ifndef LEARNINGVULKAN_VULKANCOMMON_HH
#define LEARNINGVULKAN_VULKANCOMMON_HH

#include <vector>
#include "vulkan_wrapper.hh"

namespace vulkan_common {
    VkSurfaceFormatKHR selectSurfaceFormat(VkPhysicalDevice gpu, VkSurfaceKHR surface,
                                           const std::vector<VkFormat> &preferredFormat = {
                                                   {VK_FORMAT_R8G8B8A8_SRGB,
                                                    VK_FORMAT_B8G8R8A8_SRGB,
                                                    VK_FORMAT_A8B8G8R8_SRGB_PACK32}});
}

#endif //LEARNINGVULKAN_VULKANCOMMON_HH
