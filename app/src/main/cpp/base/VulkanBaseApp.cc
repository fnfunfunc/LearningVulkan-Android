//
// Created by eternal on 2024/4/30.
//

#include "VulkanBaseApp.hh"

VulkanBaseApp::VulkanBaseApp() : name{"Sample name"} {

}

bool VulkanBaseApp::prepare(ANativeWindow *window) {
    return false;
}

void VulkanBaseApp::update(float deltaTime) {

}

void VulkanBaseApp::updateOverlay(float deltaTime, const std::function<void()> &additional_ui) {

}

void VulkanBaseApp::finish() {

}

bool VulkanBaseApp::isReady() const {
    return isReady_;
}

const std::string &VulkanBaseApp::getName() const {
    return name;
}

