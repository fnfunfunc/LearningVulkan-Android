//
// Created by eternal on 2024/4/30.
//

#ifndef LEARNINGVULKAN_VULKANBASEAPP_HH
#define LEARNINGVULKAN_VULKANBASEAPP_HH
#include <string>
#include <vector>
#include "vulkan_wrapper.hh"

class VulkanBaseApp {
public:
    VulkanBaseApp();

    virtual ~VulkanBaseApp() = default;

    virtual bool prepare(ANativeWindow* window);

    virtual void update(float deltaTime);

    virtual void updateOverlay(float deltaTime, const std::function<void()> &additional_ui = [](){});

    virtual void finish();

    const std::string &getName() const;

    bool isReady() const;
protected:
    bool isReady_ = false;
private:
    std::string name{};

    std::vector<std::pair<VkShaderStageFlagBits, std::string>> availableShaders;
};

#endif //LEARNINGVULKAN_VULKANBASEAPP_HH
