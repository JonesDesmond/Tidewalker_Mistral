#include "texgen.h"
#include "math.h"

static inline void set(Tex& t, int x, int y, float r, float g, float b, float a = 1.0f) {
    auto cl = [](float v) -> uint32_t { return v < 0 ? 0 : (v > 1 ? 255 : (uint32_t)(v * 255 + 0.5f)); };
    t.px[(size_t)y * t.w + x] = (cl(a) << 24) | (cl(r) << 16) | (cl(g) << 8) | cl(b);
}

static Tex noiseTex(int s, float r, float g, float b, float var, int oct, float scale) {
    Tex t(s, s);
    for (int y = 0; y < s; y++)
        for (int x = 0; x < s; x++) {
            float n = fbm(x * scale / s, y * scale / s, oct);
            float n2 = fbm(x * scale * 3.1f / s + 40, y * scale * 3.1f / s, 3);
            float v = 1.0f + (n - 0.5f) * var + (n2 - 0.5f) * var * 0.6f;
            set(t, x, y, r * v, g * v, b * v);
        }
    return t;
}

static inline uint32_t scalePx(uint32_t p, float m) {
    uint32_t a = (p >> 24) & 255;
    uint32_t r = std::min(255.0f, ((p >> 16) & 255) * m);
    uint32_t g = std::min(255.0f, ((p >> 8) & 255) * m);
    uint32_t b = std::min(255.0f, (p & 255) * m);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

Tex makeTexSand() {
    Tex t = noiseTex(128, 0.72f, 0.64f, 0.47f, 0.22f, 4, 6);
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) {
            float rip = std::sin((y + fbm(x * 0.05f, y * 0.05f, 2) * 8) * 0.9f) * 0.5f + 0.5f;
            t.px[(size_t)y * 128 + x] = scalePx(t.px[(size_t)y * 128 + x], 1.0f + (rip - 0.5f) * 0.10f);
        }
    return t;
}

Tex makeTexGrass() {
    Tex t = noiseTex(128, 0.23f, 0.38f, 0.16f, 0.45f, 5, 10);
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) {
            float blade = fbm(x * 0.35f, y * 0.35f, 3);
            if (blade > 0.58f) {
                uint32_t& p = t.px[(size_t)y * 128 + x];
                uint32_t r = std::min(255u, ((p >> 16) & 255) + 30u);
                uint32_t g = std::min(255u, ((p >> 8) & 255) + 45u);
                uint32_t b = (p & 255) + 10u;
                p = (255u << 24) | (r << 16) | (g << 8) | std::min(255u, b);
            }
        }
    return t;
}

Tex makeTexRock() {
    Tex t = noiseTex(128, 0.42f, 0.40f, 0.37f, 0.35f, 5, 8);
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) {
            float crack = fbm(x * 0.09f + 90, y * 0.09f, 4);
            if (crack < 0.30f)
                t.px[(size_t)y * 128 + x] = scalePx(t.px[(size_t)y * 128 + x], 0.55f);
        }
    return t;
}

Tex makeTexWood() {
    Tex t(128, 128);
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) {
            int plank = (x / 32) % 2;
            float grain = fbm(x * 0.02f, y * 0.35f, 4);
            float n = fbm(x * 0.08f, y * 0.08f, 3);
            float v = 0.75f + grain * 0.35f + (n - 0.5f) * 0.2f + plank * 0.08f;
            float r = 0.42f * v, g = 0.28f * v, b = 0.15f * v;
            if (x % 32 == 0) { r *= 0.5f; g *= 0.5f; b *= 0.5f; }  // plank seams
            set(t, x, y, r, g, b);
        }
    return t;
}

Tex makeTexStone() {
    Tex t(128, 128);
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) {
            int row = y / 32;
            int off = (row % 2) * 32;
            bool mortar = (y % 32 == 0) || ((x + off) % 64 == 0);
            float n = fbm(x * 0.1f, y * 0.1f, 4);
            float v = mortar ? 0.55f : 0.8f + (n - 0.5f) * 0.35f;
            set(t, x, y, 0.52f * v, 0.51f * v, 0.48f * v);
        }
    return t;
}

Tex makeTexLeaf() {
    Tex t(64, 64);
    std::fill(t.px.begin(), t.px.end(), 0);  // transparent
    for (int b = 0; b < 9; b++) {
        float bx = 8 + b * 6 + (hash2i(b, 3) % 5);
        for (int y = 0; y < 60; y++) {
            float sway = std::sin(y * 0.15f + b) * 3.0f;
            int x = (int)(bx + sway);
            if (x < 0 || x >= 64) continue;
            float h = 1.0f - y / 60.0f;
            set(t, x, y, 0.16f + 0.15f * h, 0.38f + 0.25f * h, 0.12f + 0.1f * h);
            if (x + 1 < 64) set(t, x + 1, y, 0.13f + 0.12f * h, 0.32f + 0.2f * h, 0.10f);
        }
    }
    return t;
}

Tex makeTexKelp() {
    Tex t(64, 64);
    std::fill(t.px.begin(), t.px.end(), 0);
    for (int b = 0; b < 5; b++) {
        float bx = 10 + b * 12 + (hash2i(b, 7) % 6);
        for (int y = 0; y < 62; y++) {
            float sway = std::sin(y * 0.12f + b * 2.0f) * (y * 0.09f);
            int x = (int)(bx + sway);
            if (x < 0 || x >= 64) continue;
            float h = 1.0f - y / 62.0f;
            set(t, x, y, 0.05f + 0.1f * h, 0.25f + 0.2f * h, 0.13f + 0.08f * h);
        }
    }
    return t;
}

Tex makeTexClouds() {
    Tex t(512, 256);
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 512; x++) {
            float u = (float)x / 512, v = (float)y / 256;
            float n = fbm(u * 6.0f, v * 6.0f, 5) * 0.7f + fbm(u * 14.0f + 9, v * 14.0f, 3) * 0.3f;
            n = smoothstepf(0.42f, 0.75f, n);
            float r = n, g = n, b = n;
            set(t, x, y, r, g, b);
        }
    return t;
}
