#include <android/log.h>
#include <cassert>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <vector>
#include "VulkanMain.hh"

void handleCmd(android_app *pApp, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            InitVulkan(pApp);
            break;
        case APP_CMD_TERM_WINDOW:
            DeleteVulkan();
            break;
        default:
            __android_log_print(ANDROID_LOG_INFO, "Learning Vulkan", "event not handled: %d", cmd);
    }
}


void android_main(android_app *pApp) {
    pApp->onAppCmd = handleCmd;

    int events;
    android_poll_source *source;

    do {
        if (ALooper_pollAll(IsVulkanReady() ? 1 : 0, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
            if (source != nullptr) {
                source->process(pApp, source);
            }
        }

        // render if vulkan is ready
        if (IsVulkanReady()) {
            VulkanDrawFrame();
        }
    } while (pApp->destroyRequested == 0);
}