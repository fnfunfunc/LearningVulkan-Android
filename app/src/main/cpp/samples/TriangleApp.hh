//
// Created by eternal on 2024/5/1.
//

#ifndef LEARNINGVULKAN_TRIANGLEAPP_HH
#define LEARNINGVULKAN_TRIANGLEAPP_HH

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <glm/glm.hpp>
#include <optional>
#include <utility>
#include "VulkanBaseApp.hh"
#include "vulkan_wrapper.hh"

class TriangleApp final : public VulkanBaseApp {
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
    };

    struct Vertex {
        glm::vec2 position;
        glm::vec4 color;
    };

    struct Context {
        VkInstance instance = VK_NULL_HANDLE;

        VkPhysicalDevice gpu = VK_NULL_HANDLE;

        VkDevice device = VK_NULL_HANDLE;

        VkQueue queue = VK_NULL_HANDLE;

        VkSwapchainKHR swapchain = VK_NULL_HANDLE;

        SwapchainDimensions swapchainDimensions{};

        VkSurfaceKHR surface = VK_NULL_HANDLE;

        std::optional<uint32_t> graphicsQueueIndex = std::nullopt;

        std::vector<VkImageView> swapchainImageViews{};

        std::vector<VkFramebuffer> swapchainFramebuffers{};

        VkRenderPass renderPass = VK_NULL_HANDLE;

        VkPipeline pipeline = VK_NULL_HANDLE;

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

        VkBuffer vertexBuffer;

        VkBuffer indexBuffer;

        /// A set of semaphores that can be reused
        std::vector<VkSemaphore> recycledSemaphores{};

        /// A set of per-frame data
        std::vector<PerFrameData> perFrame{};
    };
public:
    explicit TriangleApp(android_app* pApp);

    ~TriangleApp() override;

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

    void initBuffers();

    void initPerFrame(PerFrameData &perFrame) const;

    void teardownPerFrame(PerFrameData &perFrame) const;

    void renderTriangle(uint32_t swapchainIndex);

    VkResult acquireNextImage(uint32_t *image);

    VkResult presentImage(uint32_t index);

    void updateVertexBuffer(const VkCommandBuffer& commandBuffer) const;

    static bool validateExtensions(const std::vector<const char *> &requiredExtensions,
                                   const std::vector<VkExtensionProperties> &availableExtensions);
};

#endif //LEARNINGVULKAN_TRIANGLEAPP_HH
