// Windows platform layer: raw Win32 window + 32-bit DIB framebuffer presented
// with StretchDIBits. No external libraries needed; builds with MSVC or MinGW.
#include "platform.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <cstring>

class PlatformWin32 : public Platform {
public:
    HWND hwnd = nullptr;
    HDC dibDC = nullptr;
    HBITMAP dib = nullptr;
    void* dibBits = nullptr;
    BITMAPINFO bi{};
    int W = 0, H = 0;
    LARGE_INTEGER freq{}, t0{};

    static PlatformWin32* self;
    static LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == WM_DESTROY || msg == WM_CLOSE) { PostQuitMessage(0); return 0; }
        return DefWindowProcA(h, msg, wp, lp);
    }

    bool init(const char* title, int w, int h) override {
        W = w; H = h;
        HINSTANCE inst = GetModuleHandleA(nullptr);
        WNDCLASSA wc{};
        wc.lpfnWndProc = wndProc;
        wc.hInstance = inst;
        wc.lpszClassName = "TidewalkerWnd";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassA(&wc);
        hwnd = CreateWindowExA(0, "TidewalkerWnd", title,
                               WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                               CW_USEDEFAULT, CW_USEDEFAULT, w + 16, h + 39,
                               nullptr, nullptr, inst, nullptr);
        if (!hwnd) return false;
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        HDC screen = GetDC(hwnd);
        std::memset(&bi, 0, sizeof bi);
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -h;  // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
        ReleaseDC(hwnd, screen);
        if (!dib) return false;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&t0);
        return true;
    }

    static int mapKey(WPARAM vk) {
        switch (vk) {
            case VK_ESCAPE: return K_ESC;
            case 'W': return K_W; case 'A': return K_A; case 'S': return K_S;
            case 'D': return K_D; case 'E': return K_E; case 'R': return K_R;
            case 'T': return K_T;
            case VK_SPACE: return K_SPACE; case VK_SHIFT: return K_SHIFT;
            case VK_LEFT: return K_LEFT; case VK_RIGHT: return K_RIGHT;
            case VK_UP: return K_UP; case VK_DOWN: return K_DOWN;
            default: return -1;
        }
    }

    bool poll(InputEvent& e) override {
        MSG m;
        while (PeekMessageA(&m, nullptr, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) { e.type = InputEvent::Quit; e.key = -1; return true; }
            if (m.message == WM_KEYDOWN || m.message == WM_KEYUP) {
                int k = mapKey(m.wParam);
                if (k >= 0) {
                    e.type = (m.message == WM_KEYDOWN && !(m.lParam & 0x40000000)) ? InputEvent::KeyDown : InputEvent::KeyUp;
                    e.key = k;
                    return true;
                }
            }
            TranslateMessage(&m);
            DispatchMessageA(&m);
        }
        return false;
    }

    void present(const uint32_t* fb, int w, int h) override {
        if (w != W || h != H) return;
        std::memcpy(dibBits, fb, (size_t)w * h * 4);
        HDC hdc = GetDC(hwnd);
        StretchDIBits(hdc, 0, 0, w, h, 0, 0, w, h, dibBits, &bi, DIB_RGB_COLORS, SRCCOPY);
        ReleaseDC(hwnd, hdc);
    }

    void* nativeDisplay() override { return nullptr; }
    uintptr_t nativeWindow() override { return (uintptr_t)hwnd; }
    double now() override {
        LARGE_INTEGER c;
        QueryPerformanceCounter(&c);
        return (double)(c.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
    }
    void bell() override { MessageBeep(MB_ICONEXCLAMATION); }
    void sleepMs(int ms) override { Sleep(ms); }
};

Platform* createPlatform() { return new PlatformWin32(); }
#endif
