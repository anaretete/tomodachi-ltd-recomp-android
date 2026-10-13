// ==============================================================================
// src/platform/android_vulkan.cpp
// Vulkan Android WSI Bridge & Swapchain Lifecycle Implementation
// ==============================================================================

#include "android_vulkan.hpp"
#include <android/log.h>
#include <algorithm>

#define LOG_TAG "TomodachiVulkan"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace Tomodachi::Platform {

AndroidVulkanBridge& AndroidVulkanBridge::Instance() {
    static AndroidVulkanBridge s_instance;
    return s_instance;
}

bool AndroidVulkanBridge::CreateSurface(VkInstance instance, ANativeWindow* window) {
    if (!instance || !window) {
        LOGE("Failed to create Vulkan surface: invalid VkInstance or ANativeWindow.");
        return false;
    }

    m_window = window;
    m_config.width = static_cast<uint32_t>(ANativeWindow_getWidth(window));
    m_config.height = static_cast<uint32_t>(ANativeWindow_getHeight(window));

    VkAndroidSurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.window = window;

    VkResult res = vkCreateAndroidSurfaceKHR(instance, &createInfo, nullptr, &m_surface);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateAndroidSurfaceKHR failed with error code: %d", res);
        m_surface = VK_NULL_HANDLE;
        return false;
    }

    LOGI("Successfully created Android Vulkan Surface (%ux%u)", m_config.width, m_config.height);
    return true;
}

void AndroidVulkanBridge::DestroySurface(VkInstance instance) {
    if (m_surface != VK_NULL_HANDLE && instance != VK_NULL_HANDLE) {
        LOGI("Tearing down Android Vulkan Surface.");
        vkDestroySurfaceKHR(instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    m_window = nullptr;
}

bool AndroidVulkanBridge::CreateSwapchain(VkPhysicalDevice physicalDevice, VkDevice device) {
    if (m_surface == VK_NULL_HANDLE || device == VK_NULL_HANDLE) {
        LOGE("Cannot create swapchain: invalid surface or logical device.");
        return false;
    }

    // Query Surface Capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_surface, &capabilities);

    // Query Surface Formats
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, formats.data());

    VkSurfaceFormatKHR selectedFormat = formats[0];
    for (const auto& fmt : formats) {
        if ((fmt.format == VK_FORMAT_R8G8B8A8_UNORM || fmt.format == VK_FORMAT_B8G8R8A8_UNORM) &&
            fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            selectedFormat = fmt;
            break;
        }
    }
    m_config.color_format = selectedFormat.format;
    m_config.color_space = selectedFormat.colorSpace;

    // Extent
    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == 0xFFFFFFFF) {
        extent.width = std::clamp(m_config.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(m_config.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }
    m_config.width = extent.width;
    m_config.height = extent.height;

    // Triple buffering desired
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    // Present Mode (FIFO guaranteed on Android, Mailbox for unthrottled low-latency)
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, presentModes.data());

    VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& mode : presentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            selectedPresentMode = mode;
            break;
        }
    }
    m_config.present_mode = selectedPresentMode;

    // Create Swapchain
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = m_config.color_format;
    createInfo.imageColorSpace = m_config.color_space;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    if (!(capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)) {
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }
    createInfo.presentMode = m_config.present_mode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult res = vkCreateSwapchainKHR(device, &createInfo, nullptr, &m_swapchain);
    if (res != VK_SUCCESS) {
        LOGE("vkCreateSwapchainKHR failed: %d", res);
        return false;
    }

    // Retrieve images and create views
    uint32_t actualCount = 0;
    vkGetSwapchainImagesKHR(device, m_swapchain, &actualCount, nullptr);
    m_swapchain_images.resize(actualCount);
    vkGetSwapchainImagesKHR(device, m_swapchain, &actualCount, m_swapchain_images.data());

    m_swapchain_image_views.resize(actualCount);
    for (size_t i = 0; i < actualCount; ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_swapchain_images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_config.color_format;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &m_swapchain_image_views[i]) != VK_SUCCESS) {
            LOGE("Failed to create swapchain image view at index %zu", i);
            return false;
        }
    }

    LOGI("Swapchain created successfully with %u images (%ux%u)", actualCount, extent.width, extent.height);
    return true;
}

void AndroidVulkanBridge::DestroySwapchain(VkDevice device) {
    if (device == VK_NULL_HANDLE) return;

    for (auto imageView : m_swapchain_image_views) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, imageView, nullptr);
        }
    }
    m_swapchain_image_views.clear();
    m_swapchain_images.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        LOGI("Destroying active swapchain.");
        vkDestroySwapchainKHR(device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

} // namespace Tomodachi::Platform
