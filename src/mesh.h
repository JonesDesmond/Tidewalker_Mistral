#pragma once
#include "math.h"
#include "renderer.h"

// Procedural mesh builders. All meshes are triangle soup with per-vertex
// color/normal; the game computes lighting per vertex each frame.
Mesh makeBox(float sx, float sy, float sz, const Vec3& col);
Mesh makeCylinder(float r, float h, int seg, const Vec3& col, bool capTop = true, bool capBot = true);
Mesh makeCone(float r, float h, int seg, const Vec3& col);
// Appends src (transformed by m, colors tinted) into dst.
void appendMesh(Mesh& dst, const Mesh& src, const Mat4& m, const Vec3& tint);
