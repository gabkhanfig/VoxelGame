#include "vk_engine.h"
#include "vk_images.h"
#include "vk_initializers.h"
#include "vk_pipelines.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>
#include <assert.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <chrono>
#include <imgui.h>
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
    initDescriptors();
    initPipelines();
    initImgui();

    // everything went fine
    isInitialized_ = true;
}

void VulkanEngine::cleanup() {
    if (isInitialized_) {
        vkDeviceWaitIdle(device_);
        for (unsigned int i = 0; i < FRAME_OVERLAP; i++) {
            vkDestroyCommandPool(device_, frames_[i].commandPool_, nullptr);

            // destroy sync objects
            vkDestroyFence(device_, frames_[i].renderFence_, nullptr);
            vkDestroySemaphore(device_, frames_[i].renderSemaphore_, nullptr);
            vkDestroySemaphore(device_, frames_[i].swapchainSemaphore_, nullptr);

            frames_[i].deletionQueue_.flush();
        }

        mainDeletionQueue_.flush();

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
    VK_CHECK(vkWaitForFences(device_, 1, &get_current_frame().renderFence_, true, 1000000000));

    get_current_frame().deletionQueue_.flush();

    VK_CHECK(vkResetFences(device_, 1, &get_current_frame().renderFence_));

    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(device_, swapchain_, 100000000000, get_current_frame().swapchainSemaphore_, nullptr,
                                   &swapchainImageIndex));

    VkCommandBuffer cmd = get_current_frame().mainCommandBuffer_;

    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    VkCommandBufferBeginInfo cmdBeginInfo =
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    drawExtent_.width = drawImage_.imageExtent.width;
    drawExtent_.height = drawImage_.imageExtent.height;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    vkutil::transition_image(cmd, swapchainImages_[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_GENERAL);

    this->drawBackground(cmd);

    vkutil::transition_image(cmd, drawImage_.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, swapchainImages_[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkutil::copy_image_to_image(cmd, drawImage_.image, swapchainImages_[swapchainImageIndex], drawExtent_,
                                swapchainExtent_);

    draw_imgui(cmd, swapchainImageViews_[swapchainImageIndex]);

    vkutil::transition_image(cmd, swapchainImages_[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                                   get_current_frame().swapchainSemaphore_);
    VkSemaphoreSubmitInfo signalInfo =
        vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().renderSemaphore_);

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, &signalInfo, &waitInfo);

    VK_CHECK(vkQueueSubmit2(graphicsQueue_, 1, &submit, get_current_frame().renderFence_));

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &get_current_frame().renderSemaphore_;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(graphicsQueue_, &presentInfo));

    frameNumber_ += 1;
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

            ImGui_ImplSDL3_ProcessEvent(&e);
        }

        // do not draw if we are minimized
        if (stopRendering_) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();

        ImGui::Render();

        draw();
    }
}

void VulkanEngine::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) {
    VK_CHECK(vkResetFences(device_, 1, &immFence_));
    VK_CHECK(vkResetCommandBuffer(immCommandBuffer_, 0));

    VkCommandBuffer cmd = immCommandBuffer_;

    VkCommandBufferBeginInfo cmdBeginInfo =
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

    VK_CHECK(vkQueueSubmit2(graphicsQueue_, 1, &submit, immFence_));

    VK_CHECK(vkWaitForFences(device_, 1, &immFence_, true, 999999999999));
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

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = chosenGPU_;
    allocatorInfo.device = device_;
    allocatorInfo.instance = instance_;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator_));

    mainDeletionQueue_.pushFunction([&]() { vmaDestroyAllocator(allocator_); });
}

void VulkanEngine::initSwapchain() {
    createSwapchain(windowExtent_.width, windowExtent_.height);

    VkExtent3D drawImageExtent = {windowExtent_.width, windowExtent_.height, 1};

    // TODO probably change this?
    drawImage_.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    drawImage_.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = vkinit::image_create_info(drawImage_.imageFormat, drawImageUsages, drawImageExtent);

    VmaAllocationCreateInfo rimg_allocinfo{};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vmaCreateImage(allocator_, &rimg_info, &rimg_allocinfo, &drawImage_.image, &drawImage_.allocation, nullptr);

    VkImageViewCreateInfo rview_info =
        vkinit::imageview_create_info(drawImage_.imageFormat, drawImage_.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(device_, &rview_info, nullptr, &drawImage_.imageView));

    mainDeletionQueue_.pushFunction([=]() {
        vkDestroyImageView(device_, drawImage_.imageView, nullptr);
        vmaDestroyImage(allocator_, drawImage_.image, drawImage_.allocation);
    });
}

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

    {
        VK_CHECK(vkCreateCommandPool(device_, &commandPoolInfo, nullptr, &immCommandPool_));

        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(immCommandPool_, 1);

        VK_CHECK(vkAllocateCommandBuffers(device_, &cmdAllocInfo, &immCommandBuffer_));

        mainDeletionQueue_.pushFunction([=]() { vkDestroyCommandPool(device_, immCommandPool_, nullptr); });
    }
}

void VulkanEngine::initSyncStructures() {
    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (unsigned int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(device_, &fenceCreateInfo, nullptr, &frames_[i].renderFence_));

        VK_CHECK(vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &frames_[i].swapchainSemaphore_));
        VK_CHECK(vkCreateSemaphore(device_, &semaphoreCreateInfo, nullptr, &frames_[i].renderSemaphore_));
    }

    {
        VK_CHECK(vkCreateFence(device_, &fenceCreateInfo, nullptr, &immFence_));
        mainDeletionQueue_.pushFunction([=]() { vkDestroyFence(device_, immFence_, nullptr); });
    }
}

void VulkanEngine::initDescriptors() {
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};

    globalDescriptorAllocator.initPool(device_, 10, sizes);

    {
        DescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        drawImageDescriptorLayout_ = builder.build(device_, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    drawImageDescriptors_ = globalDescriptorAllocator.allocate(device_, drawImageDescriptorLayout_);

    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgInfo.imageView = drawImage_.imageView;

    VkWriteDescriptorSet drawImageWrite{};
    drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawImageWrite.pNext = nullptr;

    drawImageWrite.dstBinding = 0;
    drawImageWrite.dstSet = drawImageDescriptors_;
    drawImageWrite.descriptorCount = 1;
    drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    drawImageWrite.pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(device_, 1, &drawImageWrite, 0, nullptr);

    mainDeletionQueue_.pushFunction([&]() {
        globalDescriptorAllocator.destroyPool(device_);

        vkDestroyDescriptorSetLayout(device_, drawImageDescriptorLayout_, nullptr);
    });
}

void VulkanEngine::initPipelines() { initBackgroundPipelines(); }

void VulkanEngine::initBackgroundPipelines() {
    VkPipelineLayoutCreateInfo computeLayout{};

    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.pSetLayouts = &drawImageDescriptorLayout_;
    computeLayout.setLayoutCount = 1;

    VK_CHECK(vkCreatePipelineLayout(device_, &computeLayout, nullptr, &gradientPipelineLayout_));

    VkShaderModule computeDrawShader;
    if (!vkutil::load_shader_module(ASSET_PATH "shaders/gradient.comp.spv", device_, &computeDrawShader)) {
        std::println("Error when building the compute shader");
    }

    VkPipelineShaderStageCreateInfo stageinfo{};
    stageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageinfo.pNext = nullptr;
    stageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT; // makes compute shader
    stageinfo.module = computeDrawShader;
    stageinfo.pName = "main"; // entry point

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.layout = gradientPipelineLayout_;
    computePipelineCreateInfo.stage = stageinfo;

    VK_CHECK(
        vkCreateComputePipelines(device_, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradientPipeline_));

    vkDestroyShaderModule(device_, computeDrawShader, nullptr);

    mainDeletionQueue_.pushFunction([&]() {
        vkDestroyPipelineLayout(device_, gradientPipelineLayout_, nullptr);
        vkDestroyPipeline(device_, gradientPipeline_, nullptr);
    });
}

void VulkanEngine::initImgui() {
    VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(device_, &pool_info, nullptr, &imguiPool));

    ImGui::CreateContext();

    ImGui_ImplSDL3_InitForVulkan(window_);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance_;
    initInfo.PhysicalDevice = chosenGPU_;
    initInfo.Device = device_;
    initInfo.Queue = graphicsQueue_;
    initInfo.DescriptorPool = imguiPool;
    initInfo.MinImageCount = 3;
    initInfo.ImageCount = 3;
    initInfo.UseDynamicRendering = true;

    initInfo.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &swapchainImageFormat_;

    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);

    // ImGui_ImplVulkan_CreateFontsTexture(); no longer necessary supposedly

    mainDeletionQueue_.pushFunction([=]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(device_, imguiPool, nullptr);
    });
}

void VulkanEngine::drawBackground(VkCommandBuffer cmd) {
    // VkClearColorValue clearValue;
    // float flash = std::abs(std::sin(static_cast<float>(frameNumber_) / 120.f));
    // clearValue = {{0.0f, 0.0f, flash, 1.0f}};

    // VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    // vkCmdClearColorImage(cmd, drawImage_.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipeline_);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipelineLayout_, 0, 1, &drawImageDescriptors_,
                            0, nullptr);

    // 16x16 workgroup
    vkCmdDispatch(cmd, static_cast<uint32_t>(std::ceil(drawExtent_.width / 16.0)),
                  static_cast<uint32_t>(std::ceil(drawExtent_.height / 16.0)), 1);
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView) {
    VkRenderingAttachmentInfo colorAttachment =
        vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(swapchainExtent_, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}
