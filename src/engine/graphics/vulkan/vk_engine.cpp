#include "vk_engine.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <assert.h>
#include <chrono>
#include <thread>

constexpr bool bUseValidationLayers = false;

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::get() { return *loadedEngine; }

void VulkanEngine::init() {
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = SDL_WINDOW_VULKAN;

    window_ = SDL_CreateWindow("Vulkan Engine", windowExtent_.width, windowExtent_.height, window_flags);

    // everything went fine
    isInitialized_ = true;
}

void VulkanEngine::cleanup() {
    if (isInitialized_) {

        SDL_DestroyWindow(window_);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::draw() {
    // nothing yet
}

void VulkanEngine::run() {
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stopRendering_ = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stopRendering_ = false;
                }
            }
        }

        // do not draw if we are minimized
        if (stopRendering_) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw();
    }
}
