#include "mesh.h"

Mesh makeBox(float sx, float sy, float sz, const Vec3& col) {
    Mesh m;
    float x = sx / 2, y = sy / 2, z = sz / 2;
    Vec3 p[8] = {{-x, -y, -z}, {x, -y, -z}, {x, y, -z}, {-x, y, -z},
                 {-x, -y, z}, {x, -y, z}, {x, y, z}, {-x, y, z}};
    int faces[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
                       {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}};
    for (auto& f : faces) {
        Vec3 n = (p[f[1]] - p[f[0]]).cross(p[f[2]] - p[f[0]]).norm();
        int a = m.addVert(p[f[0]], col, n), b = m.addVert(p[f[1]], col, n);
        int c = m.addVert(p[f[2]], col, n), d = m.addVert(p[f[3]], col, n);
        m.addTri(a, b, c); m.addTri(a, c, d);
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
        int a = m.addVert(p0, col, n0), b = m.addVert(p1, col, n1);
        int c = m.addVert(p2, col, n1), d = m.addVert(p3, col, n0);
        m.addTri(a, b, c); m.addTri(a, c, d);
        if (capTop) {
            int ce = m.addVert({0, y1, 0}, col, {0, 1, 0});
            m.addTri(ce, d, c);
        }
        if (capBot) {
            int ce = m.addVert({0, y0, 0}, col, {0, -1, 0});
            m.addTri(ce, b, a);
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
        int a = m.addVert(p0, col, n), b = m.addVert(p1, col, n), c = m.addVert(tip, col, n);
        m.addTri(a, b, c);
    }
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
    }
    for (size_t t = 0; t < src.tri.size() / 3; t++)
        dst.addTri(base + src.tri[t * 3], base + src.tri[t * 3 + 1], base + src.tri[t * 3 + 2],
                   t < src.alpha.size() ? src.alpha[t] : 1.0f);
}
