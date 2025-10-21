#pragma once

#include "vk_descriptors.h"
#include "vk_types.h"
#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

struct DeletionQueue {
    std::deque<std::function<void()>> deletors;

    void pushFunction(std::function<void()>&& function) { deletors.push_back(function); }

    void flush() {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
            (*it)(); // call functors
        }

        deletors.clear();
    }
};

struct FrameData {
    VkCommandPool commandPool_;
    VkCommandBuffer mainCommandBuffer_;
    VkSemaphore swapchainSemaphore_;
    VkSemaphore renderSemaphore_;
    VkFence renderFence_;
    DeletionQueue deletionQueue_;
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

    DeletionQueue mainDeletionQueue_;

    VmaAllocator allocator_;

    AllocatedImage drawImage_;
    VkExtent2D drawExtent_;

    DescriptorAllocator globalDescriptorAllocator;
    VkDescriptorSet drawImageDescriptors_;
    VkDescriptorSetLayout drawImageDescriptorLayout_;

    VkPipeline gradientPipeline_;
    VkPipelineLayout gradientPipelineLayout_;

    VkFence immFence_;
    VkCommandBuffer immCommandBuffer_;
    VkCommandPool immCommandPool_;

    FrameData& get_current_frame() { return frames_[frameNumber_ % FRAME_OVERLAP]; };

    static VulkanEngine& get();

    void init();

    void cleanup();

    void draw();

    void run();

    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

  private:
    void initVulkan();

    void initSwapchain();

    void createSwapchain(uint32_t width, uint32_t height);

    void destroySwapchain();

    void initCommands();

    void initSyncStructures();

    void initDescriptors();

    void initPipelines();

    void initBackgroundPipelines();

    void initImgui();

    void drawBackground(VkCommandBuffer cmd);

    void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
};