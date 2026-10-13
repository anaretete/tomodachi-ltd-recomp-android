// ==============================================================================
// android/app/src/main/cpp/android_main.cpp
// Native Lifecycle Bridge & Vulkan Surface Hook for tomodachi-ltd-recomp
// ==============================================================================

#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/native_window.h>
#include <vulkan/vulkan.h>
#include <unistd.h>
#include <chrono>
#include <thread>

#include "android_storage.hpp"
#include "android_vulkan.hpp"
#include "touch_overlay.hpp"

#define LOG_TAG "TomodachiMain"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct AppContext {
    struct android_app* app = nullptr;
    bool has_focus = false;
    bool window_ready = false;
    bool assets_verified = false;

    // Vulkan Core Handles (Supplied by recomp engine or initialized via platform bridge)
    VkInstance vk_instance = VK_NULL_HANDLE;
    VkPhysicalDevice vk_physical_device = VK_NULL_HANDLE;
    VkDevice vk_device = VK_NULL_HANDLE;
    VkQueue vk_queue = VK_NULL_HANDLE;
};

// Lifecycle Event Callback
static void app_handle_cmd(struct android_app* app, int32_t cmd) {
    auto* ctx = reinterpret_cast<AppContext*>(app->userData);

    switch (cmd) {
        case APP_CMD_INIT_WINDOW: {
            LOGI("Lifecycle: APP_CMD_INIT_WINDOW received.");
            if (app->window != nullptr) {
                // Configure native buffer geometry for high-performance 32-bit presentation
                ANativeWindow_setBuffersGeometry(
                    app->window,
                    0,
                    0,
                    WINDOW_FORMAT_RGBA_8888
                );

                int32_t width = ANativeWindow_getWidth(app->window);
                int32_t height = ANativeWindow_getHeight(app->window);
                LOGI("NativeWindow dimensions: %dx%d", width, height);

                // Update Virtual Touchscreen Overlay Hitboxes
                Tomodachi::Input::TouchOverlay::Instance().UpdateDimensions(width, height);

                // Initialize or Reconnect Vulkan Android Surface
                if (ctx->vk_instance != VK_NULL_HANDLE) {
                    Tomodachi::Platform::AndroidVulkanBridge::Instance().CreateSurface(
                        ctx->vk_instance,
                        app->window
                    );
                    if (ctx->vk_device != VK_NULL_HANDLE) {
                        Tomodachi::Platform::AndroidVulkanBridge::Instance().CreateSwapchain(
                            ctx->vk_physical_device,
                            ctx->vk_device
                        );
                    }
                }
                ctx->window_ready = true;
            }
            break;
        }

        case APP_CMD_TERM_WINDOW: {
            LOGI("Lifecycle: APP_CMD_TERM_WINDOW received. Tearing down Vulkan presentation surface.");
            ctx->window_ready = false;

            // Vulkan Spec Compliance: Destroy swapchain and surface BEFORE returning from TERM_WINDOW.
            // On Android, ANativeWindow becomes invalid immediately after this callback finishes.
            if (ctx->vk_device != VK_NULL_HANDLE) {
                vkDeviceWaitIdle(ctx->vk_device);
                Tomodachi::Platform::AndroidVulkanBridge::Instance().DestroySwapchain(ctx->vk_device);
            }
            if (ctx->vk_instance != VK_NULL_HANDLE) {
                Tomodachi::Platform::AndroidVulkanBridge::Instance().DestroySurface(ctx->vk_instance);
            }
            break;
        }

        case APP_CMD_GAINED_FOCUS: {
            LOGI("Lifecycle: APP_CMD_GAINED_FOCUS - Resuming audio and simulation loop.");
            ctx->has_focus = true;
            break;
        }

        case APP_CMD_LOST_FOCUS: {
            LOGI("Lifecycle: APP_CMD_LOST_FOCUS - Pausing audio and simulation loop.");
            ctx->has_focus = false;
            break;
        }

        case APP_CMD_CONFIG_CHANGED: {
            LOGI("Lifecycle: APP_CMD_CONFIG_CHANGED (Screen rotation / size change).");
            if (app->window != nullptr) {
                int32_t width = ANativeWindow_getWidth(app->window);
                int32_t height = ANativeWindow_getHeight(app->window);
                Tomodachi::Input::TouchOverlay::Instance().UpdateDimensions(width, height);
            }
            break;
        }

        case APP_CMD_DESTROY: {
            LOGI("Lifecycle: APP_CMD_DESTROY. Finalizing application shutdown.");
            ctx->window_ready = false;
            ctx->has_focus = false;
            break;
        }

        default:
            break;
    }
}

// Input Event Callback
static int32_t app_handle_input(struct android_app* app, AInputEvent* event) {
    int32_t eventType = AInputEvent_getType(event);

    if (eventType == AINPUT_EVENT_TYPE_MOTION) {
        // Feed raw multi-touch coordinates directly to the Virtual Touch Overlay
        if (Tomodachi::Input::TouchOverlay::Instance().ProcessAndroidMotionEvent(event)) {
            return 1; // Event consumed
        }
    }

    return 0; // Event unhandled, let system process
}

// Minimal Vulkan Instance Bootstrapper (For standalone native glue initialization)
static bool InitVulkanBootstrap(AppContext* ctx) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Tomodachi Life Recomp";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "RecompEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    };

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = extensions;

    VkResult res = vkCreateInstance(&createInfo, nullptr, &ctx->vk_instance);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create bootstrap VkInstance: %d", res);
        return false;
    }

    // Select primary physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(ctx->vk_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOGE("No Vulkan physical devices found.");
        return false;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(ctx->vk_instance, &deviceCount, devices.data());
    ctx->vk_physical_device = devices[0];

    // Create logical device with graphics queue and swapchain extension
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = 0;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

    res = vkCreateDevice(ctx->vk_physical_device, &deviceCreateInfo, nullptr, &ctx->vk_device);
    if (res != VK_SUCCESS) {
        LOGE("Failed to create bootstrap VkDevice: %d", res);
        return false;
    }

    vkGetDeviceQueue(ctx->vk_device, 0, 0, &ctx->vk_queue);
    LOGI("Vulkan bootstrap initialized successfully.");
    return true;
}

// Native Entry Point invoked by android.app.NativeActivity
void android_main(struct android_app* app) {
    LOGI("=================================================");
    LOGI("Starting Tomodachi Life Recomp Native Runtime");
    LOGI("=================================================");

    AppContext ctx;
    ctx.app = app;
    app->userData = &ctx;
    app->onAppCmd = app_handle_cmd;
    app->onInputEvent = app_handle_input;

    // 1. Initialize Storage Virtualization & Redirect Desktop Paths
    Tomodachi::Platform::AndroidStorage::Instance().Initialize(app);

    // 2. Perform Asset Verification Check
    std::string missingPath;
    ctx.assets_verified = Tomodachi::Platform::AndroidStorage::Instance().VerifyAssets(missingPath);

    if (!ctx.assets_verified) {
        LOGW("Assets missing or incomplete. Presenting user notification dialog.");
        Tomodachi::Platform::AndroidStorage::Instance().ShowFatalAssetDialog(missingPath);
    } else {
        LOGI("Game assets verified at: %s", 
             Tomodachi::Platform::AndroidStorage::Instance().GetRomFsPath().c_str());
    }

    // 3. Initialize Vulkan Backend
    InitVulkanBootstrap(&ctx);

    // Main Game & Event Loop
    while (true) {
        int ident;
        int events;
        struct android_poll_source* source = nullptr;

        // Block when in background or awaiting assets; poll immediately when active
        int timeout_ms = (!ctx.has_focus || !ctx.assets_verified) ? -1 : 0;

        while ((ident = ALooper_pollOnce(timeout_ms, nullptr, &events, (void**)&source)) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }

            if (app->destroyRequested != 0) {
                LOGI("Destroy requested by Android OS. Exiting main loop.");
                if (ctx.vk_device != VK_NULL_HANDLE) {
                    vkDeviceWaitIdle(ctx.vk_device);
                    Tomodachi::Platform::AndroidVulkanBridge::Instance().DestroySwapchain(ctx.vk_device);
                    vkDestroyDevice(ctx.vk_device, nullptr);
                }
                if (ctx.vk_instance != VK_NULL_HANDLE) {
                    Tomodachi::Platform::AndroidVulkanBridge::Instance().DestroySurface(ctx.vk_instance);
                    vkDestroyInstance(ctx.vk_instance, nullptr);
                }
                return;
            }
        }

        // Active Frame Tick
        if (ctx.has_focus && ctx.window_ready && ctx.assets_verified) {
            // Retrieve current virtual controller input buffer
            auto padState = Tomodachi::Input::TouchOverlay::Instance().GetState();

            // Render loop placeholder for lifted recompilation step:
            // 1. Step lifted game logic tick with padState
            // 2. Update audio buffers
            // 3. Render game frame + Touch Overlay to Vulkan swapchain
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // Target ~60 FPS
        }
    }
}
