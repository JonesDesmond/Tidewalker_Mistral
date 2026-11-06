#include "renderer.h"
#include "font.h"
#include <algorithm>
#include <cstring>

Renderer::Renderer(int w, int h) : W(w), H(h), pix((size_t)w * h), zbuf((size_t)w * h) {}

void Renderer::clear(uint32_t c) {
    std::fill(pix.begin(), pix.end(), c);
    std::fill(zbuf.begin(), zbuf.end(), 0xFFFF);
}
void Renderer::resetDepth() { std::fill(zbuf.begin(), zbuf.end(), 0xFFFF); }

void Renderer::vgradient(uint32_t top, uint32_t bot) {
    float tr = ((top >> 16) & 255) / 255.f, tg = ((top >> 8) & 255) / 255.f, tb = (top & 255) / 255.f;
    float br = ((bot >> 16) & 255) / 255.f, bg = ((bot >> 8) & 255) / 255.f, bb = (bot & 255) / 255.f;
    for (int y = 0; y < H; y++) {
        float t = H > 1 ? (float)y / (H - 1) : 0;
        uint32_t c = rgb(lerpf(tr, br, t), lerpf(tg, bg, t), lerpf(tb, bb, t));
        uint32_t* row = &pix[(size_t)y * W];
        for (int x = 0; x < W; x++) row[x] = c;
    }
}

static const float NEARW = 0.08f;

static bool clipNear(std::vector<Renderer::CV>& poly) {
    std::vector<Renderer::CV> out;
    out.reserve(poly.size() + 2);
    size_t n = poly.size();
    for (size_t i = 0; i < n; i++) {
        Renderer::CV& a = poly[i];
        Renderer::CV& b = poly[(i + 1) % n];
        bool ain = a.w > NEARW, bin = b.w > NEARW;
        if (ain) out.push_back(a);
        if (ain != bin) {
            float t = (NEARW - a.w) / (b.w - a.w);
            Renderer::CV c;
            c.x = a.x + (b.x - a.x) * t; c.y = a.y + (b.y - a.y) * t;
            c.z = a.z + (b.z - a.z) * t; c.w = NEARW;
            c.r = a.r + (b.r - a.r) * t; c.g = a.g + (b.g - a.g) * t;
            c.b = a.b + (b.b - a.b) * t; c.a = a.a + (b.a - a.a) * t;
            out.push_back(c);
        }
    }
    poly.swap(out);
    return poly.size() >= 3;
}

void Renderer::drawMesh(const Mesh& m, const Mat4& mvp, const Vec3* lit, float globalAlpha, bool translucent) {
    const size_t nv = m.pos.size();
    std::vector<CV> cv(nv);
    for (size_t i = 0; i < nv; i++) {
        Vec4 v = xform4(mvp, m.pos[i]);
        cv[i].x = v.x; cv[i].y = v.y; cv[i].z = v.z; cv[i].w = v.w;
        cv[i].r = lit[i].x; cv[i].g = lit[i].y; cv[i].b = lit[i].z;
        cv[i].a = 1.0f;
    }
    size_t nt = m.tri.size() / 3;
    std::vector<CV> poly;
    poly.reserve(4);
    for (size_t t = 0; t < nt; t++) {
        float alpha = m.alpha[t] * globalAlpha;
        if (alpha <= 0.003f) continue;
        poly.clear();
        for (int k = 0; k < 3; k++) {
            CV& c = cv[(size_t)m.tri[t * 3 + k]];
            c.a = alpha;
            poly.push_back(c);
        }
        if (!clipNear(poly)) continue;
        SV sv[4];
        size_t sn = poly.size() > 4 ? 4 : poly.size();
        for (size_t i = 0; i < sn; i++) {
            CV& c = poly[i];
            float iw = 1.0f / c.w;
            sv[i].iw = iw;
            sv[i].sx = (c.x * iw * 0.5f + 0.5f) * W;
            sv[i].sy = (1.0f - (c.y * iw * 0.5f + 0.5f)) * H;
            sv[i].sz = c.z * iw * 0.5f + 0.5f;
            sv[i].r = c.r * iw; sv[i].g = c.g * iw; sv[i].b = c.b * iw; sv[i].a = c.a * iw;
        }
        bool blend = translucent || alpha < 0.999f;
        for (size_t i = 1; i + 1 < sn; i++) raster(sv[0], sv[i], sv[i + 1], blend);
    }
}

void Renderer::raster(const SV& a, const SV& b, const SV& c, bool blend) {
    float area = edge(a, b, c.sx, c.sy);
    if (area == 0) return;
    float minx = std::min({a.sx, b.sx, c.sx}), maxx = std::max({a.sx, b.sx, c.sx});
    float miny = std::min({a.sy, b.sy, c.sy}), maxy = std::max({a.sy, b.sy, c.sy});
    int x0 = std::max(0, (int)std::floor(minx)), x1 = std::min(W - 1, (int)std::ceil(maxx));
    int y0 = std::max(0, (int)std::floor(miny)), y1 = std::min(H - 1, (int)std::ceil(maxy));
    if (x0 > x1 || y0 > y1) return;
    float invArea = 1.0f / area;
    bool pos = area > 0;
    for (int y = y0; y <= y1; y++) {
        float py = y + 0.5f;
        uint32_t* prow = &pix[(size_t)y * W];
        uint16_t* zrow = &zbuf[(size_t)y * W];
        for (int x = x0; x <= x1; x++) {
            float px = x + 0.5f;
            float w0 = edge(b, c, px, py), w1 = edge(c, a, px, py), w2 = edge(a, b, px, py);
            if (pos) { if (w0 < 0 || w1 < 0 || w2 < 0) continue; }
            else { if (w0 > 0 || w1 > 0 || w2 > 0) continue; w0 = -w0; w1 = -w1; w2 = -w2; }
            float l0 = w0 * invArea, l1 = w1 * invArea, l2 = w2 * invArea;
            float iw = l0 * a.iw + l1 * b.iw + l2 * c.iw;
            if (iw <= 1e-9f) continue;
            float z = (l0 * a.sz * a.iw + l1 * b.sz * b.iw + l2 * c.sz * c.iw) / iw;
            if (z < 0) z = 0; else if (z > 1) z = 1;
            uint16_t z16 = (uint16_t)(z * 65535.0f);
            size_t idx = (size_t)y * W + x;
            if (z16 >= zrow[x]) continue;
            float r = (l0 * a.r + l1 * b.r + l2 * c.r) / iw;
            float g = (l0 * a.g + l1 * b.g + l2 * c.g) / iw;
            float bl = (l0 * a.b + l1 * b.b + l2 * c.b) / iw;
            float al = (l0 * a.a + l1 * b.a + l2 * c.a) / iw;
            if (blend && al < 0.999f) {
                if (al <= 0.004f) continue;
                uint32_t d = prow[x];
                float dr = ((d >> 16) & 255) / 255.f, dg = ((d >> 8) & 255) / 255.f, db = (d & 255) / 255.f;
                prow[x] = rgb(lerpf(dr, r, al), lerpf(dg, g, al), lerpf(db, bl, al));
            } else {
                zrow[x] = z16;
                prow[x] = rgb(r, g, bl);
            }
        }
    }
}

void Renderer::rect(int x, int y, int w, int h, uint32_t c, float alpha) {
    if (w <= 0 || h <= 0) return;
    int x0 = std::max(0, x), y0 = std::max(0, y);
    int x1 = std::min(W - 1, x + w - 1), y1 = std::min(H - 1, y + h - 1);
    float cr = ((c >> 16) & 255) / 255.f, cg = ((c >> 8) & 255) / 255.f, cb = (c & 255) / 255.f;
    for (int yy = y0; yy <= y1; yy++) {
        uint32_t* row = &pix[(size_t)yy * W];
        for (int xx = x0; xx <= x1; xx++) {
            if (alpha >= 0.999f) row[xx] = c;
            else {
                uint32_t d = row[xx];
                float dr = ((d >> 16) & 255) / 255.f, dg = ((d >> 8) & 255) / 255.f, db = (d & 255) / 255.f;
                row[xx] = rgb(lerpf(dr, cr, alpha), lerpf(dg, cg, alpha), lerpf(db, cb, alpha));
            }
        }
    }
}

void Renderer::circle(int cx, int cy, int r, uint32_t c, float alpha) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) {
                int px = cx + x, py = cy + y;
                if (px >= 0 && px < W && py >= 0 && py < H) {
                    if (alpha >= 0.999f) pix[(size_t)py * W + px] = c;
                    else {
                        uint32_t d = pix[(size_t)py * W + px];
                        pix[(size_t)py * W + px] = rgb(
                            lerpf(((d >> 16) & 255) / 255.f, ((c >> 16) & 255) / 255.f, alpha),
                            lerpf(((d >> 8) & 255) / 255.f, ((c >> 8) & 255) / 255.f, alpha),
                            lerpf((d & 255) / 255.f, (c & 255) / 255.f, alpha));
                    }
                }
            }
}

void Renderer::line(int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (x0 >= 0 && x0 < W && y0 >= 0 && y0 < H) pix[(size_t)y0 * W + x0] = c;
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

int Renderer::textW(const char* s, int scale) const {
    int w = 0;
    for (const char* p = s; *p; p++) w += 6 * scale;
    return w > 0 ? w - scale : 0;
}

void Renderer::text(int x, int y, const char* s, int scale, uint32_t c, float alpha) {
    if (!g_font.loaded) return;
    int cx = x;
    for (const char* p = s; *p; p++) {
        const uint8_t* g = g_font.glyph(*p);
        for (int col = 0; col < 5; col++) {
            uint8_t bits = g[col];
            if (!bits) continue;
            for (int row = 0; row < 7; row++)
                if (bits & (1u << row))
                    rect(cx + col * scale, y + row * scale, scale, scale, c, alpha);
        }
        cx += 6 * scale;
    }
}

void Renderer::present(uint32_t* dst, int dw, int dh, float vignette, uint32_t frameNo) {
    float sx = (float)W / dw, sy = (float)H / dh;
    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            int x0 = (int)(x * sx), x1 = std::max(x0 + 1, (int)((x + 1) * sx));
            int y0 = (int)(y * sy), y1 = std::max(y0 + 1, (int)((y + 1) * sy));
            float r = 0, g = 0, b = 0;
            int n = 0;
            for (int yy = y0; yy < y1 && yy < H; yy++)
                for (int xx = x0; xx < x1 && xx < W; xx++) {
                    uint32_t p = pix[(size_t)yy * W + xx];
                    r += ((p >> 16) & 255); g += ((p >> 8) & 255); b += (p & 255);
                    n++;
                }
            r /= n * 255.f; g /= n * 255.f; b /= n * 255.f;
            float nx = (x + 0.5f) / dw * 2 - 1, ny = (y + 0.5f) / dh * 2 - 1;
            float vig = 1.0f - vignette * (nx * nx + ny * ny) * 0.5f;
            float grain = (((hash2i(x + (int)frameNo * 131, y + (int)frameNo * 57) & 255) / 255.f) - 0.5f) * 0.045f;
            dst[(size_t)y * dw + x] = rgb(clampf(r * vig + grain, 0, 1),
                                          clampf(g * vig + grain, 0, 1),
                                          clampf(b * vig + grain, 0, 1));
        }
    }
}
