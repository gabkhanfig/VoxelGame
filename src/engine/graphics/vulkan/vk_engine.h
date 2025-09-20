#pragma once

#include "vk_types.h"

class VulkanEngine {
  public:
    bool isInitialized_ = false;
    int frameNumber_ = 0;
    bool stopRendering_ = false;
    VkExtent2D windowExtent_{1700, 900};
    struct SDL_Window* window_ = nullptr;

    static VulkanEngine& get();

    void init();

    void cleanup();

    void draw();

    void run();
};