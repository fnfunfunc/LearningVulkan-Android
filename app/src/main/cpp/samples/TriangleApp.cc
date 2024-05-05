//
// Created by eternal on 2024/5/1.
//
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include "Debug.hh"
#include "MathUtils.hh"
#include "TriangleApp.hh"
#include "VulkanCommon.hh"

TriangleApp::TriangleApp(android_app *pApp) : androidAppCtx(pApp) {}

TriangleApp::~TriangleApp() {
    teardown();
    androidAppCtx = nullptr;
}

bool TriangleApp::prepare(ANativeWindow *window) {
    assert(window != nullptr);

    initInstance({VK_KHR_SURFACE_EXTENSION_NAME});

    VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .window = window
    };

    CALL_VK(vkCreateAndroidSurfaceKHR(context.instance, &surfaceCreateInfo, nullptr,
                                      &context.surface))

    if (context.surface == VK_NULL_HANDLE) {
        LOGE("Failed to create window surface.");
        return false;
    }

    if (!initDevice({VK_KHR_SWAPCHAIN_EXTENSION_NAME})) {
        return false;
    }

    initSwapchain();

    initRenderPass();
    initDescriptorSetLayout();
    initPipeline();
    initFramebuffers();

    initVertexBuffers();
    initIndexBuffers();
    initUniformBuffers();

    initDescriptorPool();
    initDescriptorSets();

    isReady_ = true;
    return true;
}

void TriangleApp::update([[maybe_unused]] float deltaTime) {
    uint32_t index;

    VkResult result = acquireNextImage(&index);

    if (result != VK_SUCCESS) {
        vkQueueWaitIdle(context.queue);
        return;
    }

    renderTriangle(index);

    result = presentImage(index);

    if (result != VK_SUCCESS) {
        LOGE("Failed to present swapchain image.");
    }
}

void TriangleApp::teardown() {
    // Don't release anything until the GPU is completely idle.
    vkDeviceWaitIdle(context.device);

    teardownFramebuffers();

    for (auto &perFrame: context.perFrame) {
        teardownPerFrame(perFrame);
    }

    context.perFrame.clear();

    for (auto semaphore: context.recycledSemaphores) {
        vkDestroySemaphore(context.device, semaphore, nullptr);
    }

    if (context.uniformBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context.device, context.uniformBuffer, nullptr);
        context.uniformBuffer = VK_NULL_HANDLE;
    }

    if (context.indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context.device, context.indexBuffer, nullptr);
        context.indexBuffer = VK_NULL_HANDLE;
    }

    if (context.vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context.device, context.vertexBuffer, nullptr);
        context.vertexBuffer = VK_NULL_HANDLE;
    }

    if (context.pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context.device, context.pipeline, nullptr);
        context.pipeline = VK_NULL_HANDLE;
    }

    if (context.descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context.device, context.descriptorSetLayout, nullptr);
        context.descriptorSetLayout = VK_NULL_HANDLE;
    }

    if (context.pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context.device, context.pipelineLayout, nullptr);
        context.pipelineLayout = VK_NULL_HANDLE;
    }

    if (context.renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(context.device, context.renderPass, nullptr);
        context.renderPass = VK_NULL_HANDLE;
    }

    if (context.descriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(context.device, context.descriptorPool, 1, &context.descriptorSet);
        context.descriptorSet = VK_NULL_HANDLE;
    }

    if (context.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context.device, context.descriptorPool, nullptr);
        context.descriptorPool = VK_NULL_HANDLE;
    }

    for (VkImageView imageView: context.swapchainImageViews) {
        vkDestroyImageView(context.device, imageView, nullptr);
    }
    context.swapchainImageViews.clear();

    if (context.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
        context.surface = VK_NULL_HANDLE;
    }

    if (context.device != VK_NULL_HANDLE) {
        vkDestroyDevice(context.device, nullptr);
        context.device = VK_NULL_HANDLE;
    }
    context.graphicsQueueIndex = std::nullopt;
}

void TriangleApp::initInstance(std::vector<const char *> &&requiredInstanceExtensions) {
    if (!InitVulkan()) {
        LOGE("Failed to initialize vulkan.");
        return;
    }

    uint32_t availableInstanceExtensionsCount;
    CALL_VK(vkEnumerateInstanceExtensionProperties(nullptr, &availableInstanceExtensionsCount,
                                                   nullptr))
    std::vector<VkExtensionProperties> availableInstanceExtensions(
            availableInstanceExtensionsCount);
    CALL_VK(vkEnumerateInstanceExtensionProperties(nullptr, &availableInstanceExtensionsCount,
                                                   availableInstanceExtensions.data()))

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    requiredInstanceExtensions.emplace_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif

    if (!validateExtensions(requiredInstanceExtensions, availableInstanceExtensions)) {
        LOGE("Required instance extensions are missing.");
        return;
    }

    const VkApplicationInfo applicationInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = "Hello Triangle",
            .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .pEngineName = "main",
            .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .apiVersion = VK_MAKE_API_VERSION(0, 1, 1, 0)
    };

    VkInstanceCreateInfo instanceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pApplicationInfo = &applicationInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<uint32_t>(requiredInstanceExtensions.size()),
            .ppEnabledExtensionNames = requiredInstanceExtensions.data()
    };

    CALL_VK(vkCreateInstance(&instanceCreateInfo, nullptr, &context.instance))
}

bool TriangleApp::initDevice(std::vector<const char *> &&requiredDeviceExtensions) {
    uint32_t gpuCount = 0;
    CALL_VK(vkEnumeratePhysicalDevices(context.instance, &gpuCount, nullptr))
    assert(gpuCount > 0);
    std::vector<VkPhysicalDevice> allGpus(gpuCount);
    CALL_VK(vkEnumeratePhysicalDevices(context.instance, &gpuCount, allGpus.data()))

    for (const auto gpu: allGpus) {
        uint32_t queueFamilyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);
        if (queueFamilyCount < 1) {
            continue;
        }
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount,
                                                 queueFamilyProperties.data());

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            VkBool32 supportedPresent = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, context.surface, &supportedPresent);

            const bool isGraphicsQueue =
                    (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;

            if (isGraphicsQueue && supportedPresent) {
                context.gpu = gpu;
                context.graphicsQueueIndex = i;
                break;
            }
        }

        if (context.graphicsQueueIndex >= 0) { // found suitable queue index
            break;
        }
    }

    if (context.graphicsQueueIndex < 0) {
        LOGE("Did not find suitable queue which supports graphics, compute and presentation.");
        return false;
    }

    uint32_t availableDeviceExtensionsCount;
    CALL_VK(vkEnumerateDeviceExtensionProperties(context.gpu, nullptr,
                                                 &availableDeviceExtensionsCount, nullptr))
    std::vector<VkExtensionProperties> availableDeviceExtensions(availableDeviceExtensionsCount);
    CALL_VK(vkEnumerateDeviceExtensionProperties(context.gpu, nullptr,
                                                 &availableDeviceExtensionsCount,
                                                 availableDeviceExtensions.data()))

    if (!validateExtensions(requiredDeviceExtensions, availableDeviceExtensions)) {
        LOGE("Required device extensions are missing");
        return false;
    }

    const float queuePriorities[]{1.0f};

    VkDeviceQueueCreateInfo deviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = context.graphicsQueueIndex.value(),
            .queueCount = 1,
            .pQueuePriorities = queuePriorities
    };

    VkDeviceCreateInfo deviceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
            .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
            .pEnabledFeatures = nullptr
    };

    CALL_VK(vkCreateDevice(context.gpu, &deviceCreateInfo, nullptr, &context.device))

    if (context.graphicsQueueIndex.has_value()) {
        vkGetDeviceQueue(context.device, context.graphicsQueueIndex.value(), 0, &context.queue);
    } else {
        LOGE("context.graphicsQueueIndex is nullopt");
        return false;
    }

    return true;
}

void TriangleApp::initSwapchain() {
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    CALL_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.gpu, context.surface,
                                                      &surfaceCapabilities))

    VkSurfaceFormatKHR surfaceFormat = vulkan_common::selectSurfaceFormat(context.gpu,
                                                                          context.surface,
                                                                          {VK_FORMAT_R32G32B32A32_SFLOAT});

    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    // Determine the number of VkImage's to use in the swapchain.
    // Ideally, we desire to own 1 image at a time, the rest of the images can
    // either be rendered to and/or being queued up for display.
    uint32_t desiredSwapchainImages = surfaceCapabilities.minImageCount + 1;
    if ((surfaceCapabilities.maxImageCount > 0) &&
        (desiredSwapchainImages > surfaceCapabilities.maxImageCount)) {
        desiredSwapchainImages = surfaceCapabilities.maxImageCount;
    }

    // Figure out a suitable surface transform
    VkSurfaceTransformFlagBitsKHR preTransform;
    if ((surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0) {
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        preTransform = surfaceCapabilities.currentTransform;
    }

    const VkSwapchainKHR oldSwapchain = context.swapchain;

    // Find a supported composite alpha
    VkCompositeAlphaFlagBitsKHR composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) {
        composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    } else if (surfaceCapabilities.supportedCompositeAlpha &
               VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) {
        composite = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    } else if (surfaceCapabilities.supportedCompositeAlpha &
               VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) {
        composite = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = context.surface,
            .minImageCount = desiredSwapchainImages,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent {surfaceCapabilities.currentExtent},
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = reinterpret_cast<const uint32_t *>(&context.graphicsQueueIndex),
            .preTransform = preTransform,
            .compositeAlpha = composite,
            .presentMode = swapchainPresentMode,
            .clipped = true,
            .oldSwapchain = oldSwapchain
    };

    CALL_VK(vkCreateSwapchainKHR(context.device, &swapchainCreateInfo, nullptr, &context.swapchain))
    context.swapchainDimensions = {
            .extent {surfaceCapabilities.currentExtent},
            .format = surfaceFormat.format
    };

    if (oldSwapchain != VK_NULL_HANDLE) {
        for (VkImageView imageView: context.swapchainImageViews) {
            vkDestroyImageView(context.device, imageView, nullptr);
        }

        uint32_t imageCount;
        CALL_VK(vkGetSwapchainImagesKHR(context.device, oldSwapchain, &imageCount, nullptr))

        for (uint32_t i = 0; i < imageCount; ++i) {
            teardownPerFrame(context.perFrame.at(i));
        }

        context.swapchainImageViews.clear();
        vkDestroySwapchainKHR(context.device, oldSwapchain, nullptr);
    }

    uint32_t imageCount;
    CALL_VK(vkGetSwapchainImagesKHR(context.device, context.swapchain, &imageCount, nullptr))
    std::vector<VkImage> swapchainImages(imageCount);
    CALL_VK(vkGetSwapchainImagesKHR(context.device, context.swapchain, &imageCount,
                                    swapchainImages.data()))

    context.perFrame.clear();
    context.perFrame.resize(imageCount);

    for (uint32_t i = 0; i < imageCount; ++i) {
        initPerFrame(context.perFrame.at(i));
    }

    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo imageViewCreateInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .image = swapchainImages.at(i),
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = context.swapchainDimensions.format,
                .components {
                        .r = VK_COMPONENT_SWIZZLE_R,
                        .g = VK_COMPONENT_SWIZZLE_G,
                        .b = VK_COMPONENT_SWIZZLE_B,
                        .a = VK_COMPONENT_SWIZZLE_A
                },
                .subresourceRange {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                }
        };

        VkImageView imageView;
        CALL_VK(vkCreateImageView(context.device, &imageViewCreateInfo, nullptr, &imageView))

        context.swapchainImageViews.emplace_back(imageView);
    }
}

/**
 * @brief Initialize the Vulkan render pass
 */
void TriangleApp::initRenderPass() {
    VkAttachmentDescription attachmentDescription{
            .flags = 0,
            .format = context.swapchainDimensions.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            // When starting the frame, we want tiles to be cleared
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            // When ending the frame, we want tiles to be written out
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            // The image layout will be undefined when the render pass begins
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            // After the render pass is complete, we will transition to PRESENT_SRC_KHR layout
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

    VkSubpassDependency subpassDependency{
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
    };

    VkRenderPassCreateInfo renderPassCreateInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .attachmentCount = 1,
            .pAttachments = &attachmentDescription,
            .subpassCount = 1,
            .pSubpasses = &subpassDescription,
            .dependencyCount = 1,
            .pDependencies = &subpassDependency
    };

    CALL_VK(vkCreateRenderPass(context.device, &renderPassCreateInfo, nullptr, &context.renderPass))
}

void TriangleApp::initDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
    };

    VkDescriptorSetLayoutCreateInfo layoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = 1,
            .pBindings = &uboSetLayoutBinding
    };

    CALL_VK(vkCreateDescriptorSetLayout(context.device, &layoutCreateInfo, nullptr,
                                        &context.descriptorSetLayout))
}

void TriangleApp::initPipeline() {
    // Create a blank pipeline layout
    VkPipelineLayoutCreateInfo layoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = 1,
            .pSetLayouts = &context.descriptorSetLayout,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
    };
    CALL_VK(vkCreatePipelineLayout(context.device, &layoutCreateInfo, nullptr,
                                   &context.pipelineLayout))

    VkVertexInputBindingDescription inputBinding{
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription inputAttributes[]{
            {
                    .location = 0,
                    .binding = 0,
                    .format = VK_FORMAT_R32G32_SFLOAT,
                    .offset = offsetof(Vertex, position)
            },
            {
                    .location = 1,
                    .binding = 0,
                    .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                    .offset = offsetof(Vertex, color)
            }
    };

    VkPipelineVertexInputStateCreateInfo vertexInput{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &inputBinding,
            .vertexAttributeDescriptionCount = 2,
            .pVertexAttributeDescriptions = inputAttributes
    };

    // Specify we will use triangle lists to draw geometry.
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE
    };

    // Specify rasterization state
    VkPipelineRasterizationStateCreateInfo rasterization{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .depthBiasClamp = VK_FALSE,
            .lineWidth = 1.0f
    };

    // Our attachment will write to all color channels, but no blending is enabled.
    VkPipelineColorBlendAttachmentState colorBlendAttachmentState{
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colorBlend{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachmentState
    };

    VkPipelineViewportStateCreateInfo viewportState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .viewportCount = 1,
            // We will set it dynamically
            .pViewports = nullptr,
            .scissorCount = 1,
            // We will set it dynamically
            .pScissors = nullptr,
    };

    // Disable all depth testing
    VkPipelineDepthStencilStateCreateInfo depthStencilState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .depthTestEnable = VK_FALSE
    };

    // No multisampling
    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    // Specify that these states will be dynamic
    std::array<VkDynamicState, 2> dynamics{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = dynamics.size(),
            .pDynamicStates = dynamics.data()
    };

    VkShaderModule vertexShader, fragmentShader;
    vulkan_common::loadShaderFromFile(androidAppCtx, context.device,
                                      "shaders/triangle.vert.spv",
                                      &vertexShader);
    vulkan_common::loadShaderFromFile(androidAppCtx, context.device,
                                      "shaders/triangle.frag.spv",
                                      &fragmentShader);
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{
            VkPipelineShaderStageCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .stage = VK_SHADER_STAGE_VERTEX_BIT,
                    .module = vertexShader,
                    .pName = "main",
                    .pSpecializationInfo = nullptr,
            },
            VkPipelineShaderStageCreateInfo{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .pNext = nullptr,
                    .flags = 0,
                    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .module = fragmentShader,
                    .pName = "main",
                    .pSpecializationInfo = nullptr
            }
    };

    VkGraphicsPipelineCreateInfo pipelineCreateInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stageCount = shaderStages.size(),
            .pStages = shaderStages.data(),
            .pVertexInputState = &vertexInput,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterization,
            .pMultisampleState = &multisampleStateCreateInfo,
            .pDepthStencilState = &depthStencilState,
            .pColorBlendState = &colorBlend,
            .pDynamicState = &dynamicState,
            .layout = context.pipelineLayout,
            .renderPass = context.renderPass
    };

    CALL_VK(vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo,
                                      nullptr, &context.pipeline))

    vkDestroyShaderModule(context.device, vertexShader, nullptr);
    vkDestroyShaderModule(context.device, fragmentShader, nullptr);
}

void TriangleApp::initFramebuffers() {
    // Create framebuffer for each swapchain image view
    for (const auto &imageView: context.swapchainImageViews) {
        // Build the framebuffer
        VkFramebufferCreateInfo framebufferCreateInfo{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .renderPass = context.renderPass,
                .attachmentCount = 1,
                .pAttachments = &imageView,
                .width = context.swapchainDimensions.extent.width,
                .height = context.swapchainDimensions.extent.height,
                .layers = 1
        };

        VkFramebuffer framebuffer;
        CALL_VK(vkCreateFramebuffer(context.device, &framebufferCreateInfo, nullptr, &framebuffer))

        context.swapchainFramebuffers.emplace_back(framebuffer);
    }
}

void TriangleApp::teardownFramebuffers() {
    // Wait until device is idle before teardown
    vkQueueWaitIdle(context.queue);

    for (const auto &framebuffer: context.swapchainFramebuffers) {
        vkDestroyFramebuffer(context.device, framebuffer, nullptr);
    }

    context.swapchainFramebuffers.clear();
}

void TriangleApp::initVertexBuffers() {
    constexpr Vertex vertexData[] = {
            {.position {-100.0f, -20.0f}, .color {1.0f, 1.0f, 0.0f, 1.0f}},
            {.position {100.0f, -60.0f}, .color {1.0f, 0.0f, 1.0f, 1.0f}},
            {.position {30.0f, 100.0f}, .color {0.0f, 1.0f, 1.0f, 1.0f}},
            {.position {-170.0f, 140.0f}, .color {1.0f, 1.0f, 1.0f, 1.0f}},
    };
    constexpr VkDeviceSize bufferSize = sizeof(vertexData);
    VkDeviceMemory deviceMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 context.vertexBuffer, deviceMemory);

    void *data;
    vkMapMemory(context.device, deviceMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertexData, bufferSize);
    vkUnmapMemory(context.device, deviceMemory);
}

void TriangleApp::initIndexBuffers() {
    constexpr uint16_t indices[] = {
            0, 1, 2,
            2, 3, 0
    };
    constexpr VkDeviceSize bufferSize = sizeof(indices);
    VkDeviceMemory deviceMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 context.indexBuffer, deviceMemory);

    void *data;
    vkMapMemory(context.device, deviceMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices, bufferSize);
    vkUnmapMemory(context.device, deviceMemory);
}

void TriangleApp::initUniformBuffers() {
    constexpr VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    VkDeviceMemory deviceMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 context.uniformBuffer, deviceMemory);
}

void TriangleApp::initDescriptorPool() {
    VkDescriptorPoolSize poolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
    };

    VkDescriptorPoolCreateInfo poolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize
    };

    CALL_VK(vkCreateDescriptorPool(context.device, &poolCreateInfo, nullptr,
                                   &context.descriptorPool))
}

void TriangleApp::initDescriptorSets() {
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = context.descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &context.descriptorSetLayout
    };

    CALL_VK(vkAllocateDescriptorSets(context.device, &descriptorSetAllocateInfo,
                                     &context.descriptorSet))
}

/**
 * @brief Initializes per frame data
 * @param perFrame The data of a frame
 */
void TriangleApp::initPerFrame(TriangleApp::PerFrameData &perFrame) const {
    VkFenceCreateInfo fenceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    CALL_VK(vkCreateFence(context.device, &fenceCreateInfo, nullptr, &perFrame.queueSubmitFence))

    VkCommandPoolCreateInfo commandPoolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
            .queueFamilyIndex = context.graphicsQueueIndex.value()
    };

    CALL_VK(vkCreateCommandPool(context.device, &commandPoolCreateInfo, nullptr,
                                &perFrame.primaryCommandPool))

    VkCommandBufferAllocateInfo commandBufferAllocateInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = perFrame.primaryCommandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
    };
    CALL_VK(vkAllocateCommandBuffers(context.device, &commandBufferAllocateInfo,
                                     &perFrame.primaryCommandBuffer))

    perFrame.device = context.device;
}

void TriangleApp::teardownPerFrame(TriangleApp::PerFrameData &perFrame) const {
    if (perFrame.queueSubmitFence != VK_NULL_HANDLE) {
        vkDestroyFence(context.device, perFrame.queueSubmitFence, nullptr);
        perFrame.queueSubmitFence = VK_NULL_HANDLE;
    }

    if (perFrame.primaryCommandBuffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(context.device, perFrame.primaryCommandPool, 1,
                             &perFrame.primaryCommandBuffer);
        perFrame.primaryCommandBuffer = VK_NULL_HANDLE;
    }

    if (perFrame.primaryCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context.device, perFrame.primaryCommandPool, nullptr);
        perFrame.primaryCommandPool = VK_NULL_HANDLE;
    }

    if (perFrame.swapchainAcquireSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(context.device, perFrame.swapchainAcquireSemaphore, nullptr);
        perFrame.swapchainAcquireSemaphore = VK_NULL_HANDLE;
    }

    if (perFrame.swapchainReleaseSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(context.device, perFrame.swapchainReleaseSemaphore, nullptr);
        perFrame.swapchainReleaseSemaphore = VK_NULL_HANDLE;
    }

    perFrame.device = VK_NULL_HANDLE;
}

/**
 * @brief Renders a triangle to the specified swapchain image
 * @param swapchainIndex The swapchain index for the image being rendered.
 */
void TriangleApp::renderTriangle(uint32_t swapchainIndex) {
    VkFramebuffer framebuffer = context.swapchainFramebuffers.at(swapchainIndex);

    VkCommandBuffer commandBuffer = context.perFrame.at(swapchainIndex).primaryCommandBuffer;

    VkCommandBufferBeginInfo commandBufferBeginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
            .pInheritanceInfo = nullptr,
    };
    vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

    VkClearValue clearValue{
            .color {
                    .float32 {0.01f, 0.01f, 0.033f, 1.0f}
            },
    };

    // Begin render pass
    VkRenderPassBeginInfo renderPassBeginInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = context.renderPass,
            .framebuffer = framebuffer,
            .renderArea {
                    .offset {.x = 0, .y = 0},
                    .extent = context.swapchainDimensions.extent,
            },
            .clearValueCount = 1,
            .pClearValues = &clearValue
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    updateVertexBuffer(commandBuffer);
    updateUniformBuffer(commandBuffer);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context.pipeline);

    VkViewport viewport{
            .x = 0,
            .y = 0,
            .width = static_cast<float>(context.swapchainDimensions.extent.width),
            .height = static_cast<float>(context.swapchainDimensions.extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
    };
    // Set viewport dynamically
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{
            .offset {.x = 0, .y = 0},
            .extent {
                    .width = context.swapchainDimensions.extent.width,
                    .height = context.swapchainDimensions.extent.height
            }
    };
    // Set scissor dynamically
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &context.vertexBuffer,
                           &offset);
    vkCmdBindIndexBuffer(commandBuffer, context.indexBuffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context.pipelineLayout,
                            0, 1, &context.descriptorSet, 0, nullptr);
    vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    CALL_VK(vkEndCommandBuffer(commandBuffer))

    // Submit it to the queue with a release semaphore.
    if (context.perFrame.at(swapchainIndex).swapchainReleaseSemaphore == VK_NULL_HANDLE) {
        VkSemaphoreCreateInfo semaphoreCreateInfo{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };
        CALL_VK(vkCreateSemaphore(context.device, &semaphoreCreateInfo, nullptr,
                                  &context.perFrame.at(swapchainIndex).swapchainReleaseSemaphore))
    }

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &context.perFrame.at(swapchainIndex).swapchainAcquireSemaphore,
            .pWaitDstStageMask = &waitStage,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &context.perFrame.at(swapchainIndex).swapchainReleaseSemaphore
    };
    CALL_VK(vkQueueSubmit(context.queue, 1, &submitInfo,
                          context.perFrame.at(swapchainIndex).queueSubmitFence))
}

/**
 * @brief Acquires an image from the swapchain
 * @param[out] image
 */
VkResult TriangleApp::acquireNextImage(uint32_t *image) {
    VkSemaphore acquireSemaphore;
    if (context.recycledSemaphores.empty()) {
        VkSemaphoreCreateInfo semaphoreCreateInfo{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        CALL_VK(vkCreateSemaphore(context.device, &semaphoreCreateInfo, nullptr, &acquireSemaphore))
    } else {
        acquireSemaphore = context.recycledSemaphores.back();
        context.recycledSemaphores.pop_back();
    }

    VkResult result = vkAcquireNextImageKHR(context.device, context.swapchain,
                                            std::numeric_limits<uint64_t>::max(), acquireSemaphore,
                                            VK_NULL_HANDLE, image);

    if (result != VK_SUCCESS) {
        context.recycledSemaphores.emplace_back(acquireSemaphore);
        return result;
    }

    // If we have outstanding fences for this swapchain image, wait for them to complete first.
    // After begin frame returns, it is safe to reuse or delete resources which
    // were used previously.
    //
    // We wait for fences which completes N frames earlier, so we do not stall,
    // waiting for all GPU work to complete before this returns.
    // Normally, this doesn't really block at all,
    // since we're waiting for old frames to have been completed, but just in case.
    if (context.perFrame[*image].queueSubmitFence != VK_NULL_HANDLE) {
        vkWaitForFences(context.device, 1, &context.perFrame[*image].queueSubmitFence, true,
                        std::numeric_limits<uint64_t>::max());
        vkResetFences(context.device, 1, &context.perFrame[*image].queueSubmitFence);
    }

    if (context.perFrame[*image].primaryCommandPool != VK_NULL_HANDLE) {
        vkResetCommandPool(context.device, context.perFrame[*image].primaryCommandPool, 0);
    }

    VkSemaphore oldSemaphore = context.perFrame[*image].swapchainAcquireSemaphore;

    if (oldSemaphore != VK_NULL_HANDLE) {
        context.recycledSemaphores.emplace_back(oldSemaphore);
    }

    context.perFrame[*image].swapchainAcquireSemaphore = acquireSemaphore;

    return VK_SUCCESS;
}

/**
 * @brief Presents an image to the swapchain
 */
VkResult TriangleApp::presentImage(uint32_t index) {
    VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &context.perFrame.at(index).swapchainReleaseSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &context.swapchain,
            .pImageIndices = &index,
            .pResults = nullptr
    };
    return vkQueuePresentKHR(context.queue, &presentInfo);
}

void TriangleApp::updateVertexBuffer(const VkCommandBuffer &commandBuffer) const {
//    Vertex vertexData[] = {
//            {.position {-0.5f, 0.5f}, .color {1.0f, 1.0f, 0.0f, 1.0f}},
//            {.position {-0.5f, -0.5f}, .color {1.0f, 0.0f, 1.0f, 1.0f}},
//            {.position {0.5f, -0.5f}, .color {0.0f, 1.0f, 1.0f, 1.0f}},
//            {.position {0.5f, 0.5f}, .color {1.0f, 1.0f, 1.0f, 1.0f}},
//    };
//
//    vkCmdUpdateBuffer(commandBuffer, context.vertexBuffer, 0, sizeof(vertexData), vertexData);
}

void TriangleApp::updateUniformBuffer(VkCommandBuffer const &commandBuffer) const {
    using namespace std::chrono;
    const auto timestamp = static_cast<double>(duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count()) / 1000.0;

    constexpr float pulseRate = 1.5f;
    const float scaleFactor = 1.0f + 0.5f * std::cos(pulseRate * timestamp);
    const glm::vec2 scale{scaleFactor, scaleFactor};
    const glm::mat4x4 scaleMatrix = math_utils::scale2D(scale);

    constexpr float rotationRate = 2.5f;
    const float rotationAngle = rotationRate * static_cast<float>(timestamp);
    const glm::mat4x4 rotationMatrix = math_utils::rotateZ(rotationAngle);

    constexpr float orbitalRadius = 200.0f;
    const glm::vec2 translation =
            orbitalRadius * glm::vec2{std::cos(timestamp), std::sin(timestamp)};
    const glm::mat4x4 translationMatrix = math_utils::translate2D(translation);

    const glm::mat4x4 modelMatrix = translationMatrix * rotationMatrix * scaleMatrix;

    const float aspectRatio =
            static_cast<float>(context.swapchainDimensions.extent.width) /
            static_cast<float>(context.swapchainDimensions.extent.height);
    constexpr float canvasWidth = 800.0f;
    const float canvasHeight = canvasWidth / aspectRatio;
    const glm::mat4x4 projectionMatrix = math_utils::orthographicProjection(-canvasWidth / 2,
                                                                            canvasHeight / 2,
                                                                            canvasWidth / 2,
                                                                            -canvasHeight / 2, 0.0,
                                                                            1.0);

    const UniformBufferObject ubo{
            .modelMatrix = modelMatrix,
            .projectionMatrix = projectionMatrix
    };

    vkCmdUpdateBuffer(commandBuffer, context.uniformBuffer, 0, sizeof(ubo), &ubo);

    VkDescriptorBufferInfo descriptorBufferInfo{
            .buffer = context.uniformBuffer,
            .offset = 0,
            .range = sizeof(ubo)
    };

    VkWriteDescriptorSet descriptorWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = context.descriptorSet,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &descriptorBufferInfo,
            .pTexelBufferView = nullptr,
    };

    vkUpdateDescriptorSets(context.device, 1, &descriptorWrite, 0, nullptr);
}

void TriangleApp::createBuffer(VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsageFlags,
                               VkSharingMode sharingMode, VkFlags requirementsMask,
                               VkBuffer &rBuffer, VkDeviceMemory &rDeviceMemory) {
    VkBufferCreateInfo bufferCreateInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = bufferSize,
            .usage = bufferUsageFlags,
            .sharingMode = sharingMode,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &context.graphicsQueueIndex.value()
    };

    CALL_VK(vkCreateBuffer(context.device, &bufferCreateInfo, nullptr, &rBuffer))

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(context.device, rBuffer, &memoryRequirements);

    uint32_t memoryTypeIndex;
    vulkan_common::mapMemoryTypeToIndex(context.gpu, memoryRequirements.memoryTypeBits,
                                        requirementsMask, &memoryTypeIndex);

    VkMemoryAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = memoryTypeIndex
    };

    CALL_VK(vkAllocateMemory(context.device, &allocateInfo, nullptr, &rDeviceMemory))

    CALL_VK(vkBindBufferMemory(context.device, rBuffer, rDeviceMemory, 0))
}


bool TriangleApp::validateExtensions(const std::vector<const char *> &requiredExtensions,
                                     const std::vector<VkExtensionProperties> &availableExtensions) {
    for (const char *required: requiredExtensions) {
        bool found = false;
        for (const VkExtensionProperties &availableExtension: availableExtensions) {
            if (strcmp(required, availableExtension.extensionName) == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            return false;
        }
    }

    return true;
}
