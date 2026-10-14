// ==============================================================================
// src/platform/android_vulkan.hpp
// Vulkan Android WSI Bridge & Swapchain Lifecycle Management
// ==============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <android/native_window.h>
#include <vector>

namespace Tomodachi::Platform {

struct VulkanSurfaceConfig {
    VkFormat color_format = VK_FORMAT_R8G8B8A8_UNORM;
    VkColorSpaceKHR color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR; // V-Sync default on mobile
    uint32_t width = 0;
    uint32_t height = 0;
};

class AndroidVulkanBridge {
public:
    static AndroidVulkanBridge& Instance();

    // Attach Android Native Window and instantiate VkSurfaceKHR
    bool CreateSurface(VkInstance instance, ANativeWindow* window);

    // Destroy active VkSurfaceKHR before ANativeWindow is torn down by Android
    void DestroySurface(VkInstance instance);

    // Swapchain creation and recreation
    bool CreateSwapchain(VkPhysicalDevice physicalDevice, VkDevice device);
    void DestroySwapchain(VkDevice device);

    // Active Frame Presentation (Renders clear color and submits to screen)
    bool RenderFrame(VkDevice device, VkQueue queue, float r, float g, float b);

    // Accessors
    VkSurfaceKHR GetSurface() const { return m_surface; }
    VkSwapchainKHR GetSwapchain() const { return m_swapchain; }
    const VulkanSurfaceConfig& GetConfig() const { return m_config; }
    const std::vector<VkImage>& GetSwapchainImages() const { return m_swapchain_images; }
    const std::vector<VkImageView>& GetSwapchainImageViews() const { return m_swapchain_image_views; }
    bool IsReady() const { return m_surface != VK_NULL_HANDLE && m_swapchain != VK_NULL_HANDLE; }

private:
    AndroidVulkanBridge() = default;
    ~AndroidVulkanBridge() = default;
    AndroidVulkanBridge(const AndroidVulkanBridge&) = delete;
    AndroidVulkanBridge& operator=(const AndroidVulkanBridge&) = delete;

    ANativeWindow* m_window = nullptr;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VulkanSurfaceConfig m_config;

    std::vector<VkImage> m_swapchain_images;
    std::vector<VkImageView> m_swapchain_image_views;
    std::vector<VkFramebuffer> m_framebuffers;

    VkRenderPass m_render_pass = VK_NULL_HANDLE;
    VkCommandPool m_command_pool = VK_NULL_HANDLE;
    VkCommandBuffer m_command_buffer = VK_NULL_HANDLE;
    VkSemaphore m_image_available_sem = VK_NULL_HANDLE;
    VkSemaphore m_render_finished_sem = VK_NULL_HANDLE;
    VkFence m_in_flight_fence = VK_NULL_HANDLE;
};

} // namespace Tomodachi::Platform
