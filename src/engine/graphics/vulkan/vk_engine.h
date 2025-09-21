#pragma once

#include "vk_types.h"
#include <cstdint>
#include <vector>


struct FrameData {

    VkCommandPool commandPool_;
    VkCommandBuffer mainCommandBuffer_;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
  public:
    bool isInitialized_ = false;
    int frameNumber_ = 0;
    bool stopRendering_ = false;
    VkExtent2D windowExtent_{1700, 900};
    struct SDL_Window* window_ = nullptr;

    VkInstance instance_;                     // library handle
    VkDebugUtilsMessengerEXT debugMessenger_; // debug stuff
    VkPhysicalDevice chosenGPU_;              // gpu chosen as default device
    VkDevice device_;                         // device for commands
    VkSurfaceKHR surface_;                    // window surface

    VkSwapchainKHR swapchain_;
    VkFormat swapchainImageFormat_;

    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkExtent2D swapchainExtent_;

    FrameData frames_[FRAME_OVERLAP];

    VkQueue graphicsQueue_;
    uint32_t graphicsQueueFamily_;

    FrameData& get_current_frame() { return frames_[frameNumber_ % FRAME_OVERLAP]; };

    static VulkanEngine& get();

    void init();

    void cleanup();

    void draw();

    void run();

  private:
    void initVulkan();

    void initSwapchain();

    void createSwapchain(uint32_t width, uint32_t height);

    void destroySwapchain();

    void initCommands();

    void initSyncStructures();
};