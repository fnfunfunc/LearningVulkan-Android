#include <android/log.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include "samples/TriangleApp.hh"

void handleCmd(android_app *pApp, int32_t cmd) {
    auto *const pHelloTriangle = reinterpret_cast<TriangleApp *>(pApp->userData);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (pHelloTriangle != nullptr && !pHelloTriangle->isReady()) {
                pHelloTriangle->prepare(pApp->window);
            }
            break;
        case APP_CMD_DESTROY:
            delete pHelloTriangle;
            break;
        default:
            __android_log_print(ANDROID_LOG_INFO, "Learning Vulkan", "event not handled: %d", cmd);
    }
}


void android_main(android_app *pApp) {
    auto* pTriangleApp = new TriangleApp(pApp);
    pApp->userData = pTriangleApp;
    pApp->onAppCmd = handleCmd;

    int events;
    android_poll_source *source;

    do {
        if (ALooper_pollAll(pTriangleApp->isReady() ? 1 : 0, nullptr, &events,
                            reinterpret_cast<void **>(&source)) >= 0) {
            if (source != nullptr) {
                source->process(pApp, source);
            }
        }

        // render if vulkan is ready
        if (pTriangleApp->isReady()) {
            pTriangleApp->update(0.0f);
        }
    } while (pApp->destroyRequested == 0);
}