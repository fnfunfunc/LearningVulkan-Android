//
// Created by eternal on 2024/4/29.
//

#ifndef LEARNINGVULKAN_TRIANGLEAPP_HH
#define LEARNINGVULKAN_TRIANGLEAPP_HH

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include "vulkan_wrapper.hh"
#include "VulkanDefinition.hh"

class TriangleApp {
public:
    explicit TriangleApp(android_app *pApp) noexcept;

    void run();

    bool isReady() const;

    void release();

    bool drawFrame();

private:
    android_app *_androidAppCtx;
    VulkanDeviceInfo _device{};
    VulkanSwapchainInfo _swapchain{};
    VulkanRenderInfo _render{};
    VulkanBufferInfo _buffer{};
    VulkanGfxPipelineInfo _gfxPipelineInfo{};

    bool _initVulkan();

    void _deleteVulkan();

    void _createVulkanDevice(ANativeWindow *platformWindow, VkApplicationInfo *applicationInfo);
    void _createSwapchain();
    void _createRenderPass();
    void _createFrameBuffers(const VkRenderPass &renderPass, VkImageView depthView = VK_NULL_HANDLE);
    void _createBuffers();
    VkResult _createGraphicsPipeline();
    void _createCommands();

    void _deleteSwapchain();
    void _deleteGraphicsPipeline() const;
    void _deleteBuffers() const;

};

#endif //LEARNINGVULKAN_TRIANGLEAPP_HH
