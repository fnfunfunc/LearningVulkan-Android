//
// Created by eternal on 2024/4/26.
//

#ifndef LEARNINGVULKAN_VULKANMAIN_HH
#define LEARNINGVULKAN_VULKANMAIN_HH

#include <game-activity/native_app_glue/android_native_app_glue.h>

// Initialize vulkan device context, after run, vulkan is ready to draw
bool InitVulkan(android_app *pApp);

// Delete vulkan device context when application goes away
void DeleteVulkan();

// Check if vulkan is ready to draw
bool IsVulkanReady();

// Ask vulkan to render a frame
bool VulkanDrawFrame();

#endif //LEARNINGVULKAN_VULKANMAIN_HH
