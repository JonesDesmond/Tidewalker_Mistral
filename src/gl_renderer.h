#pragma once
#include "renderer.h"
#include "gl_api.h"
#include <map>
#include <string>
#include <vector>

// GPU backend (OpenGL 3.3 core, loaded at runtime). Same IRenderer API as the
// software rasterizer, so the game code is unchanged. Requires a GL context:
// call glPlatformInit() before constructing.
class GLRenderer : public IRenderer {
public:
    GLRenderer(int w, int h);
    ~GLRenderer();
    bool ok = false;

    bool isGL() override { return true; }
    void setHeightmap(const float* hm, int n, float worldSize);

    void resetDepth() override;
    void skyPass(uint32_t topC, uint32_t horC, const Tex* clouds, float cloudAmt,
                 int sunX, int sunY, float sunGlow, uint32_t sunC,
                 int moonX, int moonY, uint32_t moonC, float night, float t, float dusk) override;
    void beginOpaque() override;
    void endOpaque() override;
    void beginOver() override;
    void endOver() override;
    void drawMesh(const Mesh& m, const Mat4& mvp, const Mat4& model, const Vec3& tint, float globalAlpha) override;
    void rect(int x, int y, int w, int h, uint32_t c, float alpha = 1.0f) override;
    void circle(int cx, int cy, int r, uint32_t c, float alpha = 1.0f) override;
    void line(int x0, int y0, int x1, int y1, uint32_t c) override;
    void text(int x, int y, const char* s, int scale, uint32_t c, float alpha = 1.0f) override;
    void present(uint32_t* dst, int dw, int dh, float vignette, uint32_t frameNo) override;

private:
    unsigned progMesh = 0, progSky = 0, progBright = 0, progBlur = 0, progComposite = 0, progRect = 0, progText = 0;
    unsigned vaoScratch = 0, vboScratch = 0, iboScratch = 0, vaoSky = 0;
    unsigned fboScene = 0, texScene = 0;
    unsigned fboBloomA = 0, texBloomA = 0, fboBloomB = 0, texBloomB = 0;
    unsigned texWhite = 0, texHeight = 0;
    int heightN = 0;
    float worldSize = 1600;
    std::map<const Tex*, unsigned> texCache;
    unsigned texClouds = 0;
    const Tex* cloudsPtr = nullptr;
    unsigned fontTex = 0;
    bool passOver = false;
    // buffered 2D
    struct Cmd2D {
        int kind;  // 0 rect, 1 circle, 2 line, 3 text
        float x, y, w, h, r;
        uint32_t c;
        float alpha;
        std::string s;
        int scale;
    };
    std::vector<Cmd2D> cmds;

    unsigned compile(unsigned type, const char* src);
    unsigned link(unsigned vs, unsigned fs);
    GLint uni(unsigned prog, const char* name);
    unsigned texFor(const Tex* t);
    void uploadMesh(const Mesh& m);
    void draw2D();
    Mat4 ortho() const;
};
