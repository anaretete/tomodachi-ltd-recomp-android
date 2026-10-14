# Tomodachi Life: Living the Dream — Native Android Port Harness

Multi-architecture native Android porting harness for [`tomodachi-ltd-recomp`](https://github.com/RussoPhone/tomodachi-ltd-recomp), supporting **ARM64 (`arm64-v8a`)**, **ARM32 (`armeabi-v7a`)**, **x86_64**, and **x86**.

---

## Features

- **Multi-Architecture Support:** Builds for `arm64-v8a`, `armeabi-v7a`, `x86_64`, and `x86`, with both a Universal APK and architecture-specific splits.
- **Native NDK Architecture:** Pure C++20 engine with proper JNI/`NativeActivity` exports and 16KB page-size alignment support.
- **Vulkan 1.1+ Rendering:** Full `VkSurfaceKHR` integration using `vkCreateAndroidSurfaceKHR`, safe lifecycle swapchain teardown, and graceful hardware detection.
- **Low-Latency Virtual Touch Overlay:** Responsive multi-touch on-screen controller (Left Analog Stick, D-Pad, A/B/X/Y, Shoulders L/R/ZL/ZR, System +/-) and seamless native capacitive screen touch pass-through.
- **Android Storage Virtualization:** Automatic path redirection for internal and external storage sandboxes, including missing RomFS asset verification and native dialogs.
- **Automated CI/CD & Signed Releases:** GitHub Actions compiles, signs (v1/v2/v3), and releases all APK variants automatically.

---

## Directory Structure

```
├── .github/workflows/
│   └── build-and-release.yml    # Automated CI/CD pipeline
├── android/
│   ├── app/
│   │   ├── build.gradle.kts     # NativeActivity app module
│   │   └── src/main/
│   │       ├── AndroidManifest.xml
│   │       ├── cpp/
│   │       │   └── android_main.cpp  # Lifecycle & Vulkan surface hook
│   │       └── res/
│   ├── build.gradle.kts         # Root Gradle build
│   └── settings.gradle.kts
├── cmake/
│   └── Android.cmake            # NDK toolchain overlay
├── CMakeLists.txt               # Unified project CMake build
└── src/
    ├── input/
    │   ├── touch_overlay.hpp    # Touch HUD & input injection
    │   └── touch_overlay.cpp
    └── platform/
        ├── android_storage.hpp  # RomFS path resolver & JNI dialog
        ├── android_storage.cpp
        ├── android_vulkan.hpp   # Vulkan WSI swapchain bridge
        └── android_vulkan.cpp
```

---

## Installation & RomFS Setup

1. Download the latest `tomodachi-ltd-recomp-arm64.apk` from the [Releases](releases) section.
2. Install the APK on your Android device.
3. Dump your Switch game's `romfs` folder.
4. Copy the `romfs` directory to your device's external app storage:
   ```bash
   adb shell mkdir -p /sdcard/Android/data/com.recomp.tomodachiltd/files/romfs
   adb push /path/to/romfs/* /sdcard/Android/data/com.recomp.tomodachiltd/files/romfs/
   ```
5. Launch the game!

---

## Building from Source

### Using Gradle
```bash
cd android
./gradlew assembleRelease
```
The output APK will be located at:
`android/app/build/outputs/apk/release/app-release.apk`
