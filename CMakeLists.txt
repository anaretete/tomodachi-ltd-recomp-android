# ==============================================================================
# CMakeLists.txt
# Root Build Definition for tomodachi-ltd-recomp (Desktop + Android NDK)
# ==============================================================================

cmake_minimum_required(VERSION 3.22.1)
project(tomodachi-ltd-recomp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Platform detection and configuration
if(ANDROID)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Android.cmake)
endif()

# Core sources: Platform abstraction, virtual touch overlay, and asset provisioning
set(TOMODACHI_COMMON_SOURCES
    src/input/touch_overlay.cpp
    src/platform/android_storage.cpp
    src/platform/android_vulkan.cpp
)

# Third-party Dependencies
if(ANDROID)
    # SDL3 Subdirectory / FetchContent
    if(NOT TARGET SDL3::SDL3-static AND NOT TARGET SDL3::SDL3)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/SDL3/CMakeLists.txt")
            add_subdirectory(extern/SDL3 EXCLUDE_FROM_ALL)
        else()
            include(FetchContent)
            FetchContent_Declare(
                SDL3
                GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
                GIT_TAG release-3.2.0
                GIT_SHALLOW TRUE
            )
            FetchContent_MakeAvailable(SDL3)
        endif()
    endif()

    if(TARGET SDL3::SDL3-static)
        set(SDL3_TARGET SDL3::SDL3-static)
    elseif(TARGET SDL3::SDL3)
        set(SDL3_TARGET SDL3::SDL3)
    else()
        set(SDL3_TARGET SDL3-static)
    endif()

    # Shared Library target for Android NativeActivity
    # Must match android.app.lib_name declared in AndroidManifest.xml
    add_library(tomodachi_app SHARED
        ${TOMODACHI_COMMON_SOURCES}
        android/app/src/main/cpp/android_main.cpp
    )

    target_include_directories(tomodachi_app PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
        ${CMAKE_CURRENT_SOURCE_DIR}/src/input
    )

    target_link_libraries(tomodachi_app PRIVATE
        tomodachi_android_platform
        ${SDL3_TARGET}
    )

    # Export JNI and native entry symbols
    set_target_properties(tomodachi_app PROPERTIES
        OUTPUT_NAME "tomodachi_app"
        LINK_FLAGS "-Wl,--no-undefined -Wl,-z,max-page-size=16384"
    )

else()
    # Desktop build path (Linux / Windows)
    add_executable(tomodachi-ltd-recomp
        ${TOMODACHI_COMMON_SOURCES}
        src/main.cpp
    )
    find_package(SDL3 REQUIRED CONFIG)
    find_package(Vulkan REQUIRED)
    target_link_libraries(tomodachi-ltd-recomp PRIVATE
        SDL3::SDL3
        Vulkan::Vulkan
    )
endif()
