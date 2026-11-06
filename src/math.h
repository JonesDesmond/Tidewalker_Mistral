#pragma once
#include <cmath>
#include <cstdint>

struct Vec2 { float x = 0, y = 0; };

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const { return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x}; }
    float len() const { return std::sqrt(dot(*this)); }
    Vec3 norm() const { float l = len(); return l > 1e-9f ? *this / l : Vec3(); }
};
inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

struct Vec4 { float x = 0, y = 0, z = 0, w = 1; };

struct Mat4 {
    float m[16] = {};  // column-major
    static Mat4 ident() { Mat4 r; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1; return r; }
    static Mat4 translate(const Vec3& t) { Mat4 r = ident(); r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r; }
    static Mat4 scale(const Vec3& s) { Mat4 r; r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z; r.m[15] = 1; return r; }
    static Mat4 rotX(float a) { float c = std::cos(a), s = std::sin(a); Mat4 r = ident(); r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c; return r; }
    static Mat4 rotY(float a) { float c = std::cos(a), s = std::sin(a); Mat4 r = ident(); r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c; return r; }
    static Mat4 rotZ(float a) { float c = std::cos(a), s = std::sin(a); Mat4 r = ident(); r.m[0] = c; r.m[1] = -s; r.m[4] = s; r.m[5] = c; return r; }
    static Mat4 perspective(float fovY, float aspect, float zn, float zf) {
        float f = 1.0f / std::tan(fovY * 0.5f);
        Mat4 r;
        r.m[0] = f / aspect; r.m[5] = f;
        r.m[10] = (zf + zn) / (zn - zf); r.m[11] = -1;
        r.m[14] = 2 * zf * zn / (zn - zf);
        return r;
    }
    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Vec3 f = (target - eye).norm();
        Vec3 s = f.cross(up).norm();
        Vec3 u = s.cross(f);
        Mat4 r = ident();
        r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
        r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
        r.m[12] = -s.dot(eye); r.m[13] = -u.dot(eye); r.m[14] = f.dot(eye);
        return r;
    }
};
inline Vec4 xform4(const Mat4& m, const Vec3& v) {
    float x = v.x, y = v.y, z = v.z;
    return {
        m.m[0] * x + m.m[4] * y + m.m[8] * z + m.m[12],
        m.m[1] * x + m.m[5] * y + m.m[9] * z + m.m[13],
        m.m[2] * x + m.m[6] * y + m.m[10] * z + m.m[14],
        m.m[3] * x + m.m[7] * y + m.m[11] * z + m.m[15],
    };
}
inline Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}

inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline Vec3 lerpf3(const Vec3& a, const Vec3& b, float t) { return {lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t)}; }
inline float smoothstepf(float a, float b, float t) {
    float x = clampf((t - a) / (b - a), 0, 1);
    return x * x * (3 - 2 * x);
}

inline uint32_t hash2i(int x, int z) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)z * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
inline float hash01(int x, int z) { return (hash2i(x, z) & 1023) / 1024.0f; }

inline float vnoise(float x, float z) {
    int xi = (int)std::floor(x), zi = (int)std::floor(z);
    float xf = x - xi, zf = z - zi;
    float a = hash01(xi, zi), b = hash01(xi + 1, zi);
    float c = hash01(xi, zi + 1), d = hash01(xi + 1, zi + 1);
    float u = xf * xf * (3 - 2 * xf), v = zf * zf * (3 - 2 * zf);
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v;
}
inline float fbm(float x, float z, int oct) {
    float s = 0, a = 1, f = 1, t = 0;
    for (int i = 0; i < oct; i++) { s += a * vnoise(x * f, z * f); t += a; a *= 0.5f; f *= 2.0f; }
    return s / t;
}
