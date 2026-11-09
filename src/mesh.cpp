#include "mesh.h"
#include <cmath>

Mesh makeBox(float sx, float sy, float sz, const Vec3& col) {
    Mesh m;
    float x = sx / 2, y = sy / 2, z = sz / 2;
    Vec3 p[8] = {{-x, -y, -z}, {x, -y, -z}, {x, y, -z}, {-x, y, -z},
                 {-x, -y, z}, {x, -y, z}, {x, y, z}, {-x, y, z}};
    int faces[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
                       {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}};
    Vec2 fuv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (auto& f : faces) {
        Vec3 n = (p[f[1]] - p[f[0]]).cross(p[f[2]] - p[f[0]]).norm();
        int v[4];
        for (int k = 0; k < 4; k++) v[k] = m.addVert(p[f[k]], col, n, fuv[k]);
        m.addTri(v[0], v[1], v[2]); m.addTri(v[0], v[2], v[3]);
    }
    return m;
}

Mesh makeCylinder(float r, float h, int seg, const Vec3& col, bool capTop, bool capBot) {
    Mesh m;
    float y0 = -h / 2, y1 = h / 2;
    for (int i = 0; i < seg; i++) {
        float a0 = (float)i / seg * 6.2831853f, a1 = (float)(i + 1) / seg * 6.2831853f;
        Vec3 p0 = {std::cos(a0) * r, y0, std::sin(a0) * r}, p1 = {std::cos(a1) * r, y0, std::sin(a1) * r};
        Vec3 p2 = {std::cos(a1) * r, y1, std::sin(a1) * r}, p3 = {std::cos(a0) * r, y1, std::sin(a0) * r};
        Vec3 n0 = {std::cos(a0), 0, std::sin(a0)}, n1 = {std::cos(a1), 0, std::sin(a1)};
        int a = m.addVert(p0, col, n0, {a0 / 6.2831853f, 0});
        int b = m.addVert(p1, col, n1, {a1 / 6.2831853f, 0});
        int c = m.addVert(p2, col, n1, {a1 / 6.2831853f, 1});
        int d = m.addVert(p3, col, n0, {a0 / 6.2831853f, 1});
        m.addTri(a, b, c); m.addTri(a, c, d);
        if (capTop) {
            int ce = m.addVert({0, y1, 0}, col, {0, 1, 0}, {0.5f, 0.5f});
            int e = m.addVert(p3, col, {0, 1, 0}, {0.5f + p3.x / (2 * r) * 0.5f, 0.5f + p3.z / (2 * r) * 0.5f});
            int f = m.addVert(p2, col, {0, 1, 0}, {0.5f + p2.x / (2 * r) * 0.5f, 0.5f + p2.z / (2 * r) * 0.5f});
            m.addTri(ce, e, f);
        }
        if (capBot) {
            int ce = m.addVert({0, y0, 0}, col, {0, -1, 0}, {0.5f, 0.5f});
            int e = m.addVert(p1, col, {0, -1, 0}, {0.5f, 0.5f});
            int f = m.addVert(p0, col, {0, -1, 0}, {0.5f, 0.5f});
            m.addTri(ce, e, f);
        }
    }
    return m;
}

Mesh makeCone(float r, float h, int seg, const Vec3& col) {
    Mesh m;
    float y0 = -h / 2, y1 = h / 2;
    for (int i = 0; i < seg; i++) {
        float a0 = (float)i / seg * 6.2831853f, a1 = (float)(i + 1) / seg * 6.2831853f;
        Vec3 p0 = {std::cos(a0) * r, y0, std::sin(a0) * r}, p1 = {std::cos(a1) * r, y0, std::sin(a1) * r};
        Vec3 tip = {0, y1, 0};
        Vec3 n = (p1 - p0).cross(tip - p0).norm();
        int a = m.addVert(p0, col, n, {a0 / 6.2831853f, 0});
        int b = m.addVert(p1, col, n, {a1 / 6.2831853f, 0});
        int c = m.addVert(tip, col, n, {0.5f, 1});
        m.addTri(a, b, c);
    }
    return m;
}

Mesh makeRock(float r, int seed, const Vec3& col) {
    // jittered icosahedron
    static const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
    Vec3 base[12] = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t},
                     {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
    int idx[20][3] = {{0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4},
                      {11, 10, 2}, {10, 7, 6}, {7, 1, 8}, {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8},
                      {3, 8, 9}, {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}};
    Mesh m;
    int v[12];
    for (int i = 0; i < 12; i++) {
        Vec3 p = base[i].norm() * r * (0.8f + hash01(seed + i, seed) * 0.45f);
        float u = (float)i / 12, vv = (i % 2) ? 1.0f : 0.0f;
        v[i] = m.addVert(p, col, p.norm(), {u, vv});
    }
    for (auto& f : idx) m.addTri(v[f[0]], v[f[1]], v[f[2]]);
    return m;
}

Mesh makeTuft(float h, const Vec3& col) {
    Mesh m;
    float w = h * 0.55f;
    // two crossed quads, alpha-tested leaf texture
    Vec3 p[8] = {{-w, 0, 0}, {w, 0, 0}, {w, h, 0}, {-w, h, 0},
                 {0, 0, -w}, {0, 0, w}, {0, h, w}, {0, h, -w}};
    int quads[2][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}};
    Vec2 uv[4] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    for (auto& q : quads) {
        int a = m.addVert(p[q[0]], col, {0, 0, 1}, uv[0]);
        int b = m.addVert(p[q[1]], col, {0, 0, 1}, uv[1]);
        int c = m.addVert(p[q[2]], col, {0, 0, 1}, uv[2]);
        int d = m.addVert(p[q[3]], col, {0, 0, 1}, uv[3]);
        m.addTri(a, b, c, 1.0f); m.addTri(a, c, d, 1.0f);
        m.addTri(b, a, d, 1.0f); m.addTri(c, b, d, 1.0f);  // double-sided
    }
    m.alphaTest = true;
    return m;
}

Mesh makeBlob(float r) {
    Mesh m;
    int seg = 12;
    int ce = m.addVert({0, 0, 0}, {1, 1, 1}, {0, 1, 0}, {0.5f, 0.5f});
    for (int i = 0; i <= seg; i++) {
        float a = (float)i / seg * 6.2831853f;
        m.addVert({std::cos(a) * r, 0, std::sin(a) * r}, {1, 1, 1}, {0, 1, 0},
                  {0.5f + std::cos(a) * 0.5f, 0.5f + std::sin(a) * 0.5f});
    }
    for (int i = 1; i <= seg; i++) m.addTri(ce, i, i + 1, 1.0f);
    return m;
}

void appendMesh(Mesh& dst, const Mesh& src, const Mat4& m, const Vec3& tint) {
    int base = (int)dst.pos.size();
    for (size_t i = 0; i < src.pos.size(); i++) {
        Vec4 v = xform4(m, src.pos[i]);
        Vec3 c = {src.col[i].x * tint.x, src.col[i].y * tint.y, src.col[i].z * tint.z};
        Vec4 n4 = xform4(m, src.nrm[i]);
        dst.pos.push_back({v.x / v.w, v.y / v.w, v.z / v.w});
        dst.col.push_back(c);
        dst.nrm.push_back(Vec3(n4.x, n4.y, n4.z).norm());
        dst.uv.push_back(i < src.uv.size() ? src.uv[i] : Vec2{0, 0});
        dst.splat.push_back(i < src.splat.size() ? src.splat[i] : Vec3{1, 0, 0});
    }
    for (size_t t = 0; t < src.tri.size() / 3; t++)
        dst.addTri(base + src.tri[t * 3], base + src.tri[t * 3 + 1], base + src.tri[t * 3 + 2],
                   t < src.alpha.size() ? src.alpha[t] : 1.0f);
    if (!dst.tex) dst.tex = src.tex;
    if (!dst.tex2) dst.tex2 = src.tex2;
    if (!dst.tex3) dst.tex3 = src.tex3;
    if (src.alphaTest) dst.alphaTest = true;
    if (src.water) dst.water = true;
    if (src.emissive) dst.emissive = true;
}
