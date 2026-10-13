// ==============================================================================
// src/input/touch_overlay.cpp
// Low-Latency Virtual Touchscreen Controller Overlay Implementation
// ==============================================================================

#include "touch_overlay.hpp"
#include <cmath>
#include <algorithm>
#include <android/log.h>

#define LOG_TAG "TomodachiTouch"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

namespace Tomodachi::Input {

TouchOverlay& TouchOverlay::Instance() {
    static TouchOverlay s_instance;
    return s_instance;
}

TouchOverlay::TouchOverlay() {
    RecomputeHitboxes();
}

void TouchOverlay::UpdateDimensions(int32_t width, int32_t height) {
    if (width <= 0 || height <= 0) return;
    m_width = width;
    m_height = height;
    RecomputeHitboxes();
}

void TouchOverlay::RecomputeHitboxes() {
    m_circle_buttons.clear();
    m_rect_buttons.clear();

    const float fw = static_cast<float>(m_width);
    const float fh = static_cast<float>(m_height);

    // 1. Left Virtual Analog Stick Base
    m_stick_base_x = fw * 0.14f;
    m_stick_base_y = fh * 0.70f;
    m_stick_base_radius = fh * 0.20f;
    m_stick_knob_x = m_stick_base_x;
    m_stick_knob_y = m_stick_base_y;
    m_stick_knob_radius = m_stick_base_radius * 0.45f;

    // 2. D-Pad (Up, Down, Left, Right) positioned above stick or left-center
    const float dpad_cx = fw * 0.14f;
    const float dpad_cy = fh * 0.32f;
    const float dpad_btn_r = fh * 0.055f;
    const float dpad_offset = fh * 0.085f;

    m_circle_buttons.push_back({ dpad_cx, dpad_cy - dpad_offset, dpad_btn_r, BTN_DPAD_UP, "UP" });
    m_circle_buttons.push_back({ dpad_cx, dpad_cy + dpad_offset, dpad_btn_r, BTN_DPAD_DOWN, "DOWN" });
    m_circle_buttons.push_back({ dpad_cx - dpad_offset, dpad_cy, dpad_btn_r, BTN_DPAD_LEFT, "LEFT" });
    m_circle_buttons.push_back({ dpad_cx + dpad_offset, dpad_cy, dpad_btn_r, BTN_DPAD_RIGHT, "RIGHT" });

    // 3. Face Buttons (Nintendo Switch diamond layout: A right, B bottom, X top, Y left)
    const float face_cx = fw * 0.88f;
    const float face_cy = fh * 0.68f;
    const float face_btn_r = fh * 0.065f;
    const float face_offset = fh * 0.11f;

    m_circle_buttons.push_back({ face_cx + face_offset, face_cy, face_btn_r, BTN_A, "A" });
    m_circle_buttons.push_back({ face_cx, face_cy + face_offset, face_btn_r, BTN_B, "B" });
    m_circle_buttons.push_back({ face_cx, face_cy - face_offset, face_btn_r, BTN_X, "X" });
    m_circle_buttons.push_back({ face_cx - face_offset, face_cy, face_btn_r, BTN_Y, "Y" });

    // 4. Shoulder & Trigger Buttons
    const float shoulder_w = fw * 0.10f;
    const float shoulder_h = fh * 0.075f;

    // Left Shoulders (L & ZL)
    m_rect_buttons.push_back({ fw * 0.04f, fh * 0.03f, shoulder_w, shoulder_h, BTN_L, "L" });
    m_rect_buttons.push_back({ fw * 0.04f, fh * 0.12f, shoulder_w, shoulder_h, BTN_ZL, "ZL" });

    // Right Shoulders (R & ZR)
    m_rect_buttons.push_back({ fw * 0.86f, fh * 0.03f, shoulder_w, shoulder_h, BTN_R, "R" });
    m_rect_buttons.push_back({ fw * 0.86f, fh * 0.12f, shoulder_w, shoulder_h, BTN_ZR, "ZR" });

    // 5. System Buttons (+ and -)
    const float sys_w = fw * 0.06f;
    const float sys_h = fh * 0.055f;
    m_rect_buttons.push_back({ fw * 0.38f, fh * 0.04f, sys_w, sys_h, BTN_MINUS, "-" });
    m_rect_buttons.push_back({ fw * 0.56f, fh * 0.04f, sys_w, sys_h, BTN_PLUS, "+" });
}

void TouchOverlay::HandlePointerDown(int32_t pointer_id, float x, float y) {
    TouchPointer ptr;
    ptr.pointer_id = pointer_id;
    ptr.current_x = x;
    ptr.current_y = y;
    ptr.role = TouchElementRole::NONE;
    ptr.assigned_button = 0;

    // Check Virtual Left Analog Stick
    float dx = x - m_stick_base_x;
    float dy = y - m_stick_base_y;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= m_stick_base_radius * 1.3f && m_stick_pointer_id == -1) {
        m_stick_pointer_id = pointer_id;
        ptr.role = TouchElementRole::LEFT_STICK;
        HandlePointerMove(pointer_id, x, y);
        m_active_pointers[pointer_id] = ptr;
        RefreshControllerState();
        return;
    }

    // Check Circular Buttons (D-Pad & Face)
    for (const auto& btn : m_circle_buttons) {
        float bdx = x - btn.cx;
        float bdy = y - btn.cy;
        if (std::sqrt(bdx * bdx + bdy * bdy) <= btn.radius * 1.15f) {
            ptr.role = TouchElementRole::BUTTON;
            ptr.assigned_button = btn.mask;
            m_active_pointers[pointer_id] = ptr;
            RefreshControllerState();
            return;
        }
    }

    // Check Rectangular Buttons (Shoulders & System)
    for (const auto& btn : m_rect_buttons) {
        if (x >= btn.x && x <= btn.x + btn.width &&
            y >= btn.y && y <= btn.y + btn.height) {
            ptr.role = TouchElementRole::BUTTON;
            ptr.assigned_button = btn.mask;
            m_active_pointers[pointer_id] = ptr;
            RefreshControllerState();
            return;
        }
    }

    // Touch does not hit any controller widget: Treat as native Switch screen touch
    ptr.role = TouchElementRole::SCREEN_TOUCH_PASSTHROUGH;
    m_active_pointers[pointer_id] = ptr;
    RefreshControllerState();
}

void TouchOverlay::HandlePointerMove(int32_t pointer_id, float x, float y) {
    auto it = m_active_pointers.find(pointer_id);
    if (it == m_active_pointers.end()) return;

    it->second.current_x = x;
    it->second.current_y = y;

    if (it->second.role == TouchElementRole::LEFT_STICK && pointer_id == m_stick_pointer_id) {
        float dx = x - m_stick_base_x;
        float dy = y - m_stick_base_y;
        float dist = std::sqrt(dx * dx + dy * dy);

        const float max_dist = m_stick_base_radius;
        if (dist > max_dist) {
            dx = (dx / dist) * max_dist;
            dy = (dy / dist) * max_dist;
            dist = max_dist;
        }

        m_stick_knob_x = m_stick_base_x + dx;
        m_stick_knob_y = m_stick_base_y + dy;

        // Apply deadzone and normalize to [-1.0f, 1.0f]
        float norm_x = dx / max_dist;
        float norm_y = dy / max_dist;
        float mag = std::sqrt(norm_x * norm_x + norm_y * norm_y);
        const float deadzone = 0.12f;

        if (mag > deadzone) {
            float scale = (mag - deadzone) / (1.0f - deadzone);
            m_state.left_stick_x = (norm_x / mag) * scale;
            m_state.left_stick_y = -(norm_y / mag) * scale; // Invert Y for Switch coordinate system
        } else {
            m_state.left_stick_x = 0.0f;
            m_state.left_stick_y = 0.0f;
        }
    }

    RefreshControllerState();
}

void TouchOverlay::HandlePointerUp(int32_t pointer_id) {
    if (pointer_id == m_stick_pointer_id) {
        m_stick_pointer_id = -1;
        m_stick_knob_x = m_stick_base_x;
        m_stick_knob_y = m_stick_base_y;
        m_state.left_stick_x = 0.0f;
        m_state.left_stick_y = 0.0f;
    }

    m_active_pointers.erase(pointer_id);
    RefreshControllerState();
}

void TouchOverlay::RefreshControllerState() {
    uint64_t buttons = 0;
    bool screen_touch = false;
    float screen_x = 0.0f;
    float screen_y = 0.0f;

    for (const auto& [id, ptr] : m_active_pointers) {
        if (ptr.role == TouchElementRole::BUTTON) {
            buttons |= ptr.assigned_button;
        } else if (ptr.role == TouchElementRole::SCREEN_TOUCH_PASSTHROUGH) {
            screen_touch = true;
            screen_x = ptr.current_x / static_cast<float>(m_width);
            screen_y = ptr.current_y / static_cast<float>(m_height);
        }
    }

    m_state.buttons = buttons;
    m_state.touch_active = screen_touch;
    m_state.touch_x = std::clamp(screen_x, 0.0f, 1.0f);
    m_state.touch_y = std::clamp(screen_y, 0.0f, 1.0f);
}

bool TouchOverlay::ProcessAndroidMotionEvent(const AInputEvent* event) {
    int32_t action = AMotionEvent_getAction(event);
    int32_t actionCode = action & AMOTION_EVENT_ACTION_MASK;
    int32_t pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

    switch (actionCode) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN: {
            int32_t pointerId = AMotionEvent_getPointerId(event, pointerIndex);
            float x = AMotionEvent_getX(event, pointerIndex);
            float y = AMotionEvent_getY(event, pointerIndex);
            HandlePointerDown(pointerId, x, y);
            return true;
        }
        case AMOTION_EVENT_ACTION_MOVE: {
            size_t pointerCount = AMotionEvent_getPointerCount(event);
            for (size_t i = 0; i < pointerCount; ++i) {
                int32_t pointerId = AMotionEvent_getPointerId(event, i);
                float x = AMotionEvent_getX(event, i);
                float y = AMotionEvent_getY(event, i);
                HandlePointerMove(pointerId, x, y);
            }
            return true;
        }
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_POINTER_UP: {
            int32_t pointerId = AMotionEvent_getPointerId(event, pointerIndex);
            HandlePointerUp(pointerId);
            return true;
        }
        case AMOTION_EVENT_ACTION_CANCEL: {
            m_active_pointers.clear();
            m_stick_pointer_id = -1;
            m_stick_knob_x = m_stick_base_x;
            m_stick_knob_y = m_stick_base_y;
            m_state = VirtualControllerState{};
            return true;
        }
        default:
            break;
    }
    return false;
}

bool TouchOverlay::ProcessSDLEvent(const SDL_Event* event) {
    if (!event) return false;

    switch (event->type) {
        case SDL_EVENT_FINGER_DOWN: {
            int32_t id = static_cast<int32_t>(event->tfinger.fingerID);
            float x = event->tfinger.x * m_width;
            float y = event->tfinger.y * m_height;
            HandlePointerDown(id, x, y);
            return true;
        }
        case SDL_EVENT_FINGER_MOTION: {
            int32_t id = static_cast<int32_t>(event->tfinger.fingerID);
            float x = event->tfinger.x * m_width;
            float y = event->tfinger.y * m_height;
            HandlePointerMove(id, x, y);
            return true;
        }
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED: {
            int32_t id = static_cast<int32_t>(event->tfinger.fingerID);
            HandlePointerUp(id);
            return true;
        }
        default:
            break;
    }
    return false;
}

void TouchOverlay::GenerateGeometry(std::vector<OverlayVertex>& outVertices, std::vector<uint16_t>& outIndices) {
    if (!m_visible) return;

    outVertices.clear();
    outIndices.clear();

    const uint32_t base_color = (static_cast<uint32_t>(m_opacity * 255.0f) << 24) | 0x00FFFFFF;
    const uint32_t active_color = (static_cast<uint32_t>(m_opacity * 255.0f) << 24) | 0x0033CCFF;

    // Helper: Add solid disc
    auto AddCircle = [&](float cx, float cy, float radius, uint32_t color) {
        const int segments = 24;
        uint16_t centerIndex = static_cast<uint16_t>(outVertices.size());
        outVertices.push_back({ cx, cy, 0.5f, 0.5f, color });

        for (int i = 0; i <= segments; ++i) {
            float angle = static_cast<float>(i) * (2.0f * 3.14159265f / segments);
            float vx = cx + std::cos(angle) * radius;
            float vy = cy + std::sin(angle) * radius;
            outVertices.push_back({ vx, vy, 0.5f + 0.5f * std::cos(angle), 0.5f + 0.5f * std::sin(angle), color });
        }

        for (int i = 1; i <= segments; ++i) {
            outIndices.push_back(centerIndex);
            outIndices.push_back(centerIndex + i);
            outIndices.push_back(centerIndex + i + 1);
        }
    };

    // Helper: Add solid rectangle
    auto AddRect = [&](float x, float y, float w, float h, uint32_t color) {
        uint16_t baseIdx = static_cast<uint16_t>(outVertices.size());
        outVertices.push_back({ x, y, 0.0f, 0.0f, color });
        outVertices.push_back({ x + w, y, 1.0f, 0.0f, color });
        outVertices.push_back({ x + w, y + h, 1.0f, 1.0f, color });
        outVertices.push_back({ x, y + h, 0.0f, 1.0f, color });

        outIndices.push_back(baseIdx + 0);
        outIndices.push_back(baseIdx + 1);
        outIndices.push_back(baseIdx + 2);
        outIndices.push_back(baseIdx + 0);
        outIndices.push_back(baseIdx + 2);
        outIndices.push_back(baseIdx + 3);
    };

    // 1. Draw Left Stick
    AddCircle(m_stick_base_x, m_stick_base_y, m_stick_base_radius, 0x44FFFFFF);
    AddCircle(m_stick_knob_x, m_stick_knob_y, m_stick_knob_radius, 
              (m_stick_pointer_id != -1) ? active_color : base_color);

    // 2. Draw Circular Buttons
    for (const auto& btn : m_circle_buttons) {
        bool pressed = (m_state.buttons & btn.mask) != 0;
        AddCircle(btn.cx, btn.cy, btn.radius, pressed ? active_color : base_color);
    }

    // 3. Draw Rectangular Buttons
    for (const auto& btn : m_rect_buttons) {
        bool pressed = (m_state.buttons & btn.mask) != 0;
        AddRect(btn.x, btn.y, btn.width, btn.height, pressed ? active_color : base_color);
    }
}

} // namespace Tomodachi::Input

extern "C" {

void Recomp_Android_InjectPadState(Tomodachi::Input::VirtualControllerState* outState) {
    if (outState) {
        *outState = Tomodachi::Input::TouchOverlay::Instance().GetState();
    }
}

uint64_t Recomp_Android_GetButtonMask() {
    return Tomodachi::Input::TouchOverlay::Instance().GetState().buttons;
}

}
