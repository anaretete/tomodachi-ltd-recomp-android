# ==============================================================================
# cmake/Android.cmake
# Native Android NDK Toolchain Overlay for tomodachi-ltd-recomp
# ==============================================================================

if(NOT ANDROID)
    message(FATAL_ERROR "cmake/Android.cmake must only be included in an Android build context.")
endif()

message(STATUS "Configuring tomodachi-ltd-recomp for Android (ABI: ${ANDROID_ABI}, API: ${ANDROID_PLATFORM_LEVEL})")

# Architecture specific optimization flags
if(ANDROID_ABI STREQUAL "arm64-v8a")
    add_compile_options(-march=armv8-a+crypto+crc)
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    add_compile_options(-march=armv7-a -mfloat-abi=softfp -mfpu=neon)
elseif(ANDROID_ABI STREQUAL "x86_64")
    add_compile_options(-march=x86-64 -msse4.2 -mpopcnt)
elseif(ANDROID_ABI STREQUAL "x86")
    add_compile_options(-march=i686 -msse3)
endif()

# Global Android Preprocessor Definitions
add_compile_definitions(
    __ANDROID__
    ANDROID
    VK_USE_PLATFORM_ANDROID_KHR
    SDL_PLATFORM_ANDROID
    RECOMP_ANDROID=1
    NOMINMAX
)

# Common Compiler optimization flags
add_compile_options(
    -O3
    -ffast-math
    -funroll-loops
    -fomit-frame-pointer
    -Wall
    -Wextra
    -Wno-unused-parameter
)

# Explicitly disable desktop GUI and dialog dependencies
set(ENABLE_QT OFF CACHE BOOL "Disable Desktop Qt6" FORCE)
set(ENABLE_ZENITY OFF CACHE BOOL "Disable Zenity file picker" FORCE)
set(USE_WAYLAND OFF CACHE BOOL "Disable Wayland windowing" FORCE)
set(USE_X11 OFF CACHE BOOL "Disable X11 windowing" FORCE)

# Intercept and stub find_package for Qt6 to ensure desktop headers are never pulled
macro(find_package)
    if("${ARGV0}" STREQUAL "Qt6" OR "${ARGV0}" STREQUAL "Qt5")
        message(STATUS "Android Overlay: Blocked desktop package search for ${ARGV0}")
    else()
        _find_package(${ARGV})
    endif()
endmacro()

# Standard Android NDK Core System Libraries
find_library(ANDROID_LIB android REQUIRED)
find_library(LOG_LIB log REQUIRED)
find_library(EGL_LIB EGL REQUIRED)
find_library(GLES3_LIB GLESv3 REQUIRED)
find_library(VULKAN_LIB vulkan REQUIRED)
find_library(AAUDIO_LIB aaudio REQUIRED)
find_library(OPENSLES_LIB OpenSLES REQUIRED)

set(ANDROID_SYSTEM_LIBRARIES
    ${ANDROID_LIB}
    ${LOG_LIB}
    ${EGL_LIB}
    ${GLES3_LIB}
    ${VULKAN_LIB}
    ${AAUDIO_LIB}
    ${OPENSLES_LIB}
)

# NDK Native App Glue Library configuration
if(EXISTS "${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
    add_library(native_app_glue STATIC
        "${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c"
    )
    target_include_directories(native_app_glue PUBLIC
        "${ANDROID_NDK}/sources/android/native_app_glue"
    )
    target_link_libraries(native_app_glue PUBLIC ${LOG_LIB} ${ANDROID_LIB})
else()
    message(WARNING "NDK native_app_glue not located at standard path; ensure glue is provided in source tree.")
endif()

# SDL3 Android Configuration
# Configure SDL3 for embedded mobile compilation without desktop video subsystems
set(SDL_SHARED OFF CACHE BOOL "Build static SDL3 for Android APK packaging" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build static SDL3 for Android APK packaging" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Disable SDL test library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Disable SDL unit tests" FORCE)
set(SDL_VULKAN ON CACHE BOOL "Enable Vulkan support in SDL3" FORCE)
set(SDL_OPENGLES ON CACHE BOOL "Enable OpenGL ES in SDL3" FORCE)
set(SDL_ALSA OFF CACHE BOOL "Disable ALSA audio" FORCE)
set(SDL_PULSEAUDIO OFF CACHE BOOL "Disable PulseAudio" FORCE)
set(SDL_PIPEWIRE OFF CACHE BOOL "Disable PipeWire audio" FORCE)
set(SDL_JACK OFF CACHE BOOL "Disable JACK audio" FORCE)
set(SDL_KMSDRM OFF CACHE BOOL "Disable KMSDRM" FORCE)
set(SDL_WAYLAND OFF CACHE BOOL "Disable Wayland in SDL3" FORCE)
set(SDL_X11 OFF CACHE BOOL "Disable X11 in SDL3" FORCE)

# Interface target bundling Android compile flags, defines, and system links
add_library(tomodachi_android_platform INTERFACE)
target_include_directories(tomodachi_android_platform INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/src/input
)
target_link_libraries(tomodachi_android_platform INTERFACE
    ${ANDROID_SYSTEM_LIBRARIES}
    native_app_glue
)
