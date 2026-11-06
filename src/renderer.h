#pragma once
#include "math.h"
#include <cstdint>
#include <vector>

// Triangle mesh with per-vertex base color and normal, per-triangle alpha.
struct Mesh {
    std::vector<Vec3> pos;
    std::vector<Vec3> col;
    std::vector<Vec3> nrm;
    std::vector<int> tri;      // 3 indices per triangle
    std::vector<float> alpha;  // per triangle, 0..1
    int addVert(const Vec3& p, const Vec3& c, const Vec3& n) {
        pos.push_back(p); col.push_back(c); nrm.push_back(n);
        return (int)pos.size() - 1;
    }
    void addTri(int a, int b, int c, float alpha = 1.0f) {
        tri.push_back(a); tri.push_back(b); tri.push_back(c);
        this->alpha.push_back(alpha);
    }
};

// Software renderer: perspective-correct gouraud rasterizer with a 16-bit
// depth buffer, alpha blending, 2D drawing for HUD, and SSAA downsample.
class Renderer {
public:
    int W, H;
    std::vector<uint32_t> pix;   // 0x00RRGGBB
    std::vector<uint16_t> zbuf;

    Renderer(int w, int h);

    void clear(uint32_t c);
    void resetDepth();
    void vgradient(uint32_t top, uint32_t bot);

    // Draw mesh with model-view-projection matrix. lit = per-vertex lit rgb (0..1),
    // must have same size as mesh.pos. globalAlpha multiplies per-tri alpha.
    void drawMesh(const Mesh& m, const Mat4& mvp, const Vec3* lit, float globalAlpha, bool translucent);

    // 2D drawing (screen space, pixels of the internal buffer).
    void rect(int x, int y, int w, int h, uint32_t c, float alpha = 1.0f);
    void circle(int cx, int cy, int r, uint32_t c, float alpha = 1.0f);
    void line(int x0, int y0, int x1, int y1, uint32_t c);
    void text(int x, int y, const char* s, int scale, uint32_t c, float alpha = 1.0f);
    int textW(const char* s, int scale) const;

    // Box-filter downsample from internal res to dst (dw x dh), applying
    // vignette and film grain. dst must hold dw*dh pixels.
    void present(uint32_t* dst, int dw, int dh, float vignette, uint32_t frameNo);

    struct CV { float x, y, z, w, r, g, b, a; };
    struct SV { float sx, sy, sz, iw, r, g, b, a; };
private:
    void raster(const SV& a, const SV& b, const SV& c, bool blend);
    static float edge(const SV& a, const SV& b, float px, float py) {
        return (b.sx - a.sx) * (py - a.sy) - (b.sy - a.sy) * (px - a.sx);
    }
};

inline uint32_t rgb(float r, float g, float b) {
    auto cl = [](float v) -> int { return v < 0 ? 0 : (v > 1 ? 255 : (int)(v * 255.0f + 0.5f)); };
    return ((uint32_t)cl(r) << 16) | ((uint32_t)cl(g) << 8) | (uint32_t)cl(b);
}
