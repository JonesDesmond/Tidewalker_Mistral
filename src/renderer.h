#pragma once
#include "math.h"
#include <cstdint>
#include <vector>

// Texture: 0xAARRGGBB pixels.
struct Tex {
    int w = 0, h = 0;
    std::vector<uint32_t> px;
    Tex() = default;
    Tex(int w_, int h_) : w(w_), h(h_), px((size_t)w_ * h_) {}
};

// Triangle mesh. Vertex color is an albedo tint; uv for texturing; splat for
// 3-way texture blending; flags for alpha-test, water shader and emissive.
struct Mesh {
    std::vector<Vec3> pos, nrm, col;
    std::vector<Vec2> uv;
    std::vector<Vec3> splat;
    std::vector<int> tri;
    std::vector<float> alpha;
    const Tex* tex = nullptr;
    const Tex* tex2 = nullptr;
    const Tex* tex3 = nullptr;
    bool alphaTest = false, water = false, emissive = false;

    int addVert(const Vec3& p, const Vec3& c, const Vec3& n, const Vec2& t) {
        pos.push_back(p); col.push_back(c); nrm.push_back(n); uv.push_back(t);
        splat.push_back({1, 0, 0});
        return (int)pos.size() - 1;
    }
    void addTri(int a, int b, int c, float alpha = 1.0f) {
        tri.push_back(a); tri.push_back(b); tri.push_back(c);
        this->alpha.push_back(alpha);
    }
};

struct LightEnv {
    Vec3 sunDir{0, 1, 0}, sunCol{1, 1, 1}, fogCol{0.5f, 0.5f, 0.5f};
    Vec3 lanPos{0, 0, 0}, lanCol{1, 0.72f, 0.42f}, camPos{0, 0, 0};
    float sunI = 1, amb = 0.12f, fogDens = 0.003f, lanI = 0;
    float sea = 0, waveT = 0;
};

class Renderer {
public:
    int W, H;
    std::vector<uint32_t> pix;   // 0x00RRGGBB
    std::vector<uint16_t> zbuf;
    LightEnv L;

    Renderer(int w, int h);

    void resetDepth();
    void skyPass(uint32_t topC, uint32_t horC, const Tex* clouds, float cloudAmt,
                 int sunX, int sunY, float sunGlow, uint32_t sunC,
                 int moonX, int moonY, uint32_t moonC, float night, float t, float dusk);

    // Opaque pass: queue with beginOpaque(), render in parallel with endOpaque().
    void beginOpaque();
    void endOpaque();
    // Translucent pass over the current frame: queue with beginOver(), render
    // with endOver(). Workers seed from the current pix/zbuf.
    void beginOver();
    void endOver();
    // Draw mesh. model = model matrix; mvp = proj*view*model; tint multiplies albedo.
    void drawMesh(const Mesh& m, const Mat4& mvp, const Mat4& model, const Vec3& tint, float globalAlpha);

    // 2D drawing (internal buffer pixels).
    void rect(int x, int y, int w, int h, uint32_t c, float alpha = 1.0f);
    void circle(int cx, int cy, int r, uint32_t c, float alpha = 1.0f);
    void line(int x0, int y0, int x1, int y1, uint32_t c);
    void text(int x, int y, const char* s, int scale, uint32_t c, float alpha = 1.0f);
    int textW(const char* s, int scale) const;

    // Downsample to dst with ACES tonemap + bloom + vignette + grain.
    void present(uint32_t* dst, int dw, int dh, float vignette, uint32_t frameNo);

    static uint32_t sampleBil(const Tex& t, float u, float v);
    static uint32_t sampleNear(const Tex& t, float u, float v);

    // ABI-stable helpers shared with free functions
    struct CV {
        float x, y, z, w;
        float r, g, b, a;
        float u, v;
        float nx, ny, nz;
        float ff;              // per-vertex fog factor
        float wx, wy, wz;
        float s0, s1, s2;
    };
    struct SV {
        float sx, sy, sz, iw;
        float r, g, b, a, u, v, nx, ny, nz, ff, wx, wy, wz, s0, s1, s2;
    };

    struct Call { const Mesh* m; Mat4 mvp, model; Vec3 tint; float alpha; };
private:
    struct TriJob { SV a, b, c; const Mesh* m; float alpha; };
    std::vector<Call> calls;
    std::vector<TriJob> jobs;
    bool collecting = false;
    bool seedFromMain = false;
    int nThreads = 1;
    std::vector<std::vector<uint32_t>> wpix;
    std::vector<std::vector<uint16_t>> wz;

    void rasterJob(const TriJob& j, uint32_t* pix, uint16_t* zbuf, int y0, int y1);
    static float edge(const SV& a, const SV& b, float px, float py) {
        return (b.sx - a.sx) * (py - a.sy) - (b.sy - a.sy) * (px - a.sx);
    }
    std::vector<uint32_t> winBuf, bloomA, bloomB;
};

inline uint32_t rgb(float r, float g, float b) {
    auto cl = [](float v) -> int { return v < 0 ? 0 : (v > 1 ? 255 : (int)(v * 255.0f + 0.5f)); };
    return ((uint32_t)cl(r) << 16) | ((uint32_t)cl(g) << 8) | (uint32_t)cl(b);
}
