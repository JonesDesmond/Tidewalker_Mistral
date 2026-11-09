#pragma once
#include <cstdint>

enum Key {
    K_ESC = 0, K_W, K_A, K_S, K_D, K_E, K_R, K_T,
    K_SPACE, K_SHIFT, K_LEFT, K_RIGHT, K_UP, K_DOWN,
    K_COUNT
};

struct InputEvent {
    enum Type { KeyDown, KeyUp, Quit } type;
    int key;
};

// Minimal windowing/input/audio-bell interface. Implemented per platform:
// Linux X11 (loaded via dlopen, no build-time deps) and Win32 (raw API + DIB).
struct Platform {
    virtual ~Platform() = default;
    virtual bool init(const char* title, int w, int h) = 0;
    virtual bool poll(InputEvent& e) = 0;                 // non-blocking
    virtual void present(const uint32_t* fb, int w, int h) = 0;  // fb: 0x00RRGGBB
    virtual double now() = 0;                              // seconds, monotonic
    virtual void bell() = 0;
    virtual void sleepMs(int ms) = 0;
    // Native handles for the GL backend: Linux returns (Display*, X Window id),
    // Windows returns (nullptr, HWND).
    virtual void* nativeDisplay() { return nullptr; }
    virtual uintptr_t nativeWindow() { return 0; }
};

Platform* createPlatform();
