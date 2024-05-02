# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.22.1)

project("learningvulkan")


include_directories(
        vulkan_wrapper
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
                     -DVK_USE_PLATFORM_ANDROID_KHR")

# Searches for a package provided by the game activity dependency
find_package(game-activity REQUIRED CONFIG)

get_target_property(game-activity-include
        game-activity::game-activity
        INTERFACE_INCLUDE_DIRECTORIES)

# Creates your game shared library. The name must be the same as the
# one used for loading in your Kotlin/Java or AndroidManifest.txt files.
file(GLOB_RECURSE baseFiles "base/*.cc")

add_library(learningvulkan SHARED
        main.cc
        HelloTriangle.cc
        TriangleApp.cc
        VulkanUtils.cc
        base/VulkanBaseApp.cc
        base/VulkanCommon.cc
        ${CMAKE_SOURCE_DIR}/vulkan_wrapper/vulkan_wrapper.cc
        ${game-activity-include}/game-activity/native_app_glue/android_native_app_glue.c
        ${game-activity-include}/game-activity/GameActivity.cpp
        ${game-activity-include}/game-text-input/gametextinput.cpp
)

include_directories(base)

# Configure libraries CMake uses to link your target library.
target_link_libraries(learningvulkan
        game-activity::game-activity
        android
        log)