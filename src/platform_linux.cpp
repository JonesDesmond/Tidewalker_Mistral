// Linux platform layer: X11 loaded at runtime via dlopen, so the binary has
// zero build-time dependencies and runs on any desktop Linux with libX11.
#include "platform.h"
#ifndef _WIN32
#include <dlfcn.h>
#include <time.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdint>

typedef struct _XDisplay Display;
typedef unsigned long XWindow;
typedef unsigned long XTime;
typedef struct _XGC* XGC;

// ABI-stable X11 structs (public layout from Xlib.h).
typedef struct _XImage {
    int width, height, xoffset, format;
    char* data;
    int byte_order, bitmap_unit, bitmap_bit_order, bitmap_pad, depth;
    int bytes_per_line, bits_per_pixel;
    unsigned long red_mask, green_mask, blue_mask;
    void* obdata;
    struct {
        void* create_image; void* destroy_image; void* get_pixel;
        void* put_pixel; void* sub_image; void* add_pixel;
    } f;
} XImage;

struct XKeyEventL {
    int type; unsigned long serial; int send_event; Display* display;
    XWindow window, root, subwindow; XTime time;
    int x, y, x_root, y_root; unsigned int state; unsigned int keycode; int same_screen;
};
union XEventL { int type; XKeyEventL xkey; char pad[192]; };

static Display* (*pXOpenDisplay)(const char*);
static int (*pXDefaultScreen)(Display*);
static XWindow (*pXRootWindow)(Display*, int);
static XWindow (*pXCreateSimpleWindow)(Display*, XWindow, int, int, unsigned, unsigned, unsigned, unsigned long, unsigned long);
static int (*pXSelectInput)(Display*, XWindow, long);
static int (*pXStoreName)(Display*, XWindow, const char*);
static int (*pXMapWindow)(Display*, XWindow);
static int (*pXFlush)(Display*);
static int (*pXSync)(Display*, int);
static int (*pXPending)(Display*);
static int (*pXNextEvent)(Display*, XEventL*);
static XGC (*pXCreateGC)(Display*, XWindow, unsigned long, void*);
static void* (*pXDefaultVisual)(Display*, int);
static int (*pXDefaultDepth)(Display*, int);
static XImage* (*pXCreateImage)(Display*, void*, unsigned, int, int, char*, unsigned, unsigned, int, int);
static int (*pXInitImage)(XImage*);
static int (*pXPutImage)(Display*, XWindow, XGC, XImage*, int, int, int, int, unsigned, unsigned);
static int (*pXDestroyImage)(XImage*);
static int (*pXCloseDisplay)(Display*);
static int (*pXBell)(Display*, int);

static bool loadX11() {
    void* h = dlopen("libX11.so.6", RTLD_NOW | RTLD_GLOBAL);
    if (!h) h = dlopen("libX11.so", RTLD_NOW | RTLD_GLOBAL);
    if (!h) return false;
    #define L(name) p##name = (decltype(p##name))dlsym(h, #name); if (!p##name) return false;
    L(XOpenDisplay); L(XDefaultScreen); L(XRootWindow); L(XCreateSimpleWindow);
    L(XSelectInput); L(XStoreName); L(XMapWindow); L(XFlush); L(XSync); L(XPending);
    L(XNextEvent); L(XCreateGC); L(XDefaultVisual); L(XDefaultDepth); L(XCreateImage);
    L(XInitImage); L(XPutImage); L(XDestroyImage); L(XCloseDisplay); L(XBell);
    #undef L
    return true;
}

class PlatformLinux : public Platform {
public:
    Display* dpy = nullptr;
    XWindow win = 0;
    XGC gc = nullptr;
    XImage* img = nullptr;
    char* imgData = nullptr;
    int W = 0, H = 0;
    bool ok = false;

    bool init(const char* title, int w, int h) override {
        if (!loadX11()) { std::fprintf(stderr, "tidewalker: libX11 not found\n"); return false; }
        dpy = pXOpenDisplay(nullptr);
        if (!dpy) { std::fprintf(stderr, "tidewalker: cannot open X display (set DISPLAY)\n"); return false; }
        int scr = pXDefaultScreen(dpy);
        XWindow root = pXRootWindow(dpy, scr);
        win = pXCreateSimpleWindow(dpy, root, 0, 0, w, h, 0, 0, 0);
        pXSelectInput(dpy, win, (1L << 0) | (1L << 1) | (1L << 17));  // KeyPress|KeyRelease|StructureNotify
        pXStoreName(dpy, win, title);
        pXMapWindow(dpy, win);
        pXFlush(dpy);
        gc = pXCreateGC(dpy, win, 0, nullptr);
        W = w; H = h;
        imgData = new char[(size_t)w * h * 4];
        img = pXCreateImage(dpy, pXDefaultVisual(dpy, scr), pXDefaultDepth(dpy, scr), 2 /*ZPixmap*/, 0,
                            imgData, w, h, 32, 0);
        if (!img) return false;
        img->byte_order = 0;  // LSBFirst
        img->bitmap_unit = 32; img->bitmap_bit_order = 0; img->bitmap_pad = 32;
        img->bits_per_pixel = 32; img->bytes_per_line = w * 4;
        img->red_mask = 0xFF0000; img->green_mask = 0xFF00; img->blue_mask = 0xFF;
        pXInitImage(img);
        pXSync(dpy, 0);
        ok = true;
        return true;
    }

    static int mapKey(unsigned int kc) {
        switch (kc) {
            case 9: return K_ESC; case 25: return K_W; case 38: return K_A;
            case 39: return K_S; case 40: return K_D; case 26: return K_E;
            case 27: return K_R; case 28: return K_T; case 65: return K_SPACE;
            case 50: return K_SHIFT; case 113: return K_LEFT; case 114: return K_RIGHT;
            case 111: return K_UP; case 116: return K_DOWN;
            default: return -1;
        }
    }

    bool poll(InputEvent& e) override {
        if (!ok) return false;
        while (pXPending(dpy) > 0) {
            XEventL ev;
            pXNextEvent(dpy, &ev);
            if (ev.type == 2 || ev.type == 3) {  // KeyPress / KeyRelease
                int k = mapKey(ev.xkey.keycode);
                if (k >= 0) {
                    e.type = ev.type == 2 ? InputEvent::KeyDown : InputEvent::KeyUp;
                    e.key = k;
                    return true;
                }
            }
        }
        return false;
    }

    void present(const uint32_t* fb, int w, int h) override {
        if (!ok || w != W || h != H) return;
        std::memcpy(imgData, fb, (size_t)w * h * 4);
        pXPutImage(dpy, win, gc, img, 0, 0, 0, 0, w, h);
        pXFlush(dpy);
    }

    void* nativeDisplay() override { return ok ? (void*)dpy : nullptr; }
    uintptr_t nativeWindow() override { return (uintptr_t)win; }
    double now() override {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec + ts.tv_nsec * 1e-9;
    }
    void bell() override { if (ok) { pXBell(dpy, 60); pXFlush(dpy); } }
    void sleepMs(int ms) override { usleep(ms * 1000); }
};

Platform* createPlatform() { return new PlatformLinux(); }
#endif
