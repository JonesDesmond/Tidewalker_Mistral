#pragma once
#include "math.h"
#include "renderer.h"

// Procedural mesh builders. All vertices carry uv; meshes reference a Tex.
Mesh makeBox(float sx, float sy, float sz, const Vec3& col);
Mesh makeCylinder(float r, float h, int seg, const Vec3& col, bool capTop = true, bool capBot = true);
Mesh makeCone(float r, float h, int seg, const Vec3& col);
Mesh makeRock(float r, int seed, const Vec3& col);          // jittered icosphere
Mesh makeTuft(float h, const Vec3& col);                    // 2 crossed quads, alphaTest
Mesh makeBlob(float r);                                      // flat circle in XZ (blob shadow)
// Bakes src (transformed by m, tinted) into dst, inheriting dst's texture flags.
void appendMesh(Mesh& dst, const Mesh& src, const Mat4& m, const Vec3& tint);
