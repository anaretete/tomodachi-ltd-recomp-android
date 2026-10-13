// ==============================================================================
// src/input/touch_overlay.hpp
// Low-Latency Virtual Touchscreen Controller Overlay for Nintendo Switch Recomp
// ==============================================================================

#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <android/input.h>
#include <SDL3/SDL.h>

namespace Tomodachi::Input {

// Nintendo Switch Standard Controller Button Bitmasks
enum SwitchButton : uint64_t {
    BTN_A          = 1ULL << 0,
    BTN_B          = 1ULL << 1,
    BTN_X          = 1ULL << 2,
    BTN_Y          = 1ULL << 3,
    BTN_LSTICK     = 1ULL << 4,
    BTN_RSTICK     = 1ULL << 5,
    BTN_L          = 1ULL << 6,
    BTN_R          = 1ULL << 7,
    BTN_ZL         = 1ULL << 8,
    BTN_ZR         = 1ULL << 9,
    BTN_PLUS       = 1ULL << 10,
    BTN_MINUS      = 1ULL << 11,
    BTN_DPAD_LEFT  = 1ULL << 12,
    BTN_DPAD_UP    = 1ULL << 13,
    BTN_DPAD_RIGHT = 1ULL << 14,
    BTN_DPAD_DOWN  = 1ULL << 15,
};

// Recomp Virtual Gamepad State Buffer
struct VirtualControllerState {
    uint64_t buttons = 0;
    float left_stick_x = 0.0f;   // Normalized [-1.0f, 1.0f]
    float left_stick_y = 0.0f;   // Normalized [-1.0f, 1.0f]
    float right_stick_x = 0.0f;  // Normalized [-1.0f, 1.0f]
    float right_stick_y = 0.0f;  // Normalized [-1.0f, 1.0f]

    // Capacitive Screen Touch Emulation (Crucial for Tomodachi Life interactions)
    bool touch_active = false;
    float touch_x = 0.0f;        // Normalized [0.0f, 1.0f]
    float touch_y = 0.0f;        // Normalized [0.0f, 1.0f]
};

// 2D Vertex structure for Vulkan overlay rendering
struct OverlayVertex {
    float x, y;       // NDC or Pixel coordinates
    float u, v;       // Texture UV
    uint32_t color;   // RGBA8 packed
};

enum class TouchElementRole {
    NONE = 0,
    LEFT_STICK,
    BUTTON,
    SCREEN_TOUCH_PASSTHROUGH
};

struct TouchPointer {
    int32_t pointer_id = -1;
    float current_x = 0.0f;
    float current_y = 0.0f;
    TouchElementRole role = TouchElementRole::NONE;
    uint64_t assigned_button = 0;
};

class TouchOverlay {
public:
    static TouchOverlay& Instance();

    // Set screen viewport dimensions to recompute hitboxes
    void UpdateDimensions(int32_t width, int32_t height);

    // Ingest Native Android Motion Events directly
    bool ProcessAndroidMotionEvent(const AInputEvent* event);

    // Ingest SDL3 Finger Touch Events
    bool ProcessSDLEvent(const SDL_Event* event);

    // Retrieve active input state injected into the lifted recompiled engine
    const VirtualControllerState& GetState() const { return m_state; }

    // Generates 2D geometry for drawing virtual buttons over Vulkan viewport
    void GenerateGeometry(std::vector<OverlayVertex>& outVertices, std::vector<uint16_t>& outIndices);

    // Configuration
    void SetOverlayVisible(bool visible) { m_visible = visible; }
    bool IsOverlayVisible() const { return m_visible; }
    void SetOpacity(float alpha) { m_opacity = alpha; }

private:
    TouchOverlay();
    ~TouchOverlay() = default;

    void RecomputeHitboxes();
    void HandlePointerDown(int32_t pointer_id, float x, float y);
    void HandlePointerMove(int32_t pointer_id, float x, float y);
    void HandlePointerUp(int32_t pointer_id);
    void RefreshControllerState();

    int32_t m_width = 1920;
    int32_t m_height = 1080;
    bool m_visible = true;
    float m_opacity = 0.65f;

    VirtualControllerState m_state;
    std::unordered_map<int32_t, TouchPointer> m_active_pointers;

    // Hitbox definitions
    struct CircleButton {
        float cx, cy, radius;
        uint64_t mask;
        const char* label;
    };

    struct RectButton {
        float x, y, width, height;
        uint64_t mask;
        const char* label;
    };

    // Virtual Stick Center & Dimensions
    float m_stick_base_x = 0.0f;
    float m_stick_base_y = 0.0f;
    float m_stick_base_radius = 0.0f;
    float m_stick_knob_x = 0.0f;
    float m_stick_knob_y = 0.0f;
    float m_stick_knob_radius = 0.0f;
    int32_t m_stick_pointer_id = -1;

    std::vector<CircleButton> m_circle_buttons;
    std::vector<RectButton> m_rect_buttons;
};

// Global hook injected into lifted Switch binary code
extern "C" {
    void Recomp_Android_InjectPadState(VirtualControllerState* outState);
    uint64_t Recomp_Android_GetButtonMask();
}

} // namespace Tomodachi::Input
