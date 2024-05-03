//
// Created by eternal on 2024/5/1.
//

#ifndef LEARNINGVULKAN_HELLOTRIANGLE_HH
#define LEARNINGVULKAN_HELLOTRIANGLE_HH

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <utility>
#include "base/VulkanBaseApp.hh"
#include "vulkan_wrapper.hh"

class HelloTriangle final : public VulkanBaseApp {
    struct SwapchainDimensions {
        VkExtent2D extent;

        /// Pixel format of the swapchain
        VkFormat format = VK_FORMAT_UNDEFINED;
    };

    /**
     * @brief Per-frame data
     */
    struct PerFrameData {
        VkDevice device = VK_NULL_HANDLE;

        VkFence queueSubmitFence = VK_NULL_HANDLE;

        VkCommandPool primaryCommandPool = VK_NULL_HANDLE;

        VkCommandBuffer primaryCommandBuffer = VK_NULL_HANDLE;

        VkSemaphore swapchainAcquireSemaphore = VK_NULL_HANDLE;

        VkSemaphore swapchainReleaseSemaphore = VK_NULL_HANDLE;

        int32_t queueIndex = -1;
    };

    struct Context {
        VkInstance instance = VK_NULL_HANDLE;

        VkPhysicalDevice gpu = VK_NULL_HANDLE;

        VkDevice device = VK_NULL_HANDLE;

        VkQueue queue = VK_NULL_HANDLE;

        VkSwapchainKHR swapchain = VK_NULL_HANDLE;

        SwapchainDimensions swapchainDimensions{};

        VkSurfaceKHR surface = VK_NULL_HANDLE;

        int32_t graphicsQueueIndex = -1;

        std::vector<VkImageView> swapchainImageViews{};

        std::vector<VkFramebuffer> swapchainFramebuffers{};

        VkRenderPass renderPass = VK_NULL_HANDLE;

        VkPipeline pipeline = VK_NULL_HANDLE;

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

        VkBuffer vertexBuffer;

        /// A set of semaphores that can be reused
        std::vector<VkSemaphore> recycledSemaphores{};

        /// A set of per-frame data
        std::vector<PerFrameData> perFrame{};
    };
public:
    explicit HelloTriangle(android_app* pApp);

    ~HelloTriangle() override;

    bool prepare(ANativeWindow *window) override;

    void update(float deltaTime) override;

private:
    Context context;

    android_app* androidAppCtx;

    void teardown();

    void initInstance(std::vector<const char *> &&requiredInstanceExtensions);

    bool initDevice(std::vector<const char *> &&requiredDeviceExtensions);

    void initSwapchain();

    void initRenderPass();

    void initPipeline();

    void initFramebuffers();

    void teardownFramebuffers();

    void initVertexBuffers();

    void initPerFrame(PerFrameData &perFrame) const;

    void teardownPerFrame(PerFrameData &perFrame);

    void renderTriangle(uint32_t swapchainIndex);

    VkResult acquireNextImage(uint32_t *image);

    VkResult presentImage(uint32_t index);

    void updateVertexBuffer(const VkCommandBuffer& commandBuffer);

    static bool validateExtensions(const std::vector<const char *> &requiredExtensions,
                                   const std::vector<VkExtensionProperties> &availableExtensions);
};

#endif //LEARNINGVULKAN_HELLOTRIANGLE_HH
