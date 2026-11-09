#include "renderer.h"
#include "font.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <thread>

Renderer::Renderer(int w, int h) : W(w), H(h), pix((size_t)w * h), zbuf((size_t)w * h) {
    unsigned hc = std::thread::hardware_concurrency();
    nThreads = std::max(1, (int)std::min(hc, 5u));
    wpix.resize(nThreads);
    wz.resize(nThreads);
    for (int i = 0; i < nThreads; i++) {
        wpix[i].assign((size_t)w * h, 0);
        wz[i].assign((size_t)w * h, 0xFFFF);
    }
}

void Renderer::resetDepth() { std::fill(zbuf.begin(), zbuf.end(), 0xFFFF); }

uint32_t Renderer::sampleBil(const Tex& t, float u, float v) {
    u -= std::floor(u); v -= std::floor(v);
    float x = u * t.w - 0.5f, y = v * t.h - 0.5f;
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    float fx = x - x0, fy = y - y0;
    int x1 = x0 + 1, y1 = y0 + 1;
    x0 = (x0 % t.w + t.w) % t.w; x1 = (x1 % t.w + t.w) % t.w;
    y0 = (y0 % t.h + t.h) % t.h; y1 = (y1 % t.h + t.h) % t.h;
    auto px = [&](int xx, int yy) { return t.px[(size_t)yy * t.w + xx]; };
    uint32_t c00 = px(x0, y0), c10 = px(x1, y0), c01 = px(x0, y1), c11 = px(x1, y1);
    auto ch = [&](int s) {
        float v0 = ((c00 >> s) & 255) + (((c10 >> s) & 255) - ((c00 >> s) & 255)) * fx;
        float v1 = ((c01 >> s) & 255) + (((c11 >> s) & 255) - ((c01 >> s) & 255)) * fx;
        return (uint32_t)(v0 + (v1 - v0) * fy + 0.5f);
    };
    return (ch(24) << 24) | (ch(16) << 16) | (ch(8) << 8) | ch(0);
}

uint32_t Renderer::sampleNear(const Tex& t, float u, float v) {
    u -= std::floor(u); v -= std::floor(v);
    int x = (int)(u * t.w) % t.w, y = (int)(v * t.h) % t.h;
    return t.px[(size_t)y * t.w + x];
}

// ---------------------------------------------------------------- sky

void Renderer::skyPass(uint32_t topC, uint32_t horC, const Tex* clouds, float cloudAmt,
                       int sunX, int sunY, float sunGlow, uint32_t sunC,
                       int moonX, int moonY, uint32_t moonC, float night, float t, float dusk) {
    float tr = ((topC >> 16) & 255) / 255.f, tg = ((topC >> 8) & 255) / 255.f, tb = (topC & 255) / 255.f;
    float hr = ((horC >> 16) & 255) / 255.f, hg = ((horC >> 8) & 255) / 255.f, hb = (horC & 255) / 255.f;
    float sr = ((sunC >> 16) & 255) / 255.f, sg = ((sunC >> 8) & 255) / 255.f, sb = (sunC & 255) / 255.f;
    auto rowFn = [&](int y) {
        float ty = H > 1 ? (float)y / (H - 1) : 0;
        float br = lerpf(hr, tr, ty), bg = lerpf(hg, tg, ty), bb = lerpf(hb, tb, ty);
        uint32_t* row = &pix[(size_t)y * W];
        for (int x = 0; x < W; x++) {
            float r = br, g = bg, b = bb;
            if (sunGlow > 0.001f) {
                float dx = x - sunX, dy = y - sunY;
                float d = std::sqrt(dx * dx + dy * dy) / (H * 0.55f);
                float gl = std::pow(std::max(0.0f, 1.0f - d), 3.0f) * sunGlow;
                r += sr * gl; g += sg * gl; b += sb * gl;
            }
            if (night > 0.01f) {
                float h = hash01(x * 3 + 11, y * 7 + 5);
                if (h > 0.9982f) {
                    float s = (h - 0.9982f) * 5000.0f * night;
                    r += s; g += s; b += s * 1.1f;
                }
                float dx = x - moonX, dy = y - moonY;
                float d = std::sqrt(dx * dx + dy * dy);
                int mr = (int)(H * 0.018f);
                if (d < mr) { r = 0.72f; g = 0.78f; b = 0.88f; }
                else if (d < mr * 3) {
                    float gl = std::pow(std::max(0.0f, 1.0f - d / (mr * 3.0f)), 3.0f) * 0.35f * night;
                    r += gl * 0.7f; g += gl * 0.75f; b += gl * 0.85f;
                }
            }
            if (clouds && cloudAmt > 0.01f) {
                float cu = (float)x / W * 3.0f + t * 0.006f;
                float cv = (float)y / H * 1.6f + 0.3f;
                uint32_t cl = sampleNear(*clouds, cu, cv);
                float lum = (((cl >> 16) & 255) * 0.5f + ((cl >> 8) & 255) * 0.35f + (cl & 255) * 0.15f) / 255.0f;
                float cr = lerpf(0.10f, 1.0f, 1.0f - night) + dusk * 0.5f;
                float cg = lerpf(0.12f, 0.98f, 1.0f - night) + dusk * 0.15f;
                float cb = lerpf(0.20f, 0.95f, 1.0f - night);
                float amt = clampf(lum * 1.3f, 0, 1) * cloudAmt * (1.0f - ty * 0.55f);
                r = lerpf(r, cr, amt); g = lerpf(g, cg, amt); b = lerpf(b, cb, amt);
            }
            row[x] = rgb(clampf(r, 0, 1), clampf(g, 0, 1), clampf(b, 0, 1));
        }
    };
    std::vector<std::thread> th;
    int band = (H + nThreads - 1) / nThreads;
    for (int t = 0; t < nThreads; t++) {
        int y0 = t * band, y1 = std::min(H, y0 + band);
        if (y0 >= y1) continue;
        th.emplace_back([&rowFn, y0, y1] { for (int y = y0; y < y1; y++) rowFn(y); });
    }
    for (auto& x : th) x.join();
}

// ---------------------------------------------------------------- 3D

static const float NEARW = 0.08f;

static int clipNearArr(Renderer::CV* poly, int n) {
    Renderer::CV out[8];
    int m = 0;
    for (int i = 0; i < n; i++) {
        Renderer::CV& a = poly[i];
        Renderer::CV& b = poly[(i + 1) % n];
        bool ain = a.w > NEARW, bin = b.w > NEARW;
        if (ain) out[m++] = a;
        if (ain != bin) {
            float t = (NEARW - a.w) / (b.w - a.w);
            Renderer::CV c;
            c.x = a.x + (b.x - a.x) * t; c.y = a.y + (b.y - a.y) * t;
            c.z = a.z + (b.z - a.z) * t; c.w = NEARW;
            c.r = a.r + (b.r - a.r) * t; c.g = a.g + (b.g - a.g) * t;
            c.b = a.b + (b.b - a.b) * t; c.a = a.a + (b.a - a.a) * t;
            c.u = a.u + (b.u - a.u) * t; c.v = a.v + (b.v - a.v) * t;
            c.nx = a.nx + (b.nx - a.nx) * t; c.ny = a.ny + (b.ny - a.ny) * t; c.nz = a.nz + (b.nz - a.nz) * t;
            c.ff = a.ff + (b.ff - a.ff) * t;
            c.wx = a.wx + (b.wx - a.wx) * t; c.wy = a.wy + (b.wy - a.wy) * t; c.wz = a.wz + (b.wz - a.wz) * t;
            c.s0 = a.s0 + (b.s0 - a.s0) * t; c.s1 = a.s1 + (b.s1 - a.s1) * t; c.s2 = a.s2 + (b.s2 - a.s2) * t;
            out[m++] = c;
        }
    }
    std::memcpy(poly, out, (size_t)m * sizeof(Renderer::CV));
    return m;
}

static void fillVertex(Renderer::CV& v, const Mesh& m, size_t i, const Renderer::Call& c, const LightEnv& L, int W, int H) {
    (void)W; (void)H;
    Vec4 cl = xform4(c.mvp, m.pos[i]);
    Vec4 wp = xform4(c.model, m.pos[i]);
    float wx = wp.x / wp.w, wy = wp.y / wp.w, wz = wp.z / wp.w;
    v.x = cl.x; v.y = cl.y; v.z = cl.z; v.w = cl.w;
    float nl = m.nrm[i].len();
    Vec3 n = nl > 1e-6f ? m.nrm[i] / nl : Vec3(0, 1, 0);
    Vec3 ld = L.lanPos - Vec3(wx, wy, wz);
    float d2 = ld.dot(ld);
    float att = L.lanI / (1.0f + d2 * 0.05f);
    float ndl = std::sqrt(d2) > 1e-6f ? std::max(0.0f, n.dot(ld / std::sqrt(d2))) : 0;
    float lp = att * ndl;
    v.r = m.col[i].x * c.tint.x + L.lanCol.x * lp * 0.9f;
    v.g = m.col[i].y * c.tint.y + L.lanCol.y * lp * 0.9f;
    v.b = m.col[i].z * c.tint.z + L.lanCol.z * lp * 0.9f;
    v.a = 1.0f;
    v.u = i < m.uv.size() ? m.uv[i].x : 0;
    v.v = i < m.uv.size() ? m.uv[i].y : 0;
    v.nx = n.x; v.ny = n.y; v.nz = n.z;
    float dist = std::sqrt((wx - L.camPos.x) * (wx - L.camPos.x) + (wy - L.camPos.y) * (wy - L.camPos.y) + (wz - L.camPos.z) * (wz - L.camPos.z));
    float fd = dist * L.fogDens;
    v.ff = 1.0f - std::exp(-fd * fd);
    v.wx = wx; v.wy = wy; v.wz = wz;
    v.s0 = i < m.splat.size() ? m.splat[i].x : 1;
    v.s1 = i < m.splat.size() ? m.splat[i].y : 0;
    v.s2 = i < m.splat.size() ? m.splat[i].z : 0;
}

static void projectCV(Renderer::SV& s, const Renderer::CV& cc, int W, int H) {
    float iw = 1.0f / cc.w;
    s.iw = iw;
    s.sx = (cc.x * iw * 0.5f + 0.5f) * W;
    s.sy = (1.0f - (cc.y * iw * 0.5f + 0.5f)) * H;
    s.sz = cc.z * iw * 0.5f + 0.5f;
    s.r = cc.r * iw; s.g = cc.g * iw; s.b = cc.b * iw; s.a = cc.a * iw;
    s.u = cc.u * iw; s.v = cc.v * iw;
    s.nx = cc.nx * iw; s.ny = cc.ny * iw; s.nz = cc.nz * iw;
    s.ff = cc.ff * iw;
    s.wx = cc.wx * iw; s.wy = cc.wy * iw; s.wz = cc.wz * iw;
    s.s0 = cc.s0 * iw; s.s1 = cc.s1 * iw; s.s2 = cc.s2 * iw;
}

void Renderer::beginOpaque() { collecting = true; seedFromMain = false; calls.clear(); }
void Renderer::beginOver() { collecting = true; seedFromMain = true; calls.clear(); }
void Renderer::endOver() { endOpaque(); }

void Renderer::endOpaque() {
    collecting = false;
    // 1) transform + clip queued triangles into a flat job list, in parallel
    jobs.clear();
    {
        std::vector<std::vector<TriJob>> parts(nThreads);
        std::vector<std::thread> th;
        size_t ncalls = calls.size();
        for (int t = 0; t < nThreads; t++) {
            th.emplace_back([this, &parts, t, ncalls] {
                std::vector<CV> cv;
                CV poly[8];
                size_t begin = ncalls * t / nThreads, end = ncalls * (t + 1) / nThreads;
                for (size_t ci = begin; ci < end; ci++) {
                    const Call& c = calls[ci];
                    const Mesh& m = *c.m;
                    size_t nv = m.pos.size();
                    cv.resize(nv);
                    for (size_t i = 0; i < nv; i++) fillVertex(cv[i], m, i, c, L, W, H);
                    size_t nt = m.tri.size() / 3;
                    for (size_t t2 = 0; t2 < nt; t2++) {
                        float alpha = m.alpha[t2] * c.alpha;
                        if (alpha <= 0.003f) continue;
                        poly[0] = cv[(size_t)m.tri[t2 * 3]]; poly[0].a = alpha;
                        poly[1] = cv[(size_t)m.tri[t2 * 3 + 1]]; poly[1].a = alpha;
                        poly[2] = cv[(size_t)m.tri[t2 * 3 + 2]]; poly[2].a = alpha;
                        int pn = clipNearArr(poly, 3);
                        if (pn < 3) continue;
                        SV sv[4];
                        for (int i = 0; i < pn && i < 4; i++) projectCV(sv[i], poly[i], W, H);
                        for (int i = 1; i + 1 < pn && i + 1 < 4; i++)
                            parts[t].push_back({sv[0], sv[i], sv[i + 1], &m, c.alpha});
                    }
                }
            });
        }
        for (auto& x : th) x.join();
        size_t total = 0;
        for (auto& p : parts) total += p.size();
        jobs.reserve(total);
        for (auto& p : parts) jobs.insert(jobs.end(), p.begin(), p.end());
    }
    // 2) rasterize row bands in parallel into per-thread buffers
    int band = (H + nThreads - 1) / nThreads;
    {
        std::vector<std::thread> th;
        for (int t = 0; t < nThreads; t++) {
            int y0 = t * band, y1 = std::min(H, y0 + band);
            if (y0 >= y1) continue;
            th.emplace_back([this, y0, y1, t] {
                uint32_t* p = wpix[t].data();
                uint16_t* z = wz[t].data();
                for (int y = y0; y < y1; y++) {
                    std::memcpy(p + (size_t)y * W, pix.data() + (size_t)y * W, (size_t)W * 4);
                    if (seedFromMain)
                        std::memcpy(z + (size_t)y * W, zbuf.data() + (size_t)y * W, (size_t)W * 2);
                    else
                        std::fill(z + (size_t)y * W, z + (size_t)y * W + W, 0xFFFF);
                }
                for (const TriJob& j : jobs) rasterJob(j, p, z, y0, y1);
            });
        }
        for (auto& x : th) x.join();
    }
    // 3) z-merge worker bands back into the main buffer, in parallel
    {
        std::vector<std::thread> th;
        for (int t = 0; t < nThreads; t++) {
            int y0 = t * band, y1 = std::min(H, y0 + band);
            if (y0 >= y1) continue;
            th.emplace_back([this, t, y0, y1] {
                for (int y = y0; y < y1; y++)
                    for (int x = 0; x < W; x++) {
                        size_t i = (size_t)y * W + x;
                        if (wz[t][i] < zbuf[i]) { zbuf[i] = wz[t][i]; pix[i] = wpix[t][i]; }
                    }
            });
        }
        for (auto& x : th) x.join();
    }
    calls.clear();
    jobs.clear();
}

void Renderer::drawMesh(const Mesh& m, const Mat4& mvp, const Mat4& model, const Vec3& tint, float globalAlpha) {
    if (collecting) { calls.push_back({&m, mvp, model, tint, globalAlpha}); return; }
    // immediate path (translucent / post-opaque): single-threaded into pix/zbuf
    int saved = nThreads;
    nThreads = 1;
    beginOpaque();
    calls.push_back({&m, mvp, model, tint, globalAlpha});
    endOpaque();
    nThreads = saved;
}

void Renderer::rasterJob(const TriJob& j, uint32_t* pixB, uint16_t* zbufB, int y0, int y1) {
    const Mesh& m = *j.m;
    const SV& a = j.a; const SV& b = j.b; const SV& c = j.c;
    float area = edge(a, b, c.sx, c.sy);
    if (area == 0) return;
    float minx = std::min({a.sx, b.sx, c.sx}), maxx = std::max({a.sx, b.sx, c.sx});
    float miny = std::min({a.sy, b.sy, c.sy}), maxy = std::max({a.sy, b.sy, c.sy});
    int x0 = std::max(0, (int)std::floor(minx)), x1 = std::min(W - 1, (int)std::ceil(maxx));
    int ry0 = std::max(y0, (int)std::floor(miny)), ry1 = std::min(y1 - 1, (int)std::ceil(maxy));
    if (x0 > x1 || ry0 > ry1) return;
    float invArea = 1.0f / area;
    bool pos = area > 0;
    bool hasTex = m.tex != nullptr;
    bool hasSplat = m.tex2 != nullptr && m.tex3 != nullptr;
    bool isWater = m.water;
    for (int y = ry0; y <= ry1; y++) {
        float py = y + 0.5f;
        uint32_t* prow = pixB + (size_t)y * W;
        uint16_t* zrow = zbufB + (size_t)y * W;
        for (int x = x0; x <= x1; x++) {
            float px = x + 0.5f;
            float w0 = edge(b, c, px, py), w1 = edge(c, a, px, py), w2 = edge(a, b, px, py);
            if (pos) { if (w0 < 0 || w1 < 0 || w2 < 0) continue; }
            else { if (w0 > 0 || w1 > 0 || w2 > 0) continue; w0 = -w0; w1 = -w1; w2 = -w2; }
            float l0 = w0 * invArea, l1 = w1 * invArea, l2 = w2 * invArea;
            float iw = l0 * a.iw + l1 * b.iw + l2 * c.iw;
            if (iw <= 1e-9f) continue;
            float z = (l0 * a.sz * a.iw + l1 * b.sz * b.iw + l2 * c.sz * c.iw) / iw;
            z = z < 0 ? 0 : (z > 1 ? 1 : z);
            uint16_t z16 = (uint16_t)(z * 65535.0f);
            size_t idx = (size_t)y * W + x;
            if (z16 >= zrow[x]) continue;

            float u = (l0 * a.u + l1 * b.u + l2 * c.u) / iw;
            float v = (l0 * a.v + l1 * b.v + l2 * c.v) / iw;
            float nx = (l0 * a.nx + l1 * b.nx + l2 * c.nx) / iw;
            float ny = (l0 * a.ny + l1 * b.ny + l2 * c.ny) / iw;
            float nz = (l0 * a.nz + l1 * b.nz + l2 * c.nz) / iw;
            float ff = (l0 * a.ff + l1 * b.ff + l2 * c.ff) / iw;
            float tr = (l0 * a.r + l1 * b.r + l2 * c.r) / iw;
            float tg = (l0 * a.g + l1 * b.g + l2 * c.g) / iw;
            float tb = (l0 * a.b + l1 * b.b + l2 * c.b) / iw;
            float al = (l0 * a.a + l1 * b.a + l2 * c.a) / iw;

            float ar = tr, ag = tg, ab = tb;
            if (hasSplat) {
                float s0 = (l0 * a.s0 + l1 * b.s0 + l2 * c.s0) / iw;
                float s1 = (l0 * a.s1 + l1 * b.s1 + l2 * c.s1) / iw;
                float s2 = (l0 * a.s2 + l1 * b.s2 + l2 * c.s2) / iw;
                uint32_t t1 = sampleBil(*m.tex, u, v);
                uint32_t t2 = sampleNear(*m.tex2, u * 1.7f + 0.31f, v * 1.7f + 0.17f);
                uint32_t t3 = sampleNear(*m.tex3, u * 2.3f + 0.53f, v * 2.3f + 0.71f);
                float r1 = ((t1 >> 16) & 255) / 255.f, g1 = ((t1 >> 8) & 255) / 255.f, b1 = (t1 & 255) / 255.f;
                float r2 = ((t2 >> 16) & 255) / 255.f, g2 = ((t2 >> 8) & 255) / 255.f, b2 = (t2 & 255) / 255.f;
                float r3 = ((t3 >> 16) & 255) / 255.f, g3 = ((t3 >> 8) & 255) / 255.f, b3 = (t3 & 255) / 255.f;
                ar = (r1 * s0 + r2 * s1 + r3 * s2) * tr;
                ag = (g1 * s0 + g2 * s1 + g3 * s2) * tg;
                ab = (b1 * s0 + b2 * s1 + b3 * s2) * tb;
            } else if (hasTex) {
                uint32_t t = sampleBil(*m.tex, u, v);
                ar = ((t >> 16) & 255) / 255.f * tr;
                ag = ((t >> 8) & 255) / 255.f * tg;
                ab = (t & 255) / 255.f * tb;
                al *= ((t >> 24) & 255) / 255.0f;
                if (m.alphaTest && ((t >> 24) & 255) < 128) continue;
            }

            float cr, cg, cb;
            if (isWater) {
                float wx = (l0 * a.wx + l1 * b.wx + l2 * c.wx) / iw;
                float wy = (l0 * a.wy + l1 * b.wy + l2 * c.wy) / iw;
                float wz = (l0 * a.wz + l1 * b.wz + l2 * c.wz) / iw;
                float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (nl > 1e-6f) { nx /= nl; ny /= nl; nz /= nl; }
                float rt = L.waveT;
                nx += 0.09f * std::sin(wx * 1.9f + rt * 3.1f) * std::cos(wz * 1.3f - rt * 2.2f);
                nz += 0.09f * std::cos(wx * 1.1f - rt * 2.7f) * std::sin(wz * 2.1f + rt * 2.9f);
                float il = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);
                nx *= il; ny *= il; nz *= il;
                cr = ar; cg = ag; cb = ab;  // vertex color: depth/foam baked
                Vec3 V = L.camPos - Vec3(wx, wy, wz);
                float vl = std::sqrt(V.x * V.x + V.y * V.y + V.z * V.z);
                V = V / (vl > 1e-6f ? vl : 1);
                float fres = std::pow(std::max(0.0f, 1.0f - (V.x * nx + V.y * ny + V.z * nz)), 3.0f);
                cr = lerpf(cr, L.fogCol.x * 1.15f, fres * 0.8f);
                cg = lerpf(cg, L.fogCol.y * 1.15f, fres * 0.8f);
                cb = lerpf(cb, L.fogCol.z * 1.15f, fres * 0.8f);
                Vec3 Hv = L.sunDir + V;
                float hl = std::sqrt(Hv.x * Hv.x + Hv.y * Hv.y + Hv.z * Hv.z);
                Hv = Hv / (hl > 1e-6f ? hl : 1);
                float spec = std::pow(std::max(0.0f, nx * Hv.x + ny * Hv.y + nz * Hv.z), 220.0f) * L.sunI;
                cr += L.sunCol.x * spec; cg += L.sunCol.y * spec; cb += L.sunCol.z * spec;
                al = 0.78f + 0.2f * fres;
            } else if (m.emissive) {
                cr = ar; cg = ag; cb = ab;
            } else {
                float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (nl > 1e-6f) { nx /= nl; ny /= nl; nz /= nl; }
                float diff = std::max(0.0f, nx * L.sunDir.x + ny * L.sunDir.y + nz * L.sunDir.z);
                cr = ar * (L.amb + L.sunI * diff * L.sunCol.x);
                cg = ag * (L.amb + L.sunI * diff * L.sunCol.y);
                cb = ab * (L.amb + L.sunI * diff * L.sunCol.z);
            }
            cr = lerpf(cr, L.fogCol.x, ff);
            cg = lerpf(cg, L.fogCol.y, ff);
            cb = lerpf(cb, L.fogCol.z, ff);

            zrow[x] = z16;
            if (al >= 0.999f) {
                prow[x] = rgb(cr, cg, cb);
            } else {
                if (al <= 0.004f) { zrow[x] = 0xFFFF; continue; }
                uint32_t d = prow[x];
                float dr = ((d >> 16) & 255) / 255.f, dg = ((d >> 8) & 255) / 255.f, db = (d & 255) / 255.f;
                prow[x] = rgb(lerpf(dr, cr, al), lerpf(dg, cg, al), lerpf(db, cb, al));
            }
        }
    }
}

// ---------------------------------------------------------------- 2D

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
                row[xx] = rgb(lerpf(((d >> 16) & 255) / 255.f, cr, alpha),
                              lerpf(((d >> 8) & 255) / 255.f, cg, alpha),
                              lerpf((d & 255) / 255.f, cb, alpha));
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

// ---------------------------------------------------------------- post

static inline float aces(float c) {
    return clampf((c * (2.51f * c + 0.03f)) / (c * (2.43f * c + 0.59f) + 0.14f), 0, 1);
}

void Renderer::present(uint32_t* dst, int dw, int dh, float vignette, uint32_t frameNo) {
    float sx = (float)W / dw, sy = (float)H / dh;
    winBuf.assign((size_t)dw * dh, 0);
    auto dsRow = [&](int y) {
        for (int x = 0; x < dw; x++) {
            int x0 = (int)(x * sx), x1 = std::max(x0 + 1, (int)((x + 1) * sx));
            int y0 = (int)(y * sy), y1 = std::max(y0 + 1, (int)((y + 1) * sy));
            float r = 0, g = 0, b = 0;
            int n = 0;
            for (int yy = y0; yy < y1 && yy < H; yy++)
                for (int xx = x0; xx < x1 && xx < W; xx++) {
                    uint32_t p = pix[(size_t)yy * W + xx];
                    r += (p >> 16) & 255; g += (p >> 8) & 255; b += p & 255;
                    n++;
                }
            winBuf[(size_t)y * dw + x] = rgb(r / (n * 255.f), g / (n * 255.f), b / (n * 255.f));
        }
    };
    {
        std::vector<std::thread> th;
        int band = (dh + nThreads - 1) / nThreads;
        for (int t = 0; t < nThreads; t++) {
            int y0 = t * band, y1 = std::min(dh, y0 + band);
            if (y0 >= y1) continue;
            th.emplace_back([&dsRow, y0, y1] { for (int y = y0; y < y1; y++) dsRow(y); });
        }
        for (auto& x : th) x.join();
    }
    int bw = std::max(1, dw / 4), bh = std::max(1, dh / 4);
    bloomA.assign((size_t)bw * bh, 0);
    for (int y = 0; y < bh; y++)
        for (int x = 0; x < bw; x++) {
            float r = 0, g = 0, b = 0;
            int n = 0;
            for (int yy = y * 4; yy < std::min(y * 4 + 4, dh); yy++)
                for (int xx = x * 4; xx < std::min(x * 4 + 4, dw); xx++) {
                    uint32_t p = winBuf[(size_t)yy * dw + xx];
                    r += (p >> 16) & 255; g += (p >> 8) & 255; b += p & 255;
                    n++;
                }
            r /= n; g /= n; b /= n;
            float lum = (r * 0.5f + g * 0.35f + b * 0.15f) / 255.f;
            float k = clampf((lum - 0.72f) * 3.0f, 0, 1);
            bloomA[(size_t)y * bw + x] = rgb((r / 255.f) * k, (g / 255.f) * k, (b / 255.f) * k);
        }
    bloomB.assign(bloomA.size(), 0);
    std::vector<uint32_t> tmp(bloomA.size());
    for (int y = 0; y < bh; y++)
        for (int x = 0; x < bw; x++) {
            float r = 0, g = 0, b = 0; int n = 0;
            for (int d = -3; d <= 3; d++) {
                int xx = x + d;
                if (xx < 0 || xx >= bw) continue;
                uint32_t p = bloomA[(size_t)y * bw + xx];
                r += (p >> 16) & 255; g += (p >> 8) & 255; b += p & 255; n++;
            }
            tmp[(size_t)y * bw + x] = rgb(r / (n * 255.f), g / (n * 255.f), b / (n * 255.f));
        }
    for (int y = 0; y < bh; y++)
        for (int x = 0; x < bw; x++) {
            float r = 0, g = 0, b = 0; int n = 0;
            for (int d = -3; d <= 3; d++) {
                int yy = y + d;
                if (yy < 0 || yy >= bh) continue;
                uint32_t p = tmp[(size_t)yy * bw + x];
                r += (p >> 16) & 255; g += (p >> 8) & 255; b += p & 255; n++;
            }
            bloomB[(size_t)y * bw + x] = rgb(r / (n * 255.f), g / (n * 255.f), b / (n * 255.f));
        }
    auto finRow = [&](int y) {
        for (int x = 0; x < dw; x++) {
            uint32_t p = winBuf[(size_t)y * dw + x];
            float r = ((p >> 16) & 255) / 255.f, g = ((p >> 8) & 255) / 255.f, b = (p & 255) / 255.f;
            uint32_t bl = bloomB[(size_t)(y / 4) * bw + (x / 4)];
            r += ((bl >> 16) & 255) / 255.f * 0.55f;
            g += ((bl >> 8) & 255) / 255.f * 0.55f;
            b += (bl & 255) / 255.f * 0.55f;
            r = aces(r); g = aces(g); b = aces(b);
            float lum = r * 0.3f + g * 0.55f + b * 0.15f;
            r = lerpf(lum, r, 1.12f); g = lerpf(lum, g, 1.12f); b = lerpf(lum, b, 1.12f);
            float nx = (x + 0.5f) / dw * 2 - 1, ny = (y + 0.5f) / dh * 2 - 1;
            float vig = 1.0f - vignette * (nx * nx + ny * ny) * 0.5f;
            float grain = (((hash2i(x + (int)frameNo * 131, y + (int)frameNo * 57) & 255) / 255.f) - 0.5f) * 0.04f;
            dst[(size_t)y * dw + x] = rgb(clampf(r * vig + grain, 0, 1),
                                          clampf(g * vig + grain, 0, 1),
                                          clampf(b * vig + grain, 0, 1));
        }
    };
    {
        std::vector<std::thread> th;
        int band = (dh + nThreads - 1) / nThreads;
        for (int t = 0; t < nThreads; t++) {
            int y0 = t * band, y1 = std::min(dh, y0 + band);
            if (y0 >= y1) continue;
            th.emplace_back([&finRow, y0, y1] { for (int y = y0; y < y1; y++) finRow(y); });
        }
        for (auto& x : th) x.join();
    }
}
