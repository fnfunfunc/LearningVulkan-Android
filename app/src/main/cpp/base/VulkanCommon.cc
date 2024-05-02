//
// Created by eternal on 2024/5/2.
//
#include <cassert>
#include <vector>
#include "VulkanCommon.hh"

namespace vulkan_common {
    VkSurfaceFormatKHR selectSurfaceFormat(VkPhysicalDevice gpu, VkSurfaceKHR surface,
                                           const std::vector<VkFormat> &preferredFormat) {
        uint32_t surfaceFormatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &surfaceFormatCount, nullptr);
        assert(surfaceFormatCount > 0);
        std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &surfaceFormatCount, surfaceFormats.data());

        const auto it = std::find_if(surfaceFormats.begin(), surfaceFormats.end(),
                                     [&preferredFormat](const VkSurfaceFormatKHR &surfaceFormat) {
                                         return std::any_of(preferredFormat.begin(),
                                                            preferredFormat.end(),
                                                            [&surfaceFormat](VkFormat format) {
                                                                return surfaceFormat.format ==
                                                                       format;
                                                            });
                                     });

        // We use the first supported format as a fallback in case none of the preferred formats is available
        return it != surfaceFormats.end() ? *it : surfaceFormats.front();
    }
}