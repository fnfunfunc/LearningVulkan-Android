//
// Created by eternal on 2024/4/29.
//

#ifndef LEARNINGVULKAN_VULKANDEFINITION_HH
#define LEARNINGVULKAN_VULKANDEFINITION_HH

#include <vector>
#include "vulkan_wrapper.hh"

struct VulkanDeviceInfo {
    bool initialized;
    VkInstance instance;
    VkPhysicalDevice gpuDevice;
    VkDevice device;
    uint32_t queueFamilyIndex;

    VkSurfaceKHR surface;
    VkQueue queue;
};

struct VulkanSwapchainInfo {
    VkSwapchainKHR swapchain{};
    uint32_t swapchainLength{};

    VkExtent2D displaySize{};
    VkFormat displayFormat{};

    std::vector<VkImage> displayImages{};
    std::vector<VkImageView> displayViews{};
    std::vector<VkFramebuffer> framebuffers{};
};

struct VulkanRenderInfo {
    VkRenderPass renderPass;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    VkSemaphore semaphore;
    VkFence fence;
};

struct VulkanBufferInfo {
    VkBuffer vertexBuffer;
};

struct VulkanGfxPipelineInfo {
    VkPipelineLayout layout;
    VkPipelineCache cache;
    VkPipeline pipeline;
};

#endif //LEARNINGVULKAN_VULKANDEFINITION_HH
