//
// Created by eternal on 2024/4/29.
//
#include <array>
#include <vector>
#include "Debug.hh"
#include "TriangleApp.hh"
#include "VulkanUtils.hh"

TriangleApp::TriangleApp(android_app *pApp) noexcept: _androidAppCtx(pApp) {
}

void TriangleApp::run() {
    _initVulkan();
}

bool TriangleApp::isReady() const {
    return _device.initialized;
}

void TriangleApp::release() {
    _deleteVulkan();
}

bool TriangleApp::drawFrame() {
    uint32_t nextIndex;
    CALL_VK(vkAcquireNextImageKHR(_device.device, _swapchain.swapchain,
                                  std::numeric_limits<uint64_t>::max(), _render.semaphore,
                                  VK_NULL_HANDLE, &nextIndex))

    CALL_VK(vkResetFences(_device.device, 1, &_render.fence))

    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &_render.semaphore,
            .pWaitDstStageMask = &waitStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = &_render.commandBuffers.at(nextIndex),
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr,
    };
    CALL_VK(vkQueueSubmit(_device.queue, 1, &submitInfo, _render.fence))
    CALL_VK(vkWaitForFences(_device.device, 1, &_render.fence, VK_TRUE, 100'000'000))

    LOGI("Drawing frames");

    VkResult result;
    VkPresentInfoKHR presentInfo {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .swapchainCount = 1,
            .pSwapchains = &_swapchain.swapchain,
            .pImageIndices = &nextIndex,
            .pResults = &result
    };
    vkQueuePresentKHR(_device.queue, &presentInfo);
    return true;
}

bool TriangleApp::_initVulkan() {
    if (!InitVulkan()) {
        LOGW("Vulkan is unavailable, install vulkan and re-start");
        return false;
    }

    VkApplicationInfo applicationInfo{
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = "Triangle App",
            .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .pEngineName = "LearningVulkan",
            .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
            .apiVersion = VK_MAKE_API_VERSION(0, 1, 1, 0)
    };

    _createVulkanDevice(_androidAppCtx->window, &applicationInfo);

    _createSwapchain();

    _createRenderPass();

    _createFrameBuffers(_render.renderPass);

    _createBuffers();

    _createGraphicsPipeline();

    _createCommands();

    // Create a fence to be able to wait for draw commands to finish in the main loop before swapping the framebuffers
    VkFenceCreateInfo fenceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
    };
    CALL_VK(vkCreateFence(_device.device, &fenceCreateInfo, nullptr, &_render.fence))

    // Create a semaphore to wait for framebuffer to be available before drawing
    VkSemaphoreCreateInfo semaphoreCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
    };
    CALL_VK(vkCreateSemaphore(_device.device, &semaphoreCreateInfo, nullptr, &_render.semaphore))

    _device.initialized = true;
    return true;
}

void TriangleApp::_deleteVulkan() {
    vkFreeCommandBuffers(_device.device, _render.commandPool, _render.commandBuffers.size(),
                         _render.commandBuffers.data());
    _render.commandBuffers.clear();

    vkDestroyCommandPool(_device.device, _render.commandPool, nullptr);
    vkDestroyRenderPass(_device.device, _render.renderPass, nullptr);

    _deleteSwapchain();
    _deleteGraphicsPipeline();
    _deleteBuffers();

    vkDestroyDevice(_device.device, nullptr);
    vkDestroyInstance(_device.instance, nullptr);

    _device.initialized = false;
}

void TriangleApp::_createVulkanDevice(ANativeWindow *platformWindow,
                                      VkApplicationInfo *applicationInfo) {
    std::array<const char *, 2> instanceExtensions{VK_KHR_SURFACE_EXTENSION_NAME,
                                                   VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
    std::array<const char *, 1> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};


    VkInstanceCreateInfo instanceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .pApplicationInfo = applicationInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = instanceExtensions.size(),
            .ppEnabledExtensionNames = instanceExtensions.data(),
    };

    CALL_VK(vkCreateInstance(&instanceCreateInfo, nullptr, &_device.instance))

    VkAndroidSurfaceCreateInfoKHR androidSurfaceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .window = platformWindow
    };

    CALL_VK(vkCreateAndroidSurfaceKHR(_device.instance, &androidSurfaceCreateInfo, nullptr,
                                      &_device.surface))

    // Find one GPU to use
    uint32_t gpuCount = 0;
    CALL_VK(vkEnumeratePhysicalDevices(_device.instance, &gpuCount, nullptr))
    assert(gpuCount > 0);
    std::vector<VkPhysicalDevice> allGpus(gpuCount);
    CALL_VK(vkEnumeratePhysicalDevices(_device.instance, &gpuCount, allGpus.data()))
    _device.gpuDevice = allGpus.front();

    // Find a GFX queue family
    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(_device.gpuDevice, &queueFamilyCount, nullptr);
    assert(queueFamilyCount > 0);
    std::vector<VkQueueFamilyProperties> queueProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(_device.gpuDevice, &queueFamilyCount,
                                             queueProperties.data());


    const uint32_t selectedQueueFamilyIndex = std::distance(queueProperties.begin(),
                                                            std::find_if(queueProperties.begin(),
                                                                         queueProperties.end(),
                                                                         [](const auto &queueFamilyProperty) {
                                                                             return (queueFamilyProperty.queueFlags &
                                                                                     VK_QUEUE_GRAPHICS_BIT) !=
                                                                                    0;
                                                                         }));
    assert(selectedQueueFamilyIndex < queueFamilyCount);
    _device.queueFamilyIndex = selectedQueueFamilyIndex;

    // Specify the priority of queue
    const float priorities[] = {
            1.0f
    };
    VkDeviceQueueCreateInfo queueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = _device.queueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = priorities
    };
    VkDeviceCreateInfo deviceCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = deviceExtensions.size(),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures = nullptr
    };

    CALL_VK(vkCreateDevice(_device.gpuDevice, &deviceCreateInfo, nullptr, &_device.device))
    vkGetDeviceQueue(_device.device, _device.queueFamilyIndex, 0, &_device.queue);
}

void TriangleApp::_createSwapchain() {
    memset(&_swapchain, 0, sizeof(_swapchain));

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    CALL_VK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_device.gpuDevice, _device.surface,
                                                      &surfaceCapabilities))

    uint32_t formatCount;
    CALL_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(_device.gpuDevice, _device.surface, &formatCount,
                                                 nullptr))
    assert(formatCount > 0);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    CALL_VK(vkGetPhysicalDeviceSurfaceFormatsKHR(_device.gpuDevice, _device.surface, &formatCount,
                                                 surfaceFormats.data()))

    LOGI("%s: Got %d formats", __FUNCTION__, formatCount);

    const auto chosenFormatIter = std::find_if(surfaceFormats.cbegin(), surfaceFormats.cend(),
                                               [](const VkSurfaceFormatKHR &format) {
                                                   return format.format == VK_FORMAT_R8G8B8A8_UNORM;
                                               });
    assert(chosenFormatIter != surfaceFormats.cend());

    _swapchain.displaySize = surfaceCapabilities.currentExtent;
    _swapchain.displayFormat = chosenFormatIter->format;

    VkSwapchainCreateInfoKHR swapchainCreateInfo{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = _device.surface,
            .minImageCount = surfaceCapabilities.minImageCount,
            .imageFormat = chosenFormatIter->format,
            .imageColorSpace = chosenFormatIter->colorSpace,
            .imageExtent = surfaceCapabilities.currentExtent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &_device.queueFamilyIndex,
            .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = VK_PRESENT_MODE_FIFO_KHR,
            .clipped = VK_FALSE,
            .oldSwapchain = VK_NULL_HANDLE,
    };

    CALL_VK(vkCreateSwapchainKHR(_device.device, &swapchainCreateInfo, nullptr,
                                 &_swapchain.swapchain))

    // Get the length of the created swapchain
    CALL_VK(vkGetSwapchainImagesKHR(_device.device, _swapchain.swapchain,
                                    &_swapchain.swapchainLength, nullptr))
}

void TriangleApp::_createRenderPass() {
    VkAttachmentDescription attachmentDescription{
            .flags = 0,
            .format = _swapchain.displayFormat,
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
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
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
            .pPreserveAttachments = nullptr,
    };

    VkRenderPassCreateInfo renderPassCreateInfo{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .attachmentCount = 1,
            .pAttachments = &attachmentDescription,
            .subpassCount = 1,
            .pSubpasses = &subpassDescription,
            .dependencyCount = 0,
            .pDependencies = nullptr,
    };

    CALL_VK(vkCreateRenderPass(_device.device, &renderPassCreateInfo, nullptr, &_render.renderPass))
}

void TriangleApp::_createFrameBuffers(const VkRenderPass &renderPass, VkImageView depthView) {
    _swapchain.displayImages.resize(_swapchain.swapchainLength);
    CALL_VK(vkGetSwapchainImagesKHR(_device.device, _swapchain.swapchain,
                                    &_swapchain.swapchainLength, _swapchain.displayImages.data()))

    _swapchain.displayViews.resize(_swapchain.swapchainLength);
    for (uint32_t i = 0; i < _swapchain.swapchainLength; ++i) {
        VkImageViewCreateInfo viewCreateInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .image = _swapchain.displayImages.at(i),
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = _swapchain.displayFormat,
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
                        .layerCount = 1
                }
        };

        CALL_VK(vkCreateImageView(_device.device, &viewCreateInfo, nullptr,
                                  &_swapchain.displayViews.at(i)))
    }

    _swapchain.framebuffers.resize(_swapchain.swapchainLength);
    for (uint32_t i = 0; i < _swapchain.swapchainLength; ++i) {
        std::vector<VkImageView> attachments{_swapchain.displayViews.at(i)};
        if (depthView != VK_NULL_HANDLE) {
            attachments.emplace_back(depthView);
        }
        VkFramebufferCreateInfo framebufferCreateInfo{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .renderPass = renderPass,
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments = attachments.data(),
                .width = _swapchain.displaySize.width,
                .height = _swapchain.displaySize.height,
                .layers = 1,
        };
        CALL_VK(vkCreateFramebuffer(_device.device, &framebufferCreateInfo, nullptr,
                                    &_swapchain.framebuffers.at(i)))
    }
}

void TriangleApp::_createBuffers() {
    const float vertexData[] = {
            -1.0f, -1.0f, 0.0f,
            1.0f, -1.0f, 0.0f,
            0.0f, 1.0f, 0.0f
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
            .pQueueFamilyIndices = &_device.queueFamilyIndex
    };

    CALL_VK(vkCreateBuffer(_device.device, &bufferCreateInfo, nullptr, &_buffer.vertexBuffer))

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(_device.device, _buffer.vertexBuffer, &memoryRequirements);

    uint32_t memoryTypeIndex;
    vulkan_utils::mapMemoryTypeToIndex(_device.gpuDevice, memoryRequirements.memoryTypeBits,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memoryTypeIndex);

    VkMemoryAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memoryRequirements.size,
            .memoryTypeIndex = memoryTypeIndex
    };

    VkDeviceMemory deviceMemory;
    vkAllocateMemory(_device.device, &allocateInfo, nullptr, &deviceMemory);

    void *data;
    CALL_VK(vkMapMemory(_device.device, deviceMemory, 0, allocateInfo.allocationSize, 0, &data))

    memcpy(data, vertexData, sizeof(vertexData));
    vkUnmapMemory(_device.device, deviceMemory);

    CALL_VK(vkBindBufferMemory(_device.device, _buffer.vertexBuffer, deviceMemory, 0))
}

VkResult TriangleApp::_createGraphicsPipeline() {
    memset(&_gfxPipelineInfo, 0, sizeof(_gfxPipelineInfo));
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .setLayoutCount = 0,
            .pSetLayouts = nullptr,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr
    };

    CALL_VK(vkCreatePipelineLayout(_device.device, &pipelineLayoutCreateInfo, nullptr,
                                   &_gfxPipelineInfo.layout))

    VkShaderModule vertexShader, fragmentShader;
    vulkan_utils::loadShaderFromFile(_androidAppCtx, _device.device, "shaders/triangle.vert.spv",
                                     &vertexShader);
    vulkan_utils::loadShaderFromFile(_androidAppCtx, _device.device, "shaders/triangle.frag.spv",
                                     &fragmentShader);

    // Specify vertex and fragment shader stages
    VkPipelineShaderStageCreateInfo shaderStages[]{
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
            .width = static_cast<float>(_swapchain.displaySize.width),
            .height = static_cast<float>(_swapchain.displaySize.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
    };

    VkRect2D scissor{
            .offset {.x = 0, .y = 0},
            .extent {_swapchain.displaySize}
    };

    // Specify viewport info
    VkPipelineViewportStateCreateInfo viewportInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
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
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 0,
            .pSampleMask = &sampleMask,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE
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
            .pAttachments = &attachmentState,
            .blendConstants{}
    };

    // Specify rasterizer info
    VkPipelineRasterizationStateCreateInfo rasterInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
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
            .flags = 0,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE
    };


    // Specify vertex input state
    VkVertexInputBindingDescription vertexInputBindingDescription{
            .binding = 0,
            .stride = 3 * sizeof(float),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
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
            .flags = 0,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &vertexInputBindingDescription,
            .vertexAttributeDescriptionCount = 1,
            .pVertexAttributeDescriptions = &vertexInputAttributeDescription,
    };

    // Create pipeline cache
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .initialDataSize = 0,
            .pInitialData = nullptr
    };
    CALL_VK(vkCreatePipelineCache(_device.device, &pipelineCacheCreateInfo, nullptr,
                                  &_gfxPipelineInfo.cache))

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
            .layout = _gfxPipelineInfo.layout,
            .renderPass = _render.renderPass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = 0
    };

    VkResult pipelineResult = vkCreateGraphicsPipelines(_device.device, _gfxPipelineInfo.cache, 1,
                                                        &graphicsPipelineCreateInfo, nullptr,
                                                        &_gfxPipelineInfo.pipeline);

    // Release the shader
    vkDestroyShaderModule(_device.device, vertexShader, nullptr);
    vkDestroyShaderModule(_device.device, fragmentShader, nullptr);

    return pipelineResult;
}

void TriangleApp::_createCommands() {
    // Create a pool of command buffers to allocate command buffer
    VkCommandPoolCreateInfo commandPoolCreateInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = _device.queueFamilyIndex
    };
    CALL_VK(vkCreateCommandPool(_device.device, &commandPoolCreateInfo, nullptr,
                                &_render.commandPool))

    _render.commandBuffers = std::vector<VkCommandBuffer>(_swapchain.swapchainLength);
    for (uint32_t bufferIndex = 0; bufferIndex < _swapchain.swapchainLength; ++bufferIndex) {
        VkCommandBufferAllocateInfo commandBufferAllocateInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext = nullptr,
                .commandPool = _render.commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
        };
        CALL_VK(vkAllocateCommandBuffers(_device.device, &commandBufferAllocateInfo,
                                         &_render.commandBuffers.at(bufferIndex)))

        VkCommandBufferBeginInfo commandBufferBeginInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = nullptr,
                .flags = 0,
                .pInheritanceInfo = nullptr
        };
        CALL_VK(vkBeginCommandBuffer(_render.commandBuffers.at(bufferIndex),
                                     &commandBufferBeginInfo))

        // Transition the display image to color attachment layout
        vulkan_utils::setImageLayout(_render.commandBuffers.at(bufferIndex),
                                     _swapchain.displayImages.at(bufferIndex),
                                     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        // Start a renderpass. Any draw command has to be recorded in a renderpass
        const VkClearValue clearValue{
                .color {
                        .float32 {0.90f, 0.10f, 0.10f, 1.0f}
                }
        };
        VkRenderPassBeginInfo renderPassBeginInfo{
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .pNext = nullptr,
                .renderPass = _render.renderPass,
                .framebuffer = _swapchain.framebuffers.at(bufferIndex),
                .renderArea {.offset = {.x = 0, .y = 0}, .extent {_swapchain.displaySize}},
                .clearValueCount = 1,
                .pClearValues = &clearValue
        };
        vkCmdBeginRenderPass(_render.commandBuffers.at(bufferIndex), &renderPassBeginInfo,
                             VK_SUBPASS_CONTENTS_INLINE);

        // Bind pipeline and vertex buffers
        vkCmdBindPipeline(_render.commandBuffers.at(bufferIndex), VK_PIPELINE_BIND_POINT_GRAPHICS,
                          _gfxPipelineInfo.pipeline);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(_render.commandBuffers.at(bufferIndex), 0, 1, &_buffer.vertexBuffer,
                               &offset);

        // Draw triangle
        vkCmdDraw(_render.commandBuffers.at(bufferIndex), 3, 1, 0, 0);

        vkCmdEndRenderPass(_render.commandBuffers.at(bufferIndex));

        // Transition back to swapchain image to PRESENT_SRC_KHR
        vulkan_utils::setImageLayout(_render.commandBuffers.at(bufferIndex),
                                     _swapchain.displayImages.at(bufferIndex),
                                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                     VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        CALL_VK(vkEndCommandBuffer(_render.commandBuffers.at(bufferIndex)))
    }
}

void TriangleApp::_deleteSwapchain() {
    for (uint32_t i = 0; i < _swapchain.swapchainLength; ++i) {
        vkDestroyFramebuffer(_device.device, _swapchain.framebuffers.at(i), nullptr);
        vkDestroyImageView(_device.device, _swapchain.displayViews.at(i), nullptr);
    }
    vkDestroySwapchainKHR(_device.device, _swapchain.swapchain, nullptr);
}

void TriangleApp::_deleteGraphicsPipeline() const {
    if (_gfxPipelineInfo.pipeline == VK_NULL_HANDLE) {
        return;
    }
    vkDestroyPipeline(_device.device, _gfxPipelineInfo.pipeline, nullptr);
    vkDestroyPipelineCache(_device.device, _gfxPipelineInfo.cache, nullptr);
    vkDestroyPipelineLayout(_device.device, _gfxPipelineInfo.layout, nullptr);
}

void TriangleApp::_deleteBuffers() const {
    vkDestroyBuffer(_device.device, _buffer.vertexBuffer, nullptr);
}
