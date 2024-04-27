//
// Created by eternal on 2024/4/26.
//
#include <android/log.h>
#include <cassert>
#include <vector>
#include "VulkanMain.hh"
#include "vulkan_wrapper.hh"

// Android log function wrappers
static const char *kTAG = "Native_LearningVulkan";
#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...) \
  ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))

// Vulkan call wrapper
#define CALL_VK(func)                                                 \
  if (VK_SUCCESS != (func)) {                                         \
    __android_log_print(ANDROID_LOG_ERROR, "LearningVulkan ",               \
                        "Vulkan error. File[%s], line[%d]", __FILE__, \
                        __LINE__);                                    \
    assert(false);                                                    \
  }


struct VulkanDeviceInfo {
    bool initialized;
    VkInstance instance;
    VkPhysicalDevice gpuDevice;
    VkDevice device;
    uint32_t queueFamilyIndex;

    VkSurfaceKHR surface;
    VkQueue queue;
} device;

struct VulkanSwapchainInfo {
    VkSwapchainKHR swapchain;
    uint32_t swapchainLength;

    VkExtent2D displaySize;
    VkFormat displayFormat;

    std::vector<VkImage> displayImages;
    std::vector<VkImageView> displayViews;
    std::vector<VkFramebuffer> framebuffers;
} swapchain;

struct VulkanRenderInfo {
    VkRenderPass renderPass;
    VkCommandPool commandPool;
    VkCommandBuffer *commandBuffers;
    uint32_t commandBuffersLength;
    VkSemaphore semaphore;
    VkFence fence;
} render;

struct VulkanBufferInfo {
    VkBuffer vertexBuffer;
} buffers;

struct VulkanGfxPipelineInfo {
    VkPipelineLayout layout;
    VkPipelineCache cache;
    VkPipeline pipeline;
} gfxPipelineInfo;

android_app *androidAppCtx = nullptr;

// Helper function to transition color buffer layout
static void
setImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldImageLayout,
               VkImageLayout newImageLayout, VkPipelineStageFlags srcStages,
               VkPipelineStageFlags destStages);

void CreateVulkanDevice(ANativeWindow *platformWindow, VkApplicationInfo *applicationInfo) {
    std::vector<const char *> instanceExtensions{"VK_KHR_surface", "VK_KHR_android_surface"};
    std::vector<const char *> deviceExtensions{"VK_KHR_swapchain"};

    VkInstanceCreateInfo instanceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .pApplicationInfo = applicationInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
            .ppEnabledExtensionNames = instanceExtensions.data()
    };
    CALL_VK(vkCreateInstance(&instanceCreateInfo, nullptr, &device.instance))

    VkAndroidSurfaceCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .window = platformWindow
    };
    CALL_VK(vkCreateAndroidSurfaceKHR(device.instance, &createInfo, nullptr, &device.surface))

    // Find one GPU to use
    uint32_t gpuCount = 0;
    CALL_VK(vkEnumeratePhysicalDevices(device.instance, &gpuCount, nullptr))
    std::vector<VkPhysicalDevice> tmpGpus(gpuCount);
    CALL_VK(vkEnumeratePhysicalDevices(device.instance, &gpuCount, tmpGpus.data()))
    device.gpuDevice = tmpGpus[0];

    // Find a GFX queue family
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(device.gpuDevice, &queueFamilyCount, nullptr);
    assert(queueFamilyCount > 0);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device.gpuDevice, &queueFamilyCount,
                                             queueFamilyProperties.data());

    const uint32_t queueFamilyIndex = std::distance(queueFamilyProperties.begin(),
                                                    std::find_if(queueFamilyProperties.begin(),
                                                                 queueFamilyProperties.end(),
                                                                 [](const auto &property) {
                                                                     return (property.queueFlags &
                                                                             VK_QUEUE_GRAPHICS_BIT) !=
                                                                            0;
                                                                 }));
    assert(queueFamilyIndex < queueFamilyCount);
    device.queueFamilyIndex = queueFamilyIndex;

    // Create a logical device(vulkan device)
    float priorities[]{
            1.0f
    };
    VkDeviceQueueCreateInfo queueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = queueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = priorities
    };

    VkDeviceCreateInfo deviceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = nullptr,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<uint32_t >(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures = nullptr
    };

    CALL_VK(vkCreateDevice(device.gpuDevice, &deviceCreateInfo, nullptr, &device.device))
    vkGetDeviceQueue(device.device, device.queueFamilyIndex, 0, &device.queue);
}

void CreateSwapChain() {
    LOGI("->createSwapChain");
    memset(&swapchain, 0, sizeof(swapchain));

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.gpuDevice, device.surface,
                                              &surfaceCapabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.gpuDevice, device.surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.gpuDevice, device.surface, &formatCount,
                                         formats.data());
    LOGI("Got %d formats", formatCount);

    const auto chosenFormatIter =
            std::find_if(formats.begin(), formats.end(),
                         [](const auto &format) {
                             return format.format == VK_FORMAT_R8G8B8A8_UNORM;
                         });
    assert(chosenFormatIter != formats.end());

    swapchain.displaySize = surfaceCapabilities.currentExtent;
    swapchain.displayFormat = chosenFormatIter->format;

    VkSwapchainCreateInfoKHR swapchainCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .surface = device.surface,
            .minImageCount = surfaceCapabilities.minImageCount,
            .imageFormat = chosenFormatIter->format,
            .imageColorSpace = chosenFormatIter->colorSpace,
            .imageExtent = surfaceCapabilities.currentExtent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &device.queueFamilyIndex,
            .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = VK_PRESENT_MODE_FIFO_KHR,
            .clipped = VK_FALSE,
            .oldSwapchain = VK_NULL_HANDLE
    };
    CALL_VK(vkCreateSwapchainKHR(device.device, &swapchainCreateInfo, nullptr,
                                 &swapchain.swapchain))

    CALL_VK(vkGetSwapchainImagesKHR(device.device, swapchain.swapchain, &swapchain.swapchainLength,
                                    nullptr))

    LOGI("<-createSwapchain");
}

void DeleteSwapchain() {
    for (uint32_t i = 0; i < swapchain.swapchainLength; ++i) {
        vkDestroyFramebuffer(device.device, swapchain.framebuffers[i], nullptr);
        vkDestroyImageView(device.device, swapchain.displayViews[i], nullptr);
    }
    vkDestroySwapchainKHR(device.device, swapchain.swapchain, nullptr);
}

void CreateFrameBuffers(VkRenderPass &renderPass, VkImageView depthView = VK_NULL_HANDLE) {
    uint32_t swapchainImagesCount = 0;
    CALL_VK(vkGetSwapchainImagesKHR(device.device, swapchain.swapchain, &swapchainImagesCount,
                                    nullptr))
    swapchain.displayImages.resize(swapchainImagesCount);
    CALL_VK(vkGetSwapchainImagesKHR(device.device, swapchain.swapchain, &swapchainImagesCount,
                                    swapchain.displayImages.data()))

    swapchain.displayViews.resize(swapchainImagesCount);
    for (uint32_t i = 0; i < swapchainImagesCount; ++i) {
        VkImageViewCreateInfo viewCreateInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .image = swapchain.displayImages[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = swapchain.displayFormat,
                .components = {
                        .r = VK_COMPONENT_SWIZZLE_R,
                        .g = VK_COMPONENT_SWIZZLE_G,
                        .b = VK_COMPONENT_SWIZZLE_B,
                        .a = VK_COMPONENT_SWIZZLE_A
                },
                .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                }
        };
        CALL_VK(vkCreateImageView(device.device, &viewCreateInfo, nullptr,
                                  &swapchain.displayViews.at(i)))
    }

    swapchain.framebuffers.resize(swapchain.swapchainLength);
    for (uint32_t i = 0; i < swapchain.swapchainLength; ++i) {
        std::vector<VkImageView> attachments{swapchain.displayViews.at(i)};
        if (depthView != VK_NULL_HANDLE) {
            attachments.emplace_back(depthView);
        }
        VkFramebufferCreateInfo fbCreateInfo{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext = nullptr,
                .renderPass = renderPass,
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments = attachments.data(),
                .width = static_cast<uint32_t>(swapchain.displaySize.width),
                .height = static_cast<uint32_t>(swapchain.displaySize.height),
                .layers = 1,
        };
        CALL_VK(vkCreateFramebuffer(device.device, &fbCreateInfo, nullptr,
                                    &swapchain.framebuffers.at(i)))
    }
}

bool MapMemoryTypeToIndex(uint32_t typeBits, VkFlags requirementsMask, uint32_t *typeIndex) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(device.gpuDevice, &memoryProperties);

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

bool CreateBuffers() {
    const float vertexData[] = {
            -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f
    };

    // Create a vertex buffer
    VkBufferCreateInfo bufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = sizeof(vertexData),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &device.queueFamilyIndex
    };

    CALL_VK(vkCreateBuffer(device.device, &bufferCreateInfo, nullptr, &buffers.vertexBuffer))

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device.device, buffers.vertexBuffer, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = 0
    };

    MapMemoryTypeToIndex(memoryRequirements.memoryTypeBits,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         &allocateInfo.memoryTypeIndex);

    VkDeviceMemory deviceMemory;
    vkAllocateMemory(device.device, &allocateInfo, nullptr, &deviceMemory);

    void *data;
    CALL_VK(vkMapMemory(device.device, deviceMemory, 0, allocateInfo.allocationSize, 0, &data))

    memcpy(data, vertexData, sizeof(vertexData));
    vkUnmapMemory(device.device, deviceMemory);

    CALL_VK(vkBindBufferMemory(device.device, buffers.vertexBuffer, deviceMemory, 0));
    return true;
}

void DeleteBuffers() {
    vkDestroyBuffer(device.device, buffers.vertexBuffer, nullptr);
}

enum ShaderType {
    VERTEX_SHADER, FRAGMENT_SHADER
};

VkResult
loadShaderFromFile(const char *filePath, VkShaderModule *shaderOut, ShaderType shaderType) {
    assert(androidAppCtx != nullptr);
    AAsset *assetFile = AAssetManager_open(androidAppCtx->activity->assetManager, filePath,
                                           AASSET_MODE_BUFFER);
    const size_t fileLength = AAsset_getLength(assetFile);
    char *fileContent = new char[fileLength];

    AAsset_read(assetFile, fileContent, fileLength);
    AAsset_close(assetFile);

    VkShaderModuleCreateInfo shaderModuleCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = fileLength,
            .pCode = reinterpret_cast<const uint32_t *>(fileContent)
    };
    VkResult result = vkCreateShaderModule(device.device, &shaderModuleCreateInfo, nullptr,
                                           shaderOut);

    delete[] fileContent;

    return result;
}

VkResult CreateGraphicsPipeline() {
    memset(&gfxPipelineInfo, 0, sizeof(gfxPipelineInfo));
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .setLayoutCount = 0,
            .pSetLayouts = nullptr,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr
    };
    CALL_VK(vkCreatePipelineLayout(device.device, &pipelineLayoutCreateInfo, nullptr,
                                   &gfxPipelineInfo.layout))

    VkShaderModule vertexShader, fragmentShader;
    loadShaderFromFile("shaders/triangle.vert.spv", &vertexShader, ShaderType::VERTEX_SHADER);
    loadShaderFromFile("shaders/triangle.frag.spv", &fragmentShader, ShaderType::FRAGMENT_SHADER);

    // Specify vertex and fragment shader stages
    VkPipelineShaderStageCreateInfo shaderStages[2]{
            {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .stage = VK_SHADER_STAGE_VERTEX_BIT,
                    .module = vertexShader,
                    .pName = "main",
                    .pSpecializationInfo = nullptr,
            },
            {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .module = fragmentShader,
                    .pName = "main",
                    .pSpecializationInfo = nullptr
            }
    };

    VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(swapchain.displaySize.width),
            .height = static_cast<float>(swapchain.displaySize.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
    };

    VkRect2D scissor{
            .offset {
                    .x = 0,
                    .y = 0
            },
            .extent {swapchain.displaySize}
    };

    // Specify viewport info
    VkPipelineViewportStateCreateInfo viewportInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .viewportCount = 1,
            .pViewports = &viewport,
            .scissorCount = 1,
            .pScissors = &scissor
    };

    // Specify multi sample info
    VkSampleMask sampleMask = ~0u;
    VkPipelineMultisampleStateCreateInfo multisampleInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = nullptr,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 0,
            .pSampleMask = &sampleMask,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
    };

    // Specify color blend state
    VkPipelineColorBlendAttachmentState attachmentState{
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo colorBlendInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &attachmentState
    };

    // Specify rasterizer info
    VkPipelineRasterizationStateCreateInfo rasterInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1
    };

    // Specify input assembly info
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE
    };

#pragma region Vertex input state
    // Specify vertex input state
    VkVertexInputBindingDescription vertexInputBindingDescription{
            .binding = 0,
            .stride = 3 * sizeof(float),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription vertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = 0
    };
    VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vertexInputBindingDescription,
            .vertexAttributeDescriptionCount = 1,
            .pVertexAttributeDescriptions = &vertexInputAttributeDescription
    };
#pragma endregion

    // Create pipeline cache
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .initialDataSize = 0,
            .pInitialData = nullptr
    };
    CALL_VK(vkCreatePipelineCache(device.device, &pipelineCacheCreateInfo, nullptr,
                                  &gfxPipelineInfo.cache))

    // Create the pipeline
    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stageCount = 2,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInputStateCreateInfo,
            .pInputAssemblyState = &inputAssemblyStateCreateInfo,
            .pTessellationState = nullptr,
            .pViewportState = &viewportInfo,
            .pRasterizationState = &rasterInfo,
            .pMultisampleState = &multisampleInfo,
            .pDepthStencilState = nullptr,
            .pColorBlendState = &colorBlendInfo,
            .pDynamicState = nullptr,
            .layout = gfxPipelineInfo.layout,
            .renderPass = render.renderPass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0
    };

    VkResult pipelineResult = vkCreateGraphicsPipelines(device.device, gfxPipelineInfo.cache, 1,
                                                        &graphicsPipelineCreateInfo, nullptr,
                                                        &gfxPipelineInfo.pipeline);

    // Release the shader
    vkDestroyShaderModule(device.device, vertexShader, nullptr);
    vkDestroyShaderModule(device.device, fragmentShader, nullptr);

    return pipelineResult;
}

void DeleteGraphicsPipeline() {
    if (gfxPipelineInfo.pipeline == VK_NULL_HANDLE) {
        return;
    }
    vkDestroyPipeline(device.device, gfxPipelineInfo.pipeline, nullptr);
    vkDestroyPipelineCache(device.device, gfxPipelineInfo.cache, nullptr);
    vkDestroyPipelineLayout(device.device, gfxPipelineInfo.layout, nullptr);
}

bool InitVulkan(android_app *pApp) {
    androidAppCtx = pApp;

    if (!InitVulkan()) {
        LOGW("Vulkan is unavailable, install vulkan and re-start");
        return false;
    }

    VkApplicationInfo appInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = "first_window",
            .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .pEngineName = "LearningVulkan",
            .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .apiVersion = VK_MAKE_API_VERSION(0, 1, 1, 0),
    };

    CreateVulkanDevice(pApp->window, &appInfo);
    CreateSwapChain();

    VkAttachmentDescription attachmentDescription{
            .format = swapchain.displayFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorReference{
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpassDescription{
            .flags = 0,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = 0,
            .pInputAttachments = nullptr,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorReference,
            .pResolveAttachments = nullptr,
            .pDepthStencilAttachment = nullptr,
            .preserveAttachmentCount = 0,
            .pPreserveAttachments = nullptr
    };
    VkRenderPassCreateInfo renderPassCreateInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = nullptr,
            .attachmentCount = 1,
            .pAttachments = &attachmentDescription,
            .subpassCount = 1,
            .pSubpasses = &subpassDescription,
            .dependencyCount = 0,
            .pDependencies = nullptr
    };
    CALL_VK(vkCreateRenderPass(device.device, &renderPassCreateInfo, nullptr, &render.renderPass))

    // Create 2 frame buffers
    CreateFrameBuffers(render.renderPass);

    // Create buffers
    CreateBuffers();

    // Create graphics pipeline
    CreateGraphicsPipeline();

    // Create a pool of command buffers to allocate command buffer from
    VkCommandPoolCreateInfo commandPoolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = 0
    };
    CALL_VK(vkCreateCommandPool(device.device, &commandPoolCreateInfo, nullptr,
                                &render.commandPool))

    render.commandBuffersLength = swapchain.swapchainLength;
    render.commandBuffers = new VkCommandBuffer[swapchain.swapchainLength];
    for (uint32_t bufferIndex = 0; bufferIndex < swapchain.swapchainLength; ++bufferIndex) {
        VkCommandBufferAllocateInfo commandBufferAllocateInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = render.commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
        };
        CALL_VK(vkAllocateCommandBuffers(device.device, &commandBufferAllocateInfo,
                                         &render.commandBuffers[bufferIndex]))

        VkCommandBufferBeginInfo commandBufferBeginInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = nullptr,
                .flags = 0,
                .pInheritanceInfo = nullptr
        };
        CALL_VK(vkBeginCommandBuffer(render.commandBuffers[bufferIndex], &commandBufferBeginInfo))

        // Transition the display image to color attachment layout
        setImageLayout(render.commandBuffers[bufferIndex], swapchain.displayImages[bufferIndex],
                       VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        // Start a renderpass. Any draw command has to be recorded in a renderpass
        VkClearValue clearValue{
                .color = {
                        .float32 {0.90f, 0.10f, 0.10f, 1.0f}
                }
        };
        VkRenderPassBeginInfo renderPassBeginInfo{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .pNext = nullptr,
                .renderPass = render.renderPass,
                .framebuffer = swapchain.framebuffers[bufferIndex],
                .renderArea = {.offset {.x = 0, .y = 0}, .extent = swapchain.displaySize},
                .clearValueCount = 1,
                .pClearValues = &clearValue,
        };
        vkCmdBeginRenderPass(render.commandBuffers[bufferIndex], &renderPassBeginInfo,
                             VK_SUBPASS_CONTENTS_INLINE);

        // Bind pipeline and vertex buffers
        vkCmdBindPipeline(render.commandBuffers[bufferIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, gfxPipelineInfo.pipeline);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(render.commandBuffers[bufferIndex], 0, 1, &buffers.vertexBuffer, &offset);

        // Draw triangle
        vkCmdDraw(render.commandBuffers[bufferIndex], 3, 1, 0, 0);

        vkCmdEndRenderPass(render.commandBuffers[bufferIndex]);

        // Transition back to swapchain image to PRESENT_SRC_KHR
        setImageLayout(render.commandBuffers[bufferIndex], swapchain.displayImages[bufferIndex],
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        CALL_VK(vkEndCommandBuffer(render.commandBuffers[bufferIndex]))
    }

    // Create a fence to be able to wait for draw commands to finish in the main loop before swapping the framebuffers
    VkFenceCreateInfo fenceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
    };
    CALL_VK(vkCreateFence(device.device, &fenceCreateInfo, nullptr, &render.fence))

    // Create a semaphore to wait for framebuffer to be available before drawing
    VkSemaphoreCreateInfo semaphoreCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
    };
    CALL_VK(vkCreateSemaphore(device.device, &semaphoreCreateInfo, nullptr, &render.semaphore))

    device.initialized = true;
    return true;
}

bool IsVulkanReady() {
    return device.initialized;
}

void DeleteVulkan() {
    vkFreeCommandBuffers(device.device, render.commandPool, render.commandBuffersLength,
                         render.commandBuffers);
    delete[] render.commandBuffers;

    vkDestroyCommandPool(device.device, render.commandPool, nullptr);
    vkDestroyRenderPass(device.device, render.renderPass, nullptr);
    DeleteSwapchain();

    vkDestroyDevice(device.device, nullptr);
    vkDestroyInstance(device.instance, nullptr);

    device.initialized = false;
}

bool VulkanDrawFrame() {
    uint32_t nextIndex;
    CALL_VK(vkAcquireNextImageKHR(device.device, swapchain.swapchain,
                                  std::numeric_limits<uint64_t>::max(), render.semaphore,
                                  VK_NULL_HANDLE, &nextIndex))

    CALL_VK(vkResetFences(device.device, 1, &render.fence))

    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &render.semaphore,
            .pWaitDstStageMask = &waitStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = &render.commandBuffers[nextIndex],
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
    };
    CALL_VK(vkQueueSubmit(device.queue, 1, &submitInfo, render.fence))
    CALL_VK(vkWaitForFences(device.device, 1, &render.fence, VK_TRUE, 100'000'000))

    LOGI("Drawing frames...");

    VkResult result;
    VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .swapchainCount = 1,
            .pSwapchains = &swapchain.swapchain,
            .pImageIndices = &nextIndex,
            .pResults = &result
    };
    vkQueuePresentKHR(device.queue, &presentInfo);
    return true;
}

void setImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldImageLayout,
                    VkImageLayout newImageLayout, VkPipelineStageFlags srcStages,
                    VkPipelineStageFlags destStages) {

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
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            break;

        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            dstAccessMask =
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
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

    vkCmdPipelineBarrier(commandBuffer, srcStages, destStages,
                         0, 0, nullptr,
                         0, nullptr, 1,
                         &imageMemoryBarrier);
}