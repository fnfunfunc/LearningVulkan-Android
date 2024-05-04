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

    VkResult
    loadShaderFromFile(const android_app *androidAppCtx, const VkDevice device,
                       const char *filePath,
                       VkShaderModule *shaderOut) {
        assert(androidAppCtx != nullptr);
        AAsset *assetFile = AAssetManager_open(androidAppCtx->activity->assetManager, filePath,
                                               AASSET_MODE_BUFFER);
        const size_t fileLength = AAsset_getLength(assetFile);
        char *shaderCode = new char[fileLength];

        AAsset_read(assetFile, shaderCode, fileLength);
        AAsset_close(assetFile);

        VkShaderModuleCreateInfo shaderModuleCreateInfo{
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .codeSize = fileLength,
                .pCode = reinterpret_cast<const uint32_t *>(shaderCode)
        };

        VkResult result = vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, shaderOut);

        delete[] shaderCode;

        return result;
    }

    bool mapMemoryTypeToIndex(VkPhysicalDevice physicalDevice, uint32_t typeBits,
                              VkFlags requirementsMask, uint32_t *typeIndex) {
        VkPhysicalDeviceMemoryProperties memoryProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

        for (uint32_t i = 0; i < 32; ++i) {
            if ((typeBits & 1) == 1) {
                if ((memoryProperties.memoryTypes[i].propertyFlags & requirementsMask) ==
                    requirementsMask) {
                    *typeIndex = i;
                    return true;
                }
            }
        }
        return false;
    }
}