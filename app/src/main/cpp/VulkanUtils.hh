//
// Created by eternal on 2024/4/29.
//

#ifndef LEARNINGVULKAN_VULKANUTILS_HH
#define LEARNINGVULKAN_VULKANUTILS_HH

#include <cstdint>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include "vulkan_wrapper.hh"

namespace vulkan_utils {
    VkResult
    loadShaderFromFile(const android_app *androidAppCtx, VkDevice device, const char *filePath,
                       VkShaderModule *shaderOut);

    bool mapMemoryTypeToIndex(VkPhysicalDevice physicalDevice, uint32_t typeBits,
                              VkFlags requirementsMask, uint32_t *typeIndex);

    void
    setImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldImageLayout,
                   VkImageLayout newImageLayout, VkPipelineStageFlags srcStages,
                   VkPipelineStageFlags dstStages);
}

#endif //LEARNINGVULKAN_VULKANUTILS_HH
