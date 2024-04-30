#include <android/log.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include "TriangleApp.hh"

void handleCmd(android_app *pApp, int32_t cmd) {
    auto *const triangleApp = reinterpret_cast<TriangleApp *>(pApp->userData);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (triangleApp != nullptr) {
                triangleApp->run();
            }
            break;
        case APP_CMD_TERM_WINDOW:
            if (triangleApp != nullptr) {
                triangleApp->release();
            }
            break;
        default:
            __android_log_print(ANDROID_LOG_INFO, "Learning Vulkan", "event not handled: %d", cmd);
    }
}


void android_main(android_app *pApp) {
    TriangleApp triangleApp{pApp};
    pApp->userData = &triangleApp;
    pApp->onAppCmd = handleCmd;

    int events;
    android_poll_source *source;

    do {
        if (ALooper_pollAll(triangleApp.isReady() ? 1 : 0, nullptr, &events,
                            reinterpret_cast<void **>(&source)) >= 0) {
            if (source != nullptr) {
                source->process(pApp, source);
            }
        }

        // render if vulkan is ready
        if (triangleApp.isReady()) {
            triangleApp.drawFrame();
        }
    } while (pApp->destroyRequested == 0);
}