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

    // Extent calculation
    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == 0xFFFFFFFF) {
        extent.width = std::clamp(m_config.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(m_config.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }
    m_config.width = extent.width;
    m_config.height = extent.height;

    // Buffer count
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    // Present Mode
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

    // Create Render Pass for screen clearing & presentation
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_config.color_format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &m_render_pass) != VK_SUCCESS) {
        LOGE("Failed to create Vulkan render pass.");
        return false;
    }

    // Create Framebuffers
    m_framebuffers.resize(actualCount);
    for (size_t i = 0; i < actualCount; ++i) {
        VkImageView attachments[] = { m_swapchain_image_views[i] };
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_render_pass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = attachments;
        fbInfo.width = extent.width;
        fbInfo.height = extent.height;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            LOGE("Failed to create framebuffer at index %zu", i);
            return false;
        }
    }

    // Create Command Pool & Command Buffer
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = 0;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &m_command_pool) != VK_SUCCESS) {
        LOGE("Failed to create Vulkan command pool.");
        return false;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_command_pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &allocInfo, &m_command_buffer) != VK_SUCCESS) {
        LOGE("Failed to allocate Vulkan command buffer.");
        return false;
    }

    // Create Synchronization Objects
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(device, &semInfo, nullptr, &m_image_available_sem);
    vkCreateSemaphore(device, &semInfo, nullptr, &m_render_finished_sem);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(device, &fenceInfo, nullptr, &m_in_flight_fence);

    LOGI("Swapchain, framebuffers, and render pass initialized successfully (%ux%u)", extent.width, extent.height);
    return true;
}

bool AndroidVulkanBridge::RenderFrame(VkDevice device, VkQueue queue, float r, float g, float b) {
    if (!IsReady() || device == VK_NULL_HANDLE || queue == VK_NULL_HANDLE || m_render_pass == VK_NULL_HANDLE) {
        return false;
    }

    vkWaitForFences(device, 1, &m_in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &m_in_flight_fence);

    uint32_t imageIndex = 0;
    VkResult res = vkAcquireNextImageKHR(device, m_swapchain, UINT64_MAX, m_image_available_sem, VK_NULL_HANDLE, &imageIndex);
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        return false;
    }

    vkResetCommandBuffer(m_command_buffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(m_command_buffer, &beginInfo);

    VkClearValue clearColor = {{{ r, g, b, 1.0f }}};
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_render_pass;
    renderPassInfo.framebuffer = m_framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = { m_config.width, m_config.height };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(m_command_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(m_command_buffer);
    vkEndCommandBuffer(m_command_buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[] = { m_image_available_sem };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_command_buffer;
    VkSemaphore signalSemaphores[] = { m_render_finished_sem };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(queue, 1, &submitInfo, m_in_flight_fence) != VK_SUCCESS) {
        return false;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    VkSwapchainKHR swapchains[] = { m_swapchain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(queue, &presentInfo);
    return true;
}

void AndroidVulkanBridge::DestroySwapchain(VkDevice device) {
    if (device == VK_NULL_HANDLE) return;

    if (m_in_flight_fence != VK_NULL_HANDLE) {
        vkDestroyFence(device, m_in_flight_fence, nullptr);
        m_in_flight_fence = VK_NULL_HANDLE;
    }
    if (m_image_available_sem != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, m_image_available_sem, nullptr);
        m_image_available_sem = VK_NULL_HANDLE;
    }
    if (m_render_finished_sem != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, m_render_finished_sem, nullptr);
        m_render_finished_sem = VK_NULL_HANDLE;
    }
    if (m_command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, m_command_pool, nullptr);
        m_command_pool = VK_NULL_HANDLE;
        m_command_buffer = VK_NULL_HANDLE;
    }

    for (auto fb : m_framebuffers) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
    }
    m_framebuffers.clear();

    if (m_render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_render_pass, nullptr);
        m_render_pass = VK_NULL_HANDLE;
    }

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
