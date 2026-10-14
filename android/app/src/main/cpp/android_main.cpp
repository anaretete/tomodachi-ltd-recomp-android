// ==============================================================================
// android/app/src/main/cpp/android_main.cpp
// Native Lifecycle Bridge, Vulkan WSI & EGL Fallback Renderer for tomodachi-ltd-recomp
// ==============================================================================

#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/window.h>
#include <vulkan/vulkan.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <vector>

#include "android_storage.hpp"
#include "android_vulkan.hpp"
#include "touch_overlay.hpp"

#define LOG_TAG "TomodachiMain"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

enum class RenderBackend {
    None,
    Vulkan,
    OpenGLES
};

struct AppContext {
    struct android_app* app = nullptr;
    bool has_focus = false;
    bool window_ready = false;
    bool assets_verified = false;
    RenderBackend backend = RenderBackend::None;

    // Vulkan Handles
    VkInstance vk_instance = VK_NULL_HANDLE;
    VkPhysicalDevice vk_physical_device = VK_NULL_HANDLE;
    VkDevice vk_device = VK_NULL_HANDLE;
    VkQueue vk_queue = VK_NULL_HANDLE;

    // EGL Fallback Handles
    EGLDisplay egl_display = EGL_NO_DISPLAY;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    EGLContext egl_context = EGL_NO_CONTEXT;
    EGLConfig  egl_config = nullptr;
};

// EGL Fallback Renderer Implementation
static bool InitEGL(AppContext* ctx, ANativeWindow* window) {
    LOGI("Initializing OpenGL ES 3.0 / EGL fallback presentation pipeline...");

    ctx->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (ctx->egl_display == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed.");
        return false;
    }

    if (!eglInitialize(ctx->egl_display, nullptr, nullptr)) {
        LOGE("eglInitialize failed.");
        return false;
    }

    const EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16,
        EGL_NONE
    };

    EGLint numConfigs = 0;
    if (!eglChooseConfig(ctx->egl_display, attribs, &ctx->egl_config, 1, &numConfigs) || numConfigs <= 0) {
        LOGE("eglChooseConfig failed to find compatible configuration.");
        return false;
    }

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    ctx->egl_context = eglCreateContext(ctx->egl_display, ctx->egl_config, EGL_NO_CONTEXT, contextAttribs);
    if (ctx->egl_context == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed.");
        return false;
    }

    ctx->egl_surface = eglCreateWindowSurface(ctx->egl_display, ctx->egl_config, window, nullptr);
    if (ctx->egl_surface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed.");
        return false;
    }

    if (!eglMakeCurrent(ctx->egl_display, ctx->egl_surface, ctx->egl_surface, ctx->egl_context)) {
        LOGE("eglMakeCurrent failed.");
        return false;
    }

    LOGI("OpenGL ES 3.0 fallback initialized successfully.");
    ctx->backend = RenderBackend::OpenGLES;
    return true;
}

static void TearDownEGL(AppContext* ctx) {
    if (ctx->egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (ctx->egl_surface != EGL_NO_SURFACE) {
            eglDestroySurface(ctx->egl_display, ctx->egl_surface);
            ctx->egl_surface = EGL_NO_SURFACE;
        }
        if (ctx->egl_context != EGL_NO_CONTEXT) {
            eglDestroyContext(ctx->egl_display, ctx->egl_context);
            ctx->egl_context = EGL_NO_CONTEXT;
        }
        eglTerminate(ctx->egl_display);
        ctx->egl_display = EGL_NO_DISPLAY;
    }
}

// Minimal Vulkan Instance Bootstrapper
static bool InitVulkanBootstrap(AppContext* ctx) {
    LOGI("Probing Vulkan support...");

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Tomodachi Life Recomp";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "RecompEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0; // Baseline 1.0 ensures wide compatibility

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
        LOGW("vkCreateInstance failed (%d). Vulkan may not be supported.", res);
        return false;
    }

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(ctx->vk_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOGW("No Vulkan physical devices found on this hardware.");
        vkDestroyInstance(ctx->vk_instance, nullptr);
        ctx->vk_instance = VK_NULL_HANDLE;
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(ctx->vk_instance, &deviceCount, devices.data());
    ctx->vk_physical_device = devices[0];

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
        LOGW("vkCreateDevice failed (%d). Vulkan swapchain device creation failed.", res);
        vkDestroyInstance(ctx->vk_instance, nullptr);
        ctx->vk_instance = VK_NULL_HANDLE;
        return false;
    }

    vkGetDeviceQueue(ctx->vk_device, 0, 0, &ctx->vk_queue);
    LOGI("Vulkan bootstrap initialized successfully.");
    ctx->backend = RenderBackend::Vulkan;
    return true;
}

// Lifecycle Event Callback
static void app_handle_cmd(struct android_app* app, int32_t cmd) {
    auto* ctx = reinterpret_cast<AppContext*>(app->userData);

    switch (cmd) {
        case APP_CMD_INIT_WINDOW: {
            LOGI("Lifecycle: APP_CMD_INIT_WINDOW received.");
            if (app->window != nullptr) {
                ANativeActivity_setWindowFlags(app->activity, AWINDOW_FLAG_KEEP_SCREEN_ON, 0);

                ANativeWindow_setBuffersGeometry(
                    app->window,
                    0,
                    0,
                    WINDOW_FORMAT_RGBA_8888
                );

                int32_t width = ANativeWindow_getWidth(app->window);
                int32_t height = ANativeWindow_getHeight(app->window);
                LOGI("NativeWindow dimensions: %dx%d", width, height);

                Tomodachi::Input::TouchOverlay::Instance().UpdateDimensions(width, height);

                if (ctx->backend == RenderBackend::Vulkan && ctx->vk_instance != VK_NULL_HANDLE) {
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
                } else if (ctx->backend == RenderBackend::OpenGLES) {
                    if (ctx->egl_surface == EGL_NO_SURFACE) {
                        ctx->egl_surface = eglCreateWindowSurface(ctx->egl_display, ctx->egl_config, app->window, nullptr);
                        eglMakeCurrent(ctx->egl_display, ctx->egl_surface, ctx->egl_surface, ctx->egl_context);
                    }
                }
                ctx->window_ready = true;
            }
            break;
        }

        case APP_CMD_TERM_WINDOW: {
            LOGI("Lifecycle: APP_CMD_TERM_WINDOW received.");
            ctx->window_ready = false;

            if (ctx->backend == RenderBackend::Vulkan) {
                if (ctx->vk_device != VK_NULL_HANDLE) {
                    vkDeviceWaitIdle(ctx->vk_device);
                    Tomodachi::Platform::AndroidVulkanBridge::Instance().DestroySwapchain(ctx->vk_device);
                }
                if (ctx->vk_instance != VK_NULL_HANDLE) {
                    Tomodachi::Platform::AndroidVulkanBridge::Instance().DestroySurface(ctx->vk_instance);
                }
            } else if (ctx->backend == RenderBackend::OpenGLES) {
                if (ctx->egl_display != EGL_NO_DISPLAY && ctx->egl_surface != EGL_NO_SURFACE) {
                    eglMakeCurrent(ctx->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                    eglDestroySurface(ctx->egl_display, ctx->egl_surface);
                    ctx->egl_surface = EGL_NO_SURFACE;
                }
            }
            break;
        }

        case APP_CMD_GAINED_FOCUS: {
            LOGI("Lifecycle: APP_CMD_GAINED_FOCUS.");
            ctx->has_focus = true;
            break;
        }

        case APP_CMD_LOST_FOCUS: {
            LOGI("Lifecycle: APP_CMD_LOST_FOCUS.");
            ctx->has_focus = false;
            break;
        }

        case APP_CMD_CONFIG_CHANGED: {
            LOGI("Lifecycle: APP_CMD_CONFIG_CHANGED.");
            if (app->window != nullptr) {
                int32_t width = ANativeWindow_getWidth(app->window);
                int32_t height = ANativeWindow_getHeight(app->window);
                Tomodachi::Input::TouchOverlay::Instance().UpdateDimensions(width, height);
            }
            break;
        }

        case APP_CMD_DESTROY: {
            LOGI("Lifecycle: APP_CMD_DESTROY.");
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
        if (Tomodachi::Input::TouchOverlay::Instance().ProcessAndroidMotionEvent(event)) {
            return 1;
        }
    }

    return 0;
}

// Native Entry Point invoked by GameActivity
void android_main(struct android_app* app) {
    LOGI("=================================================");
    LOGI("Starting Tomodachi Life Recomp Native Game Session");
    LOGI("=================================================");

    AppContext ctx;
    ctx.app = app;
    app->userData = &ctx;
    app->onAppCmd = app_handle_cmd;
    app->onInputEvent = app_handle_input;

    // 1. Initialize Storage Virtualization
    Tomodachi::Platform::AndroidStorage::Instance().Initialize(app);

    // 2. Initialize Graphics Backend: Try Vulkan first, fall back to EGL/GLES3
    bool graphics_ok = InitVulkanBootstrap(&ctx);
    if (!graphics_ok) {
        LOGW("Vulkan initialization failed. Attempting EGL/GLES3 fallback...");
        // Wait for window to be available before initializing EGL
    }

    // Main Game & Event Loop
    while (true) {
        int ident;
        int events;
        struct android_poll_source* source = nullptr;

        int timeout_ms = (!ctx.has_focus || !ctx.window_ready) ? -1 : 0;

        while ((ident = ALooper_pollOnce(timeout_ms, nullptr, &events, (void**)&source)) >= 0) {
            if (source != nullptr) {
                source->process(app, source);
            }

            if (app->destroyRequested != 0) {
                LOGI("Destroy requested by Android. Exiting native loop.");
                if (ctx.backend == RenderBackend::Vulkan) {
                    if (ctx->vk_device != VK_NULL_HANDLE) {
                        vkDeviceWaitIdle(ctx.vk_device);
                        Tomodachi::Platform::AndroidVulkanBridge::Instance().DestroySwapchain(ctx.vk_device);
                        vkDestroyDevice(ctx.vk_device, nullptr);
                    }
                    if (ctx.vk_instance != VK_NULL_HANDLE) {
                        Tomodachi::Platform::AndroidVulkanBridge::Instance().DestroySurface(ctx.vk_instance);
                        vkDestroyInstance(ctx.vk_instance, nullptr);
                    }
                } else if (ctx.backend == RenderBackend::OpenGLES) {
                    TearDownEGL(&ctx);
                }
                return;
            }
        }

        // Lazy initialize EGL if Vulkan wasn't available once window is ready
        if (ctx.backend == RenderBackend::None && ctx.window_ready && app->window != nullptr) {
            if (InitEGL(&ctx, app->window)) {
                graphics_ok = true;
            }
        }

        // Active Frame Tick & Frame Presentation (Prevents black screen on all devices)
        if (ctx.has_focus && ctx.window_ready && ctx.backend != RenderBackend::None) {
            auto padState = Tomodachi::Input::TouchOverlay::Instance().GetState();

            // Interactive color feedback: shifts vibrant cyan/sky-blue upon touch
            float r = padState.touch_active ? 0.20f : 0.08f;
            float g = padState.touch_active ? 0.55f : 0.35f;
            float b = padState.touch_active ? 0.85f : 0.65f;

            if (ctx.backend == RenderBackend::Vulkan) {
                Tomodachi::Platform::AndroidVulkanBridge::Instance().RenderFrame(
                    ctx.vk_device,
                    ctx.vk_queue,
                    r, g, b
                );
            } else if (ctx.backend == RenderBackend::OpenGLES && ctx.egl_surface != EGL_NO_SURFACE) {
                glClearColor(r, g, b, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                eglSwapBuffers(ctx.egl_display, ctx.egl_surface);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }
    }
}
