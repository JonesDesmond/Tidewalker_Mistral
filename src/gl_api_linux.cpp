// Linux GL loader: libGL.so.1 via dlopen + GLX context creation.
#include "gl_api.h"
#ifndef _WIN32
#include <dlfcn.h>
#include <cstdio>
#include <cstring>

GLAPI gl;

typedef void* Display;
typedef unsigned long XWindowID;
typedef struct { int visualid; } XVisualInfo;
typedef void* GLXContext;
typedef void* GLXFBConfig;
typedef XVisualInfo* (*PFN_glXChooseVisual)(Display*, int, int*);
typedef GLXFBConfig* (*PFN_glXChooseFBConfig)(Display*, int, const int*, int*);
typedef XVisualInfo* (*PFN_glXGetVisualFromFBConfig)(Display*, GLXFBConfig);
typedef GLXContext (*PFN_glXCreateContextAttribsARB)(Display*, GLXFBConfig, GLXContext, int, const int*);
typedef int (*PFN_glXMakeCurrent)(Display*, XWindowID, GLXContext);
typedef void (*PFN_glXSwapBuffers)(Display*, XWindowID);
typedef void (*PFN_glXDestroyContext)(Display*, GLXContext);
typedef void* (*PFN_glXGetProcAddressARB)(const unsigned char*);

static void* libGL = nullptr;
static Display* g_dpy = nullptr;
static XWindowID g_win = 0;
static GLXContext g_ctx = nullptr;
static PFN_glXSwapBuffers pSwap = nullptr;
static PFN_glXMakeCurrent pMakeCurrent = nullptr;
static PFN_glXDestroyContext pDestroy = nullptr;

bool glLoadFunctions() {
    if (libGL) return true;
    libGL = dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (!libGL) { std::fprintf(stderr, "gl: libGL.so.1 not found\n"); return false; }
    auto gp = (PFN_glXGetProcAddressARB)dlsym(libGL, "glXGetProcAddressARB");
    if (!gp) gp = (PFN_glXGetProcAddressARB)dlsym(libGL, "glXGetProcAddress");
    if (!gp) return false;
    #define L(name) gl.name = (decltype(gl.name))gp((const unsigned char*)#name); if (!gl.name) { std::fprintf(stderr, "gl: missing %s\n", #name); return false; }
    L(GenVertexArrays); L(BindVertexArray); L(DeleteVertexArrays); L(GenBuffers); L(BindBuffer);
    L(BufferData); L(BufferSubData); L(DeleteBuffers); L(VertexAttribPointer); L(EnableVertexAttribArray);
    L(DrawElements); L(DrawArrays); L(CreateShader); L(ShaderSource); L(CompileShader); L(GetShaderiv);
    L(GetShaderInfoLog); L(DeleteShader); L(CreateProgram); L(AttachShader); L(LinkProgram);
    L(GetProgramiv); L(GetProgramInfoLog); L(UseProgram); L(DeleteProgram); L(GetUniformLocation);
    L(Uniform1i); L(Uniform1f); L(Uniform2f); L(Uniform3f); L(Uniform4f); L(Uniform3fv); L(UniformMatrix4fv);
    L(GenTextures); L(DeleteTextures); L(BindTexture); L(ActiveTexture); L(TexImage2D); L(TexSubImage2D);
    L(TexParameteri); L(GenerateMipmap); L(GenFramebuffers); L(BindFramebuffer); L(FramebufferTexture2D);
    L(CheckFramebufferStatus); L(DeleteFramebuffers); L(Viewport); L(ClearColor); L(Clear); L(Enable);
    L(Disable); L(BlendFunc); L(DepthFunc); L(DepthMask); L(CullFace); L(FrontFace);
    #undef L
    pSwap = (PFN_glXSwapBuffers)dlsym(libGL, "glXSwapBuffers");
    pMakeCurrent = (PFN_glXMakeCurrent)dlsym(libGL, "glXMakeCurrent");
    pDestroy = (PFN_glXDestroyContext)dlsym(libGL, "glXDestroyContext");
    auto pChooseFB = (PFN_glXChooseFBConfig)dlsym(libGL, "glXChooseFBConfig");
    auto pGetVisual = (PFN_glXGetVisualFromFBConfig)dlsym(libGL, "glXGetVisualFromFBConfig");
    auto pCreateAttribs = (PFN_glXCreateContextAttribsARB)dlsym(libGL, "glXCreateContextAttribsARB");
    (void)pChooseFB; (void)pGetVisual; (void)pCreateAttribs;
    return pSwap && pMakeCurrent && pDestroy;
}

bool glPlatformInit(void* xdisplay, uintptr_t window) {
    g_dpy = (Display*)xdisplay;
    g_win = (XWindowID)window;
    if (!glLoadFunctions()) return false;
    if (!g_win) { std::fprintf(stderr, "gl: no X window\n"); return false; }
    auto pChooseFB = (PFN_glXChooseFBConfig)dlsym(libGL, "glXChooseFBConfig");
    auto pGetVisual = (PFN_glXGetVisualFromFBConfig)dlsym(libGL, "glXGetVisualFromFBConfig");
    auto pCreateAttribs = (PFN_glXCreateContextAttribsARB)dlsym(libGL, "glXCreateContextAttribsARB");
    if (!pChooseFB || !pGetVisual || !pCreateAttribs) { std::fprintf(stderr, "gl: GLX 1.3+ required\n"); return false; }
    int fbAttribs[] = {
        0x8011 /*GLX_RENDER_TYPE*/, 0x00000001 /*GLX_RGBA_BIT*/,
        0x8012 /*GLX_DRAWABLE_TYPE*/, 0x00000001 /*GLX_WINDOW_BIT*/,
        5 /*GLX_DOUBLEBUFFER*/, 1,
        8 /*GLX_RED_SIZE*/, 8, 9 /*GREEN*/, 8, 10 /*BLUE*/, 8, 11 /*ALPHA*/, 8,
        12 /*GLX_DEPTH_SIZE*/, 24,
        0
    };
    int nfb = 0;
    GLXFBConfig* fbs = pChooseFB(g_dpy, 0 /*DefaultScreen*/, fbAttribs, &nfb);
    if (!fbs || nfb < 1) { std::fprintf(stderr, "gl: no FBConfig\n"); return false; }
    XVisualInfo* vi = pGetVisual(g_dpy, fbs[0]);
    (void)vi;
    int ctxAttribs[] = {
        0x2091 /*MAJOR*/, 3, 0x2092 /*MINOR*/, 3,
        0x9126 /*PROFILE_MASK*/, 0x00000001 /*CORE*/,
        0
    };
    g_ctx = pCreateAttribs(g_dpy, fbs[0], nullptr, 1, ctxAttribs);
    if (!g_ctx) { std::fprintf(stderr, "gl: glXCreateContextAttribsARB failed\n"); return false; }
    if (!pMakeCurrent(g_dpy, g_win, g_ctx)) { std::fprintf(stderr, "gl: makeCurrent failed\n"); return false; }
    std::printf("gl: OpenGL context created\n");
    const char* ver = (const char*)((PFN_glXGetProcAddressARB)dlsym(libGL, "glGetString")) ? nullptr : nullptr;
    (void)ver;
    return true;
}

void glPlatformSwap() { if (pSwap && g_dpy && g_win) pSwap(g_dpy, g_win); }
void glPlatformShutdown() { if (pDestroy && g_dpy && g_ctx) pDestroy(g_dpy, g_ctx); g_ctx = nullptr; }
#endif
