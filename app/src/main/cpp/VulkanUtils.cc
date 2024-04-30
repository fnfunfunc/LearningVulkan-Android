//
// Created by eternal on 2024/4/29.
//

#include <cassert>
#include "VulkanUtils.hh"

namespace vulkan_utils {
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

    void
    setImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldImageLayout,
                   VkImageLayout newImageLayout, VkPipelineStageFlags srcStages,
                   VkPipelineStageFlags dstStages) {
        VkAccessFlags srcAccessMask, dstAccessMask;
        switch (oldImageLayout) {
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                break;
            default:
                srcAccessMask = 0;
                break;
        }

        switch (newImageLayout) {
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;
            case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL:
                dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;
            default:
                dstAccessMask = 0;
                break;
        }

        VkImageMemoryBarrier imageMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = srcAccessMask,
                .dstAccessMask = dstAccessMask,
                .oldLayout = oldImageLayout,
                .newLayout = newImageLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                }
        };

        vkCmdPipelineBarrier(commandBuffer, srcStages, dstStages,
                             0, 0, nullptr,
                             0, nullptr, 1,
                             &imageMemoryBarrier);
    }
}