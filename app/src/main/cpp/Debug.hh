//
// Created by eternal on 2024/4/29.
//

#ifndef LEARNINGVULKAN_DEBUG_HH
#define LEARNINGVULKAN_DEBUG_HH
#include <android/log.h>
#include <cassert>

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

#endif //LEARNINGVULKAN_DEBUG_HH
