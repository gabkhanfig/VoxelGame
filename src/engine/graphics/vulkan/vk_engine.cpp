#include "vk_engine.h"
#include "vk_initializers.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <assert.h>
#include <chrono>
#include <iostream>
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

    initVulkan();
    initSwapchain();
    initCommands();
    initSyncStructures();

    // everything went fine
    isInitialized_ = true;
}

void VulkanEngine::cleanup() {
    if (isInitialized_) {
        vkDeviceWaitIdle(device_);
        for (unsigned int i = 0; i < FRAME_OVERLAP; i++) {
            vkDestroyCommandPool(device_, frames_[i].commandPool_, nullptr);
        }

        destroySwapchain();

        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyDevice(device_, nullptr);

        vkb::destroy_debug_utils_messenger(instance_, debugMessenger_);
        SDL_DestroyWindow(window_);

        vkDestroyInstance(instance_, nullptr);
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
            if (e.type == static_cast<decltype(e.type)>(SDL_EVENT_QUIT))
                bQuit = true;

            if (e.type == static_cast<decltype(e.type)>(SDL_EVENT_WINDOW_MINIMIZED)) {
                std::cout << "Minimized\n";
                stopRendering_ = true;
            }
            if (e.type == static_cast<decltype(e.type)>(SDL_EVENT_WINDOW_MAXIMIZED)) {
                std::cout << "Maximized\n";
                stopRendering_ = false;
            }
            if (e.type == static_cast<decltype(e.type)>(SDL_EVENT_WINDOW_RESTORED)) {
                std::cout << "Restored\n";
                stopRendering_ = false;
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

void VulkanEngine::initVulkan() {
    vkb::InstanceBuilder builder;

    auto instRet = builder.set_app_name("Example Vulkan Application")
                       .request_validation_layers(bUseValidationLayers)
                       .use_default_debug_messenger()
                       .require_api_version(1, 3, 0)
                       .build();

    vkb::Instance vkbInst = instRet.value();

    instance_ = vkbInst.instance;
    debugMessenger_ = vkbInst.debug_messenger;

    SDL_Vulkan_CreateSurface(window_, instance_, nullptr, &surface_);

    VkPhysicalDeviceVulkan13Features features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features.dynamicRendering = true;
    features.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    vkb::PhysicalDeviceSelector selector(vkbInst);
    vkb::PhysicalDevice physicalDevice = selector.set_minimum_version(1, 3)
                                             .set_required_features_13(features)
                                             .set_required_features_12(features12)
                                             .set_surface(surface_)
                                             .select()
                                             .value();

    vkb::DeviceBuilder deviceBuilder(physicalDevice);
    vkb::Device vkbDevice = deviceBuilder.build().value();

    device_ = vkbDevice.device;
    chosenGPU_ = physicalDevice.physical_device;

    graphicsQueue_ = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily_ = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::initSwapchain() { createSwapchain(windowExtent_.width, windowExtent_.height); }

void VulkanEngine::createSwapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder swapchainBuilder(chosenGPU_, device_, surface_);
    swapchainImageFormat_ = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain =
        swapchainBuilder
            .set_desired_format(
                VkSurfaceFormatKHR{.format = swapchainImageFormat_, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
            .value();

    swapchainExtent_ = vkbSwapchain.extent;
    swapchain_ = vkbSwapchain.swapchain;
    swapchainImages_ = vkbSwapchain.get_images().value();
    swapchainImageViews_ = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::destroySwapchain() {
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);

    // destroy swapchain resources
    for (int i = 0; i < swapchainImageViews_.size(); i++) {

        vkDestroyImageView(device_, swapchainImageViews_[i], nullptr);
    }
}

void VulkanEngine::initCommands() {
    // VkCommandPoolCreateInfo commandPoolInfo{};
    // commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    // commandPoolInfo.pNext = nullptr;
    // commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    // commandPoolInfo.queueFamilyIndex = graphicsQueueFamily_;

    // for (unsigned int i = 0; i < FRAME_OVERLAP; i++) {
    //     VK_CHECK(vkCreateCommandPool(device_, &commandPoolInfo, nullptr, &frames_[i].commandPool_));

    //     VkCommandBufferAllocateInfo cmdAllocInfo{};
    //     cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    //     cmdAllocInfo.pNext = nullptr;
    //     cmdAllocInfo.commandPool = frames_[i].commandPool_;
    //     cmdAllocInfo.commandBufferCount = 1;
    //     cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    //     VK_CHECK(vkAllocateCommandBuffers(device_, &cmdAllocInfo, &frames_[i].mainCommandBuffer_));
    // }

    // create a command pool for commands submitted to the graphics queue.
    // we also want the pool to allow for resetting of individual command buffers
    VkCommandPoolCreateInfo commandPoolInfo =
        vkinit::command_pool_create_info(graphicsQueueFamily_, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++) {

        VK_CHECK(vkCreateCommandPool(device_, &commandPoolInfo, nullptr, &frames_[i].commandPool_));

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(frames_[i].commandPool_, 1);

        VK_CHECK(vkAllocateCommandBuffers(device_, &cmdAllocInfo, &frames_[i].mainCommandBuffer_));
    }
}

void VulkanEngine::initSyncStructures() {}
