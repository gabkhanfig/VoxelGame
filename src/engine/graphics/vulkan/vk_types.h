#pragma once

// #include "vk_enum_string_helper.h"
// clang-format off
#include <vulkan/vulkan.h>
#include <vma_usage.h>
// clang-format on

#define VK_CHECK(x)                                                                                                    \
    do {                                                                                                               \
        VkResult err = x;                                                                                              \
        if (err) {                                                                                                     \
            fmt::print("Detected Vulkan error: {}", string_VkResult(err));                                             \
            abort();                                                                                                   \
        }                                                                                                              \
    } while (0)