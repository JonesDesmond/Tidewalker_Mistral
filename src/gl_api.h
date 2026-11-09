#pragma once
// Headerless OpenGL 3.3 core loader: all functions are loaded at runtime via
// glXGetProcAddress (Linux, libGL via dlopen) or wglGetProcAddress (Windows,
// opengl32.dll), so the code compiles without any GL headers and links only
// against dl/opengl32.
#include <cstdint>
#include <stddef.h>

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef double GLdouble;
typedef ptrdiff_t GLsizeiptr;
typedef char GLchar;

struct GLAPI {
    // buffers / vaos
    void (*GenVertexArrays)(GLsizei, GLuint*);
    void (*BindVertexArray)(GLuint);
    void (*DeleteVertexArrays)(GLsizei, const GLuint*);
    void (*GenBuffers)(GLsizei, GLuint*);
    void (*BindBuffer)(GLenum, GLuint);
    void (*BufferData)(GLenum, GLsizeiptr, const void*, GLenum);
    void (*BufferSubData)(GLenum, ptrdiff_t, GLsizeiptr, const void*);
    void (*DeleteBuffers)(GLsizei, const GLuint*);
    void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
    void (*EnableVertexAttribArray)(GLuint);
    void (*DrawElements)(GLenum, GLsizei, GLenum, const void*);
    void (*DrawArrays)(GLenum, GLint, GLsizei);
    // shaders
    GLuint (*CreateShader)(GLenum);
    void (*ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
    void (*CompileShader)(GLuint);
    void (*GetShaderiv)(GLuint, GLenum, GLint*);
    void (*GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
    void (*DeleteShader)(GLuint);
    GLuint (*CreateProgram)(void);
    void (*AttachShader)(GLuint, GLuint);
    void (*LinkProgram)(GLuint);
    void (*GetProgramiv)(GLuint, GLenum, GLint*);
    void (*GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
    void (*UseProgram)(GLuint);
    void (*DeleteProgram)(GLuint);
    GLint (*GetUniformLocation)(GLuint, const GLchar*);
    void (*Uniform1i)(GLint, GLint);
    void (*Uniform1f)(GLint, GLfloat);
    void (*Uniform2f)(GLint, GLfloat, GLfloat);
    void (*Uniform3f)(GLint, GLfloat, GLfloat, GLfloat);
    void (*Uniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
    void (*Uniform3fv)(GLint, GLsizei, const GLfloat*);
    void (*UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
    // textures
    void (*GenTextures)(GLsizei, GLuint*);
    void (*DeleteTextures)(GLsizei, const GLuint*);
    void (*BindTexture)(GLenum, GLuint);
    void (*ActiveTexture)(GLenum);
    void (*TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
    void (*TexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*);
    void (*TexParameteri)(GLenum, GLenum, GLint);
    void (*GenerateMipmap)(GLenum);
    // fbos
    void (*GenFramebuffers)(GLsizei, GLuint*);
    void (*BindFramebuffer)(GLenum, GLuint);
    void (*FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
    GLenum (*CheckFramebufferStatus)(GLenum);
    void (*DeleteFramebuffers)(GLsizei, const GLuint*);
    // state
    void (*Viewport)(GLint, GLint, GLsizei, GLsizei);
    void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (*Clear)(GLuint);
    void (*Enable)(GLenum);
    void (*Disable)(GLenum);
    void (*BlendFunc)(GLenum, GLenum);
    void (*DepthFunc)(GLenum);
    void (*DepthMask)(GLboolean);
    void (*CullFace)(GLenum);
    void (*FrontFace)(GLenum);
};
extern GLAPI gl;

// Platform context: creates a 3.3 core GL context on the given native window.
// Linux: display = X11 Display*, window = X11 Window id.
// Windows: display = nullptr, window = HWND. Returns false if unavailable
// (caller should fall back to the software renderer).
bool glPlatformInit(void* display, uintptr_t window);
void glPlatformSwap();
void glPlatformShutdown();
bool glLoadFunctions();
