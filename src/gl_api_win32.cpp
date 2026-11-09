// Windows GL loader: opengl32.dll + WGL_ARB_create_context.
#include "gl_api.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdio>

GLAPI gl;

typedef void* HGLRC;
typedef HGLRC (*PFN_wglCreateContextAttribsARB)(HDC, HGLRC, const int*);
typedef int (*PFN_wglMakeCurrent)(HDC, HGLRC);
typedef int (*PFN_wglDeleteContext)(HGLRC);
typedef void (*PFN_wglSwapBuffers)(HDC);  // actually BOOL SwapBuffers(HDC)
typedef void* (*PFN_wglGetProcAddress)(const char*);
typedef HDC (*PFN_wglGetCurrentDC)(void);

static HGLRC g_ctx = nullptr;
static HDC g_hdc = nullptr;

bool glLoadFunctions() {
    HMODULE m = LoadLibraryA("opengl32.dll");
    if (!m) { std::fprintf(stderr, "gl: opengl32.dll not found\n"); return false; }
    auto gp = (PFN_wglGetProcAddress)GetProcAddress(m, "wglGetProcAddress");
    // legacy functions come straight from opengl32
    #define LG(name) gl.name = (decltype(gl.name))GetProcAddress(m, #name); if (!gl.name && gp) gl.name = (decltype(gl.name))gp(#name); if (!gl.name) { std::fprintf(stderr, "gl: missing %s\n", #name); return false; }
    LG(GenVertexArrays); LG(BindVertexArray); LG(DeleteVertexArrays); LG(GenBuffers); LG(BindBuffer);
    LG(BufferData); LG(BufferSubData); LG(DeleteBuffers); LG(VertexAttribPointer); LG(EnableVertexAttribArray);
    LG(DrawElements); LG(DrawArrays); LG(CreateShader); LG(ShaderSource); LG(CompileShader); LG(GetShaderiv);
    LG(GetShaderInfoLog); LG(DeleteShader); LG(CreateProgram); LG(AttachShader); LG(LinkProgram);
    LG(GetProgramiv); LG(GetProgramInfoLog); LG(UseProgram); LG(DeleteProgram); LG(GetUniformLocation);
    LG(Uniform1i); LG(Uniform1f); LG(Uniform2f); LG(Uniform3f); LG(Uniform4f); LG(Uniform3fv); LG(UniformMatrix4fv);
    LG(GenTextures); LG(DeleteTextures); LG(BindTexture); LG(ActiveTexture); LG(TexImage2D); LG(TexSubImage2D);
    LG(TexParameteri); LG(GenerateMipmap); LG(GenFramebuffers); LG(BindFramebuffer); LG(FramebufferTexture2D);
    LG(CheckFramebufferStatus); LG(DeleteFramebuffers); LG(Viewport); LG(ClearColor); LG(Clear); LG(Enable);
    LG(Disable); LG(BlendFunc); LG(DepthFunc); LG(DepthMask); LG(CullFace); LG(FrontFace);
    #undef LG
    return true;
}

bool glPlatformInit(void* /*display*/, uintptr_t window) {
    if (!glLoadFunctions()) return false;
    g_hdc = GetDC((HWND)window);
    if (!g_hdc) return false;
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof pfd; pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cDepthBits = 24;
    int fmt = ChoosePixelFormat(g_hdc, &pfd);
    if (!fmt || !SetPixelFormat(g_hdc, fmt, &pfd)) return false;
    HGLRC tmp = wglCreateContext(g_hdc);
    wglMakeCurrent(g_hdc, tmp);
    auto gp = (PFN_wglGetProcAddress)GetProcAddress(GetModuleHandleA("opengl32.dll"), "wglGetProcAddress");
    auto pCreateAttribs = (PFN_wglCreateContextAttribsARB)gp("wglCreateContextAttribsARB");
    if (pCreateAttribs) {
        int attribs[] = { 0x2091, 3, 0x2092, 3, 0x9126, 0x00000001, 0 };
        g_ctx = pCreateAttribs(g_hdc, nullptr, attribs);
    }
    if (!g_ctx) g_ctx = tmp; else wglDeleteContext(tmp);
    wglMakeCurrent(g_hdc, g_ctx);
    std::printf("gl: WGL context created\n");
    return true;
}

void glPlatformSwap() { if (g_hdc) SwapBuffers(g_hdc); }
void glPlatformShutdown() { if (g_ctx) wglDeleteContext(g_ctx); g_ctx = nullptr; }
#endif
