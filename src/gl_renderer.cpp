#include "gl_renderer.h"
#include "gl_api.h"
#include "font.h"
#include <cstdio>
#include <cstring>
#include <cmath>

// GL constants
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA8 0x8058
#define GL_BGRA 0x80E1
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
#define GL_R32F 0x822E
#define GL_RED 0x1903
#define GL_R8 0x8229
#define GL_LINEAR 0x2601
#define GL_NEAREST 0x2600
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_TRIANGLES 0x0004
#define GL_UNSIGNED_INT 0x1405
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_LEQUAL 0x0203
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x0100
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_TEXTURE2 0x84C2
#define GL_TEXTURE3 0x84C3
#define GL_CULL_FACE 0x0B44

static const char* VS_MESH = R"GLSL(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec3 aCol;
layout(location=3) in vec2 aUV;
layout(location=4) in vec3 aSplat;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform float uWater;
uniform float uSea;
uniform float uTime;
out vec3 vN; out vec3 vCol; out vec2 vUV; out vec3 vSplat; out vec3 vWP;
float waveFn(float x, float z, float t){
    return 0.16*sin(0.11*x+1.3*t)+0.11*sin(0.13*z-1.7*t)+0.07*sin(0.07*(x+z)+0.8*t);
}
void main(){
    vec4 wp = uModel*vec4(aPos,1.0);
    vec3 n = mat3(uModel)*aNrm;
    if (uWater > 0.5) {
        float e = 0.6;
        float w0 = waveFn(wp.x, wp.z, uTime);
        wp.y = uSea + w0;
        float dnx = -(waveFn(wp.x+e, wp.z, uTime)-w0)/e;
        float dnz = -(waveFn(wp.x, wp.z+e, uTime)-w0)/e;
        n = normalize(vec3(dnx, 1.0, dnz));
    }
    vWP = wp.xyz; vN = n; vCol = aCol; vUV = aUV; vSplat = aSplat;
    gl_Position = uMVP*vec4(aPos,1.0);
}
)GLSL";

static const char* FS_MESH = R"GLSL(#version 330 core
in vec3 vN; in vec3 vCol; in vec2 vUV; in vec3 vSplat; in vec3 vWP;
out vec4 o;
uniform sampler2D uTex0, uTex1, uTex2, uHeight;
uniform int uHasSplat, uWater, uEmissive, uAlphaTest, uShadow;
uniform vec3 uTint;
uniform float uAlpha;
uniform vec3 uSunDir, uSunCol, uFogCol, uLanPos, uLanCol, uCamPos;
uniform float uSunI, uAmb, uFogDens, uLanI, uSea, uTime;
uniform float uWorldSize;
float heightAt(vec2 xz){ return texture(uHeight, (xz + uWorldSize*0.5)/uWorldSize).r; }
float waveFn(float x, float z, float t){
    return 0.16*sin(0.11*x+1.3*t)+0.11*sin(0.13*z-1.7*t)+0.07*sin(0.07*(x+z)+0.8*t);
}
float shadowFactor(vec2 xz){
    vec3 sd = uSunDir;
    if (sd.y < 0.03) return 1.0;
    vec2 dir = sd.xz / max(sd.y, 0.05);
    float hh = heightAt(xz) + 1.0;
    for (int i = 1; i < 28; i++) {
        vec2 p = xz + dir * (float(i) * 1.6);
        if (abs(p.x) > uWorldSize*0.5 || abs(p.y) > uWorldSize*0.5) break;
        if (heightAt(p) > hh) return 0.35;
        hh += 1.6;
    }
    return 1.0;
}
void main(){
    vec3 alb = vCol * uTint;
    float alpha = uAlpha;
    if (uHasSplat > 0) {
        vec3 t1 = texture(uTex0, vUV).rgb;
        vec3 t2 = texture(uTex1, vUV*1.7 + 0.31).rgb;
        vec3 t3 = texture(uTex2, vUV*2.3 + 0.53).rgb;
        alb *= t1*vSplat.x + t2*vSplat.y + t3*vSplat.z;
    } else {
        vec4 t = texture(uTex0, vUV);
        alb *= t.rgb;
        alpha *= t.a;
        if (uAlphaTest > 0 && t.a < 0.5) discard;
    }
    vec3 N = normalize(vN);
    vec3 col;
    if (uWater > 0.5) {
        // micro ripple
        N.xz += vec2(0.09*sin(vWP.x*1.9+uTime*3.1)*cos(vWP.z*1.3-uTime*2.2),
                     0.09*cos(vWP.x*1.1-uTime*2.7)*sin(vWP.z*2.1+uTime*2.9));
        N = normalize(N);
        float h = heightAt(vWP.xz);
        float depth = uSea - h;
        vec3 deep = vec3(0.015,0.075,0.115), shallow = vec3(0.20,0.33,0.31);
        col = mix(shallow, deep, smoothstep(0.25, 3.0, depth));
        float foam = (1.0 - smoothstep(0.03, 0.5, abs(uSea - h))) * 0.85;
        foam *= 0.65 + 0.35*sin(uTime*4.0 + vWP.x*0.9 + vWP.z*0.7);
        col = mix(col, vec3(0.92,0.96,0.97), clamp(foam,0.0,1.0));
        vec3 V = normalize(uCamPos - vWP);
        float fres = pow(1.0 - max(0.0, dot(V,N)), 3.0);
        col = mix(col, uFogCol*1.15, fres*0.8);
        vec3 Hv = normalize(uSunDir + V);
        col += uSunCol * pow(max(0.0, dot(N,Hv)), 220.0) * uSunI;
        alpha = 0.78 + 0.2*fres;
    } else if (uEmissive > 0) {
        col = alb;
    } else {
        float diff = max(0.0, dot(N, uSunDir));
        float sh = uShadow > 0 ? shadowFactor(vWP.xz) : 1.0;
        col = alb * (uAmb + uSunI * diff * sh * uSunCol);
        vec3 ld = uLanPos - vWP;
        float d2 = dot(ld,ld);
        float att = uLanI / (1.0 + d2*0.05);
        col += alb * uLanCol * att * max(0.0, dot(N, normalize(ld)));
    }
    float dist = length(vWP - uCamPos);
    float f = 1.0 - exp(-pow(dist*uFogDens, 2.0));
    col = mix(col, uFogCol, f);
    o = vec4(col, alpha);
}
)GLSL";

static const char* VS_SKY = R"GLSL(#version 330 core
out vec2 vUV;
void main(){
    vec2 p = vec2(float((gl_VertexID<<1)&2), float(gl_VertexID&2));
    vUV = p;
    gl_Position = vec4(p*2.0-1.0, 0.0, 1.0);
}
)GLSL";

static const char* FS_SKY = R"GLSL(#version 330 core
in vec2 vUV;
out vec4 o;
uniform vec3 uTop, uHor, uSunCol;
uniform sampler2D uClouds;
uniform float uCloudAmt, uSunGlow, uNight, uTime, uDusk;
uniform vec2 uSunXY, uMoonXY;
uniform vec2 uRes;
float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1,311.7)))*43758.5453); }
void main(){
    vec3 col = mix(uHor, uTop, vUV.y);
    vec2 sp = uSunXY * uRes;
    float d = length(vUV*uRes - sp) / (uRes.y*0.55);
    float gl = pow(max(0.0, 1.0-d), 3.0) * uSunGlow;
    col += uSunCol * gl;
    if (uNight > 0.01) {
        float h = hash(floor(vUV*uRes));
        if (h > 0.9982) col += vec3(1.0,1.0,1.1) * (h-0.9982)*5000.0*uNight*0.4;
        float md = length(vUV*uRes - uMoonXY*uRes);
        float mr = uRes.y*0.018;
        if (md < mr) col = vec3(0.72,0.78,0.88);
        else if (md < mr*3.0) col += vec3(0.7,0.75,0.85)*pow(max(0.0,1.0-md/(mr*3.0)),3.0)*0.35*uNight;
    }
    if (uCloudAmt > 0.01) {
        float lum = texture(uClouds, vec2(vUV.x*3.0 + uTime*0.006, vUV.y*1.6+0.3)).r;
        vec3 cc = mix(vec3(0.10,0.12,0.20), vec3(1.0,0.98,0.95), 1.0-uNight) + uDusk*vec3(0.5,0.15,0.0);
        float amt = clamp(lum*1.3, 0.0, 1.0) * uCloudAmt * (1.0 - vUV.y*0.55);
        col = mix(col, cc, amt);
    }
    o = vec4(col, 1.0);
}
)GLSL";

static const char* VS_POST = R"GLSL(#version 330 core
out vec2 vUV;
void main(){
    vec2 p = vec2(float((gl_VertexID<<1)&2), float(gl_VertexID&2));
    vUV = p;
    gl_Position = vec4(p*2.0-1.0, 0.0, 1.0);
}
)GLSL";

static const char* FS_BRIGHT = R"GLSL(#version 330 core
in vec2 vUV; out vec4 o;
uniform sampler2D uTex;
void main(){
    vec3 c = texture(uTex, vUV).rgb;
    float lum = dot(c, vec3(0.5,0.35,0.15));
    float k = clamp((lum-0.72)*3.0, 0.0, 1.0);
    o = vec4(c*k, 1.0);
}
)GLSL";

static const char* FS_BLUR = R"GLSL(#version 330 core
in vec2 vUV; out vec4 o;
uniform sampler2D uTex;
uniform vec2 uDir;
void main(){
    vec3 s = texture(uTex, vUV).rgb * 0.227;
    s += texture(uTex, vUV + uDir*1.384).rgb * 0.316;
    s += texture(uTex, vUV - uDir*1.384).rgb * 0.316;
    s += texture(uTex, vUV + uDir*3.230).rgb * 0.070;
    s += texture(uTex, vUV - uDir*3.230).rgb * 0.070;
    o = vec4(s, 1.0);
}
)GLSL";

static const char* FS_COMPOSITE = R"GLSL(#version 330 core
in vec2 vUV; out vec4 o;
uniform sampler2D uScene, uBloom;
uniform float uVig;
uniform uint uFrame;
float hash(vec2 p){ return fract(sin(dot(p, vec2(127.1,311.7)))*43758.5453); }
vec3 aces(vec3 c){ return clamp((c*(2.51*c+0.03))/(c*(2.43*c+0.59)+0.14), 0.0, 1.0); }
void main(){
    vec3 c = texture(uScene, vUV).rgb + texture(uBloom, vUV).rgb*0.55;
    c = aces(c);
    float lum = dot(c, vec3(0.3,0.55,0.15));
    c = mix(vec3(lum), c, 1.12);
    vec2 n = vUV*2.0-1.0;
    c *= 1.0 - uVig*(n.x*n.x+n.y*n.y)*0.5;
    float g = (hash(vUV*913.0 + float(uFrame))*2.0-1.0)*0.02;
    o = vec4(clamp(c+g, 0.0, 1.0), 1.0);
}
)GLSL";

static const char* VS_HUD = R"GLSL(#version 330 core
layout(location=0) in vec2 aPos;
uniform mat4 uOrtho;
out vec2 vUV;
void main(){ vUV = aPos; gl_Position = uOrtho*vec4(aPos, 0.0, 1.0); }
)GLSL";

static const char* FS_RECT = R"GLSL(#version 330 core
in vec2 vUV; out vec4 o;
uniform vec4 uColor;
void main(){ o = uColor; }
)GLSL";

static const char* FS_TEXT = R"GLSL(#version 330 core
in vec2 vUV; out vec4 o;
uniform sampler2D uFont;
uniform vec4 uColor;
void main(){
    float a = texture(uFont, vUV).r;
    o = vec4(uColor.rgb, uColor.a * a);
}
)GLSL";

// ----------------------------------------------------------------

GLRenderer::GLRenderer(int w, int h) {
    W = w; H = h;
    if (!glLoadFunctions()) return;
    progMesh = link(compile(GL_VERTEX_SHADER, VS_MESH), compile(GL_FRAGMENT_SHADER, FS_MESH));
    progSky = link(compile(GL_VERTEX_SHADER, VS_SKY), compile(GL_FRAGMENT_SHADER, FS_SKY));
    progBright = link(compile(GL_VERTEX_SHADER, VS_POST), compile(GL_FRAGMENT_SHADER, FS_BRIGHT));
    progBlur = link(compile(GL_VERTEX_SHADER, VS_POST), compile(GL_FRAGMENT_SHADER, FS_BLUR));
    progComposite = link(compile(GL_VERTEX_SHADER, VS_POST), compile(GL_FRAGMENT_SHADER, FS_COMPOSITE));
    progRect = link(compile(GL_VERTEX_SHADER, VS_HUD), compile(GL_FRAGMENT_SHADER, FS_RECT));
    progText = link(compile(GL_VERTEX_SHADER, VS_HUD), compile(GL_FRAGMENT_SHADER, FS_TEXT));
    if (!progMesh || !progSky || !progBright || !progBlur || !progComposite || !progRect || !progText) {
        std::fprintf(stderr, "gl: shader compile failed\n");
        return;
    }
    gl.GenVertexArrays(1, &vaoScratch);
    gl.GenBuffers(1, &vboScratch);
    gl.GenBuffers(1, &iboScratch);
    gl.GenVertexArrays(1, &vaoSky);
    // white 1x1
    gl.GenTextures(1, &texWhite);
    gl.BindTexture(GL_TEXTURE_2D, texWhite);
    uint32_t white = 0xFFFFFFFF;
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_BGRA, GL_UNSIGNED_BYTE, &white);
    // scene FBO
    gl.GenTextures(1, &texScene);
    gl.BindTexture(GL_TEXTURE_2D, texScene);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.GenFramebuffers(1, &fboScene);
    gl.BindFramebuffer(GL_FRAMEBUFFER, fboScene);
    gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texScene, 0);
    // bloom FBOs (1/4 res)
    int bw = std::max(1, W / 4), bh = std::max(1, H / 4);
    auto makeBloom = [&](unsigned& fbo, unsigned& tex) {
        gl.GenTextures(1, &tex);
        gl.BindTexture(GL_TEXTURE_2D, tex);
        gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, bw, bh, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl.GenFramebuffers(1, &fbo);
        gl.BindFramebuffer(GL_FRAMEBUFFER, fbo);
        gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    };
    makeBloom(fboBloomA, texBloomA);
    makeBloom(fboBloomB, texBloomB);
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    // font atlas: 128 cells of 8x8 in a 1024x8 R8 texture
    if (g_font.loaded) {
        std::vector<uint8_t> atlas(128 * 8 * 8, 0);
        for (int c = 0; c < 128; c++)
            for (int col = 0; col < 5; col++)
                for (int row = 0; row < 7; row++)
                    if (g_font.cols[c][col] & (1u << row))
                        atlas[(row * 1024) + c * 8 + col] = 255;
        gl.GenTextures(1, &fontTex);
        gl.BindTexture(GL_TEXTURE_2D, fontTex);
        gl.TexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1024, 8, 0, GL_RED, GL_UNSIGNED_BYTE, atlas.data());
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    gl.Enable(GL_DEPTH_TEST);
    gl.DepthFunc(GL_LEQUAL);
    gl.Enable(GL_CULL_FACE);
    gl.CullFace(0x0405);  // GL_BACK
    gl.FrontFace(0x0408); // GL_CCW
    ok = true;
    std::printf("gl: GLRenderer ready (%dx%d)\n", W, H);
}

GLRenderer::~GLRenderer() {}

GLint GLRenderer::uni(unsigned prog, const char* name) { return gl.GetUniformLocation(prog, name); }

unsigned GLRenderer::compile(unsigned type, const char* src) {
    unsigned s = gl.CreateShader(type);
    gl.ShaderSource(s, 1, &src, nullptr);
    gl.CompileShader(s);
    int okc = 0;
    gl.GetShaderiv(s, GL_COMPILE_STATUS, &okc);
    if (!okc) {
        char log[2048];
        gl.GetShaderInfoLog(s, 2048, nullptr, log);
        std::fprintf(stderr, "gl: shader error: %s\n", log);
        return 0;
    }
    return s;
}
unsigned GLRenderer::link(unsigned vs, unsigned fs) {
    unsigned p = gl.CreateProgram();
    gl.AttachShader(p, vs);
    gl.AttachShader(p, fs);
    gl.LinkProgram(p);
    int okl = 0;
    gl.GetProgramiv(p, GL_LINK_STATUS, &okl);
    if (!okl) {
        char log[2048];
        gl.GetProgramInfoLog(p, 2048, nullptr, log);
        std::fprintf(stderr, "gl: link error: %s\n", log);
        return 0;
    }
    gl.DeleteShader(vs);
    gl.DeleteShader(fs);
    return p;
}

unsigned GLRenderer::texFor(const Tex* t) {
    if (!t) return texWhite;
    auto it = texCache.find(t);
    if (it != texCache.end()) return it->second;
    unsigned id;
    gl.GenTextures(1, &id);
    gl.BindTexture(GL_TEXTURE_2D, id);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, t->w, t->h, 0, GL_BGRA, GL_UNSIGNED_BYTE, t->px.data());
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 0x2703);  // LINEAR_MIPMAP_LINEAR
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    gl.GenerateMipmap(GL_TEXTURE_2D);
    texCache[t] = id;
    return id;
}

void GLRenderer::setHeightmap(const float* hm, int n, float ws) {
    heightN = n;
    worldSize = ws;
    if (!ok) return;
    gl.GenTextures(1, &texHeight);
    gl.BindTexture(GL_TEXTURE_2D, texHeight);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_R32F, n, n, 0, GL_RED, GL_FLOAT, hm);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void GLRenderer::resetDepth() {
    gl.BindFramebuffer(GL_FRAMEBUFFER, fboScene);
    gl.Viewport(0, 0, W, H);
    gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GLRenderer::skyPass(uint32_t topC, uint32_t horC, const Tex* clouds, float cloudAmt,
                         int sunX, int sunY, float sunGlow, uint32_t sunC,
                         int moonX, int moonY, uint32_t moonC, float night, float t, float dusk) {
    if (!ok) return;
    gl.BindFramebuffer(GL_FRAMEBUFFER, fboScene);
    gl.Viewport(0, 0, W, H);
    gl.Disable(GL_DEPTH_TEST);
    gl.Disable(GL_BLEND);
    gl.UseProgram(progSky);
    auto c3 = [&](uint32_t c) { return Vec3(((c >> 16) & 255) / 255.f, ((c >> 8) & 255) / 255.f, (c & 255) / 255.f); };
    Vec3 top = c3(topC), hor = c3(horC), sc = c3(sunC);
    gl.Uniform3f(uni(progSky, "uTop"), top.x, top.y, top.z);
    gl.Uniform3f(uni(progSky, "uHor"), hor.x, hor.y, hor.z);
    gl.Uniform3f(uni(progSky, "uSunCol"), sc.x, sc.y, sc.z);
    gl.Uniform1f(uni(progSky, "uCloudAmt"), cloudAmt);
    gl.Uniform1f(uni(progSky, "uSunGlow"), sunGlow);
    gl.Uniform1f(uni(progSky, "uNight"), night);
    gl.Uniform1f(uni(progSky, "uTime"), t);
    gl.Uniform1f(uni(progSky, "uDusk"), dusk);
    gl.Uniform2f(uni(progSky, "uSunXY"), sunX >= 0 ? (float)sunX / W : -1.0f, sunY >= 0 ? 1.0f - (float)sunY / H : -1.0f);
    gl.Uniform2f(uni(progSky, "uMoonXY"), moonX >= 0 ? (float)moonX / W : -1.0f, moonY >= 0 ? 1.0f - (float)moonY / H : -1.0f);
    gl.Uniform2f(uni(progSky, "uRes"), (float)W, (float)H);
    if (clouds && clouds != cloudsPtr) { cloudsPtr = clouds; texClouds = texFor(clouds); }
    gl.ActiveTexture(GL_TEXTURE0);
    gl.BindTexture(GL_TEXTURE_2D, clouds ? texClouds : texWhite);
    gl.Uniform1i(uni(progSky, "uClouds"), 0);
    gl.BindVertexArray(vaoSky);
    gl.DrawArrays(GL_TRIANGLES, 0, 3);
    gl.Enable(GL_DEPTH_TEST);
}

void GLRenderer::beginOpaque() {
    passOver = false;
    gl.BindFramebuffer(GL_FRAMEBUFFER, fboScene);
    gl.Viewport(0, 0, W, H);
    gl.Disable(GL_BLEND);
    gl.DepthMask(1);
    gl.Enable(GL_DEPTH_TEST);
}
void GLRenderer::endOpaque() {}
void GLRenderer::beginOver() {
    passOver = true;
    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl.DepthMask(0);
}
void GLRenderer::endOver() { gl.DepthMask(1); gl.Disable(GL_BLEND); passOver = false; }

void GLRenderer::uploadMesh(const Mesh& m) {
    size_t nv = m.pos.size();
    std::vector<float> data(nv * 14);
    for (size_t i = 0; i < nv; i++) {
        float* d = &data[i * 14];
        d[0] = m.pos[i].x; d[1] = m.pos[i].y; d[2] = m.pos[i].z;
        d[3] = m.nrm[i].x; d[4] = m.nrm[i].y; d[5] = m.nrm[i].z;
        d[6] = m.col[i].x; d[7] = m.col[i].y; d[8] = m.col[i].z;
        d[9] = i < m.uv.size() ? m.uv[i].x : 0;
        d[10] = i < m.uv.size() ? m.uv[i].y : 0;
        d[11] = i < m.splat.size() ? m.splat[i].x : 1;
        d[12] = i < m.splat.size() ? m.splat[i].y : 0;
        d[13] = i < m.splat.size() ? m.splat[i].z : 0;
    }
    gl.BindVertexArray(vaoScratch);
    gl.BindBuffer(GL_ARRAY_BUFFER, vboScratch);
    gl.BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(data.size() * 4), data.data(), GL_DYNAMIC_DRAW);
    gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, iboScratch);
    gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(m.tri.size() * 4), m.tri.data(), GL_DYNAMIC_DRAW);
    auto ptr = [](GLuint idx, GLint size, size_t off) {
        gl.EnableVertexAttribArray(idx);
        gl.VertexAttribPointer(idx, size, GL_FLOAT, 0, 14 * 4, (const void*)off);
    };
    ptr(0, 3, 0); ptr(1, 3, 12); ptr(2, 3, 24); ptr(3, 2, 36); ptr(4, 3, 44);
}

void GLRenderer::drawMesh(const Mesh& m, const Mat4& mvp, const Mat4& model, const Vec3& tint, float globalAlpha) {
    if (!ok) return;
    gl.BindFramebuffer(GL_FRAMEBUFFER, fboScene);
    gl.Viewport(0, 0, W, H);
    gl.UseProgram(progMesh);
    gl.UniformMatrix4fv(uni(progMesh, "uMVP"), 1, 0, mvp.m);
    gl.UniformMatrix4fv(uni(progMesh, "uModel"), 1, 0, model.m);
    gl.Uniform3f(uni(progMesh, "uTint"), tint.x, tint.y, tint.z);
    gl.Uniform1f(uni(progMesh, "uAlpha"), globalAlpha);
    gl.Uniform1i(uni(progMesh, "uHasSplat"), (m.tex2 && m.tex3) ? 1 : 0);
    gl.Uniform1i(uni(progMesh, "uWater"), m.water ? 1 : 0);
    gl.Uniform1i(uni(progMesh, "uEmissive"), m.emissive ? 1 : 0);
    gl.Uniform1i(uni(progMesh, "uAlphaTest"), m.alphaTest ? 1 : 0);
    gl.Uniform1i(uni(progMesh, "uShadow"), (!m.water && !m.emissive) ? 1 : 0);
    // light env
    gl.Uniform3f(uni(progMesh, "uSunDir"), L.sunDir.x, L.sunDir.y, L.sunDir.z);
    gl.Uniform3f(uni(progMesh, "uSunCol"), L.sunCol.x, L.sunCol.y, L.sunCol.z);
    gl.Uniform3f(uni(progMesh, "uFogCol"), L.fogCol.x, L.fogCol.y, L.fogCol.z);
    gl.Uniform3f(uni(progMesh, "uLanPos"), L.lanPos.x, L.lanPos.y, L.lanPos.z);
    gl.Uniform3f(uni(progMesh, "uLanCol"), L.lanCol.x, L.lanCol.y, L.lanCol.z);
    gl.Uniform3f(uni(progMesh, "uCamPos"), L.camPos.x, L.camPos.y, L.camPos.z);
    gl.Uniform1f(uni(progMesh, "uSunI"), L.sunI);
    gl.Uniform1f(uni(progMesh, "uAmb"), L.amb);
    gl.Uniform1f(uni(progMesh, "uFogDens"), L.fogDens);
    gl.Uniform1f(uni(progMesh, "uLanI"), L.lanI);
    gl.Uniform1f(uni(progMesh, "uSea"), L.sea);
    gl.Uniform1f(uni(progMesh, "uTime"), L.waveT);
    gl.Uniform1f(uni(progMesh, "uWorldSize"), worldSize);
    // textures
    gl.ActiveTexture(GL_TEXTURE0);
    gl.BindTexture(GL_TEXTURE_2D, texFor(m.tex));
    gl.Uniform1i(uni(progMesh, "uTex0"), 0);
    gl.ActiveTexture(GL_TEXTURE1);
    gl.BindTexture(GL_TEXTURE_2D, texFor(m.tex2));
    gl.Uniform1i(uni(progMesh, "uTex1"), 1);
    gl.ActiveTexture(GL_TEXTURE2);
    gl.BindTexture(GL_TEXTURE_2D, texFor(m.tex3));
    gl.Uniform1i(uni(progMesh, "uTex2"), 2);
    gl.ActiveTexture(GL_TEXTURE3);
    gl.BindTexture(GL_TEXTURE_2D, texHeight ? texHeight : texWhite);
    gl.Uniform1i(uni(progMesh, "uHeight"), 3);
    gl.ActiveTexture(GL_TEXTURE0);

    uploadMesh(m);
    if (passOver) { gl.Enable(GL_BLEND); gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); gl.DepthMask(0); }
    else { gl.Disable(GL_BLEND); gl.DepthMask(1); }
    gl.DrawElements(GL_TRIANGLES, (GLsizei)m.tri.size(), GL_UNSIGNED_INT, nullptr);
    if (passOver) { gl.DepthMask(1); gl.Disable(GL_BLEND); }
}

// ---------------- 2D (buffered, drawn in present) ----------------

void GLRenderer::rect(int x, int y, int w, int h, uint32_t c, float alpha) {
    cmds.push_back({0, (float)x, (float)y, (float)w, (float)h, 0, c, alpha, "", 0});
}
void GLRenderer::circle(int cx, int cy, int r, uint32_t c, float alpha) {
    cmds.push_back({1, (float)cx, (float)cy, (float)r, 0, 0, c, alpha, "", 0});
}
void GLRenderer::line(int x0, int y0, int x1, int y1, uint32_t c) {
    cmds.push_back({2, (float)x0, (float)y0, (float)x1, (float)y1, 0, c, 1.0f, "", 0});
}
void GLRenderer::text(int x, int y, const char* s, int scale, uint32_t c, float alpha) {
    cmds.push_back({3, (float)x, (float)y, 0, 0, 0, c, alpha, s, scale});
}

Mat4 GLRenderer::ortho() const {
    Mat4 r = Mat4::ident();
    r.m[0] = 2.0f / W; r.m[5] = -2.0f / H; r.m[10] = -1; r.m[12] = -1; r.m[13] = 1;
    return r;
}

void GLRenderer::draw2D() {
    if (cmds.empty() || !ok) return;
    gl.BindFramebuffer(GL_FRAMEBUFFER, fboScene);
    gl.Viewport(0, 0, W, H);
    gl.Disable(GL_DEPTH_TEST);
    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    Mat4 o = ortho();
    // rects
    gl.UseProgram(progRect);
    gl.UniformMatrix4fv(uni(progRect, "uOrtho"), 1, 0, o.m);
    std::vector<float> quads;
    for (auto& c : cmds) {
        if (c.kind == 0) {
            float x = c.x, y = c.y, w = c.w, h = c.h;
            float v[12] = {x, y, x + w, y, x + w, y + h, x, y, x + w, y + h, x, y + h};
            quads.insert(quads.end(), v, v + 12);
        } else if (c.kind == 1) {
            // circle as fan
            int seg = 16;
            float cx = c.x, cy = c.y, r = c.w;
            for (int i = 0; i < seg; i++) {
                float a0 = i * 6.2831853f / seg, a1 = (i + 1) * 6.2831853f / seg;
                float v[12] = {cx, cy, cx + std::cos(a0) * r, cy + std::sin(a0) * r, cx + std::cos(a1) * r, cy + std::sin(a1) * r,
                               cx, cy, cx + std::cos(a1) * r, cy + std::sin(a1) * r, cx + std::cos(a0) * r, cy + std::sin(a0) * r};
                // 2 tris = 6 verts = 12 floats, but we wrote 12 floats as 4 verts... fix below
                quads.insert(quads.end(), v, v + 12);
            }
        } else if (c.kind == 2) {
            float dx = c.w - c.x, dy = c.h - c.y;
            float l = std::sqrt(dx * dx + dy * dy);
            if (l > 0.5f) {
                float nx = -dy / l * 1.0f, ny = dx / l * 1.0f;
                float v[12] = {c.x + nx, c.y + ny, c.w + nx, c.h + ny, c.w - nx, c.h - ny,
                               c.x + nx, c.y + ny, c.w - nx, c.h - ny, c.x - nx, c.y - ny};
                quads.insert(quads.end(), v, v + 12);
            }
        }
    }
    // draw all rect/circle/line quads with per-quad color: batch by color is complex;
    // draw one call per command for simplicity (HUD has few dozen calls)
    gl.BindVertexArray(vaoScratch);
    gl.BindBuffer(GL_ARRAY_BUFFER, vboScratch);
    for (auto& c : cmds) {
        if (c.kind == 3) continue;
        std::vector<float> v;
        if (c.kind == 0) {
            float x = c.x, y = c.y, w = c.w, h = c.h;
            float vv[12] = {x, y, x + w, y, x + w, y + h, x, y, x + w, y + h, x, y + h};
            v.assign(vv, vv + 12);
        } else if (c.kind == 1) {
            int seg = 20;
            float cx = c.x, cy = c.y, r = c.w;
            for (int i = 0; i < seg; i++) {
                float a0 = i * 6.2831853f / seg, a1 = (i + 1) * 6.2831853f / seg;
                float tri[6] = {cx, cy, cx + std::cos(a0) * r, cy + std::sin(a0) * r, cx + std::cos(a1) * r, cy + std::sin(a1) * r};
                v.insert(v.end(), tri, tri + 6);
            }
        } else if (c.kind == 2) {
            float dx = c.w - c.x, dy = c.h - c.y;
            float l = std::sqrt(dx * dx + dy * dy);
            if (l > 0.5f) {
                float nx = -dy / l, ny = dx / l;
                float vv[12] = {c.x + nx, c.y + ny, c.w + nx, c.h + ny, c.w - nx, c.h - ny,
                                c.x + nx, c.y + ny, c.w - nx, c.h - ny, c.x - nx, c.y - ny};
                v.assign(vv, vv + 12);
            }
        }
        if (v.empty()) continue;
        gl.BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(v.size() * 4), v.data(), GL_DYNAMIC_DRAW);
        gl.EnableVertexAttribArray(0);
        gl.VertexAttribPointer(0, 2, GL_FLOAT, 0, 8, nullptr);
        gl.Uniform4f(uni(progRect, "uColor"),
                     ((c.c >> 16) & 255) / 255.f, ((c.c >> 8) & 255) / 255.f, (c.c & 255) / 255.f, c.alpha);
        gl.DrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 2));
    }
    // text
    if (fontTex) {
        gl.UseProgram(progText);
        gl.UniformMatrix4fv(uni(progText, "uOrtho"), 1, 0, o.m);
        gl.ActiveTexture(GL_TEXTURE0);
        gl.BindTexture(GL_TEXTURE_2D, fontTex);
        gl.Uniform1i(uni(progText, "uFont"), 0);
        for (auto& c : cmds) {
            if (c.kind != 3) continue;
            std::vector<float> v;
            float cx = c.x;
            for (char ch : c.s) {
                int gi = (uint8_t)ch;
                float u0 = gi * 8.0f / 1024.0f, u1 = (gi * 8.0f + 5.0f) / 1024.0f;
                float x0 = cx, y0 = c.y, x1 = cx + 5 * c.scale, y1 = c.y + 7 * c.scale;
                float vv[24] = {
                    x0, y0, u0, 0.0f, x1, y0, u1, 0.0f, x1, y1, u1, 1.0f,
                    x0, y0, u0, 0.0f, x1, y1, u1, 1.0f, x0, y1, u0, 1.0f};
                v.insert(v.end(), vv, vv + 24);
                cx += 6 * c.scale;
            }
            if (v.empty()) continue;
            gl.BufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(v.size() * 4), v.data(), GL_DYNAMIC_DRAW);
            gl.EnableVertexAttribArray(0);
            gl.VertexAttribPointer(0, 2, GL_FLOAT, 0, 16, nullptr);
            gl.EnableVertexAttribArray(1);
            gl.VertexAttribPointer(1, 2, GL_FLOAT, 0, 16, (const void*)8);
            gl.Uniform4f(uni(progText, "uColor"),
                         ((c.c >> 16) & 255) / 255.f, ((c.c >> 8) & 255) / 255.f, (c.c & 255) / 255.f, c.alpha);
            gl.DrawArrays(GL_TRIANGLES, 0, (GLsizei)(v.size() / 4));
        }
    }
    (void)quads;
    gl.Disable(GL_BLEND);
    gl.Enable(GL_DEPTH_TEST);
    cmds.clear();
}

void GLRenderer::present(uint32_t* /*dst*/, int dw, int dh, float vignette, uint32_t frameNo) {
    if (!ok) return;
    draw2D();
    // bright pass
    int bw = std::max(1, W / 4), bh = std::max(1, H / 4);
    gl.Disable(GL_DEPTH_TEST);
    gl.Disable(GL_BLEND);
    gl.BindFramebuffer(GL_FRAMEBUFFER, fboBloomA);
    gl.Viewport(0, 0, bw, bh);
    gl.UseProgram(progBright);
    gl.ActiveTexture(GL_TEXTURE0);
    gl.BindTexture(GL_TEXTURE_2D, texScene);
    gl.Uniform1i(uni(progBright, "uTex"), 0);
    gl.BindVertexArray(vaoSky);
    gl.DrawArrays(GL_TRIANGLES, 0, 3);
    // blur H -> B, V -> A
    gl.UseProgram(progBlur);
    gl.BindFramebuffer(GL_FRAMEBUFFER, fboBloomB);
    gl.BindTexture(GL_TEXTURE_2D, texBloomA);
    gl.Uniform1i(uni(progBlur, "uTex"), 0);
    gl.Uniform2f(uni(progBlur, "uDir"), 1.0f / bw, 0.0f);
    gl.DrawArrays(GL_TRIANGLES, 0, 3);
    gl.BindFramebuffer(GL_FRAMEBUFFER, fboBloomA);
    gl.BindTexture(GL_TEXTURE_2D, texBloomB);
    gl.Uniform2f(uni(progBlur, "uDir"), 0.0f, 1.0f / bh);
    gl.DrawArrays(GL_TRIANGLES, 0, 3);
    // composite to screen
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    gl.Viewport(0, 0, dw, dh);
    gl.UseProgram(progComposite);
    gl.ActiveTexture(GL_TEXTURE0);
    gl.BindTexture(GL_TEXTURE_2D, texScene);
    gl.Uniform1i(uni(progComposite, "uScene"), 0);
    gl.ActiveTexture(GL_TEXTURE1);
    gl.BindTexture(GL_TEXTURE_2D, texBloomA);
    gl.Uniform1i(uni(progComposite, "uBloom"), 1);
    gl.ActiveTexture(GL_TEXTURE0);
    gl.Uniform1f(uni(progComposite, "uVig"), vignette);
    gl.Uniform1i(uni(progComposite, "uFrame"), (int)frameNo);
    gl.DrawArrays(GL_TRIANGLES, 0, 3);
    glPlatformSwap();
}
