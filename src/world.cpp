#include "world.h"
#include "mesh.h"
#include <cmath>
#include <algorithm>

struct Island { float x, z, r, peak; };
static const Island ISLANDS[] = {
    {0, 0, 280, 15},        // main island: village + dock
    {560, -300, 190, 11},   // grotto island
    {-600, -200, 160, 13},
    {-280, 540, 180, 10},
    {320, 580, 150, 9},
    {-60, -580, 130, 8},    // lighthouse isle
    {740, 320, 160, 7},
};

float World::heightAt(float x, float z) const {
    float fx = (x + HALF) / SIZE * (HM - 1);
    float fz = (z + HALF) / SIZE * (HM - 1);
    int x0 = std::clamp((int)fx, 0, HM - 2), z0 = std::clamp((int)fz, 0, HM - 2);
    float tx = std::clamp(fx - x0, 0.0f, 1.0f), tz = std::clamp(fz - z0, 0.0f, 1.0f);
    float h00 = hm[z0 * HM + x0], h10 = hm[z0 * HM + x0 + 1];
    float h01 = hm[(z0 + 1) * HM + x0], h11 = hm[(z0 + 1) * HM + x0 + 1];
    return lerpf(lerpf(h00, h10, tx), lerpf(h01, h11, tx), tz);
}

bool World::inGrotto(float x, float z) const {
    float dx = x - grottoCenter.x, dz = z - grottoCenter.z;
    return dx * dx + dz * dz < grottoRadius * grottoRadius;
}
float World::grottoFloorAt(float x, float z) const {
    float dx = x - grottoCenter.x, dz = z - grottoCenter.z;
    float d = std::sqrt(dx * dx + dz * dz);
    return -0.9f + (d / grottoRadius) * (d / grottoRadius) * 5.0f;
}

float World::tideLevel(float t, float period) {
    float p = std::fmod(t, period) / period;
    if (p < 0.0f) p += 1.0f;
    if (p < 0.125f) return TIDE_HIGH;
    if (p < 0.5f) { float f = (p - 0.125f) / 0.375f; f = f * f * (3 - 2 * f); return lerpf(TIDE_HIGH, TIDE_LOW, f); }
    if (p < 0.625f) return TIDE_LOW;
    float f = (p - 0.625f) / 0.375f; f = f * f * (3 - 2 * f);
    return lerpf(TIDE_LOW, TIDE_HIGH, f);
}

void World::tidePhase(float t, float period, std::string& name, float& secondsLeft) {
    float p = std::fmod(t, period) / period;
    if (p < 0.0f) p += 1.0f;
    if (p < 0.125f) { name = "HIGH TIDE"; secondsLeft = (0.125f - p) * period; }
    else if (p < 0.5f) { name = "EBB - WATER FALLING"; secondsLeft = (0.5f - p) * period; }
    else if (p < 0.625f) { name = "LOW TIDE"; secondsLeft = (0.625f - p) * period; }
    else { name = "FLOOD - WATER RISING"; secondsLeft = (1.0f - p) * period; }
}

float World::wave(float x, float z, float t) {
    return 0.16f * std::sin(0.11f * x + 1.3f * t)
         + 0.11f * std::sin(0.13f * z - 1.7f * t)
         + 0.07f * std::sin(0.07f * (x + z) + 0.8f * t);
}

static Vec3 terrainColor(float h, float slope, float x, float z) {
    float n = fbm(x * 0.05f, z * 0.05f, 3);
    Vec3 sand = {0.62f * (0.85f + 0.3f * n), 0.56f * (0.85f + 0.3f * n), 0.42f * (0.85f + 0.3f * n)};
    Vec3 wetSand = {0.45f, 0.42f, 0.33f};
    Vec3 grass = {0.22f + 0.1f * n, 0.40f + 0.08f * n, 0.18f};
    Vec3 rock = {0.42f * (0.85f + 0.3f * n), 0.40f, 0.38f};
    Vec3 kelp = {0.08f, 0.22f, 0.13f};
    Vec3 deep = {0.30f, 0.33f, 0.28f};
    Vec3 c = deep;
    if (h > World::TIDE_LOW - 0.6f) c = lerpf3(deep, sand, smoothstepf(World::TIDE_LOW - 0.6f, World::TIDE_LOW + 1.2f, h));
    if (h > World::TIDE_LOW + 0.6f) c = lerpf3(sand, wetSand, smoothstepf(World::TIDE_LOW + 0.6f, World::TIDE_HIGH - 0.5f, h));
    if (h > World::TIDE_HIGH - 0.5f) {
        float g = smoothstepf(World::TIDE_HIGH - 0.5f, World::TIDE_HIGH + 2.5f, h);
        c = lerpf3(wetSand, grass, g);
        float r = smoothstepf(0.55f, 0.85f, slope);
        c = lerpf3(c, rock, r);
        if (h > 8.0f) c = lerpf3(c, rock, smoothstepf(8.0f, 13.0f, h));
    }
    if (h < World::TIDE_LOW - 0.4f && h > -7.0f && fbm(x * 0.03f + 9.0f, z * 0.03f, 3) > 0.62f) c = kelp;
    return c;
}

void World::build() {
    // Heightmap.
    for (int j = 0; j < HM; j++)
        for (int i = 0; i < HM; i++) {
            float x = -HALF + (float)i / (HM - 1) * SIZE;
            float z = -HALF + (float)j / (HM - 1) * SIZE;
            float h = -5.5f + fbm(x * 0.008f + 7.3f, z * 0.008f - 2.1f, 4) * 3.0f;
            for (const Island& isl : ISLANDS) {
                float dx = x - isl.x, dz = z - isl.z;
                float d = std::sqrt(dx * dx + dz * dz);
                if (d < isl.r) {
                    float q = d / isl.r;
                    float ih = isl.peak * (1 - q * q) * (1 - q * q) - 2.5f;
                    ih += fbm(x * 0.02f, z * 0.02f, 3) * 2.2f;
                    h = std::max(h, ih);
                }
            }
            float dx = x - grottoCenter.x, dz = z - grottoCenter.z;
            float gd = std::sqrt(dx * dx + dz * dz);
            if (gd < grottoRadius + 4) h = std::min(h, grottoFloorAt(x, z));
            float vd = std::sqrt(x * x + z * z);
            if (vd < 95) h = lerpf(h, std::max(h, 6.5f), smoothstepf(95.0f, 45.0f, vd));
            hm[j * HM + i] = h;
        }

    // Terrain mesh (80x80 cells).
    const int TN = 96;
    for (int j = 0; j <= TN; j++)
        for (int i = 0; i <= TN; i++) {
            float x = -HALF + (float)i / TN * SIZE;
            float z = -HALF + (float)j / TN * SIZE;
            float h = heightAt(x, z);
            float sx = heightAt(x + 4, z) - heightAt(x - 4, z);
            float sz = heightAt(x, z + 4) - heightAt(x, z - 4);
            float slope = std::sqrt(sx * sx + sz * sz) / 8.0f;
            terrain.addVert({x, h, z}, terrainColor(h, slope, x, z), {0, 1, 0});
        }
    for (int j = 0; j < TN; j++)
        for (int i = 0; i < TN; i++) {
            int a = j * (TN + 1) + i, b = a + 1, c = a + TN + 1, d = c + 1;
            terrain.addTri(a, c, b); terrain.addTri(b, c, d);
        }

    // Water grid template.
    for (int j = 0; j <= WN; j++)
        for (int i = 0; i <= WN; i++)
            waterBase.push_back({-HALF + (float)i / WN * SIZE, -HALF + (float)j / WN * SIZE});
    for (int j = 0; j <= WN; j++)
        for (int i = 0; i <= WN; i++)
            waterMesh.addVert({waterBase[j * (WN + 1) + i].x, 0, waterBase[j * (WN + 1) + i].y}, {0.1f, 0.2f, 0.3f}, {0, 1, 0});
    for (int j = 0; j < WN; j++)
        for (int i = 0; i < WN; i++) {
            int a = j * (WN + 1) + i, b = a + 1, c = a + WN + 1, d = c + 1;
            waterMesh.addTri(a, c, b, 0.8f); waterMesh.addTri(b, c, d, 0.8f);
        }

    // Dock: scan the main island shore toward the wreck for a spot near the waterline.
    Vec3 wreckPos = {280, 0, -150};
    float dl = std::sqrt(wreckPos.x * wreckPos.x + wreckPos.z * wreckPos.z);
    float ux = wreckPos.x / dl, uz = wreckPos.z / dl;
    for (float d = 150; d < 275; d += 2) {
        float x = ux * d, z = uz * d;
        float h = heightAt(x, z);
        if (h > 0.3f && h < 1.4f) { dockPos = {x, h, z}; break; }
    }
    for (float d = 275; d > 150; d -= 2) {
        float x = ux * d, z = uz * d;
        if (heightAt(x, z) < -1.5f) { boatStart = {x, 0, z}; break; }
    }

    Vec3 rock = {0.40f, 0.38f, 0.36f}, rock2 = {0.34f, 0.32f, 0.30f};
    Vec3 wood = {0.30f, 0.20f, 0.12f}, woodDark = {0.20f, 0.13f, 0.08f};
    Vec3 red = {0.55f, 0.14f, 0.10f}, white = {0.80f, 0.78f, 0.72f};
    Vec3 stone = {0.55f, 0.54f, 0.50f};

    // Wreck (in the channel between main island and the grotto island).
    {
        float wy = heightAt(wreckPos.x, wreckPos.z);
        Mesh hull = makeBox(16, 3.2f, 5.5f, woodDark);
        appendMesh(props, hull, Mat4::translate({wreckPos.x, wy + 2.2f, wreckPos.z}) * Mat4::rotY(0.5f) * Mat4::rotZ(0.06f), {1, 1, 1});
        Mesh bow = makeBox(4, 2.4f, 4.5f, wood);
        appendMesh(props, bow, Mat4::translate({wreckPos.x + 8.5f, wy + 2.6f, wreckPos.z - 1.5f}) * Mat4::rotY(0.5f) * Mat4::rotZ(-0.12f), {1, 1, 1});
        Mesh mast = makeCylinder(0.25f, 11, 6, wood);
        appendMesh(props, mast, Mat4::translate({wreckPos.x - 2, wy + 6.5f, wreckPos.z + 0.5f}) * Mat4::rotZ(0.35f), {1, 1, 1});
        for (int i = 0; i < 5; i++) {
            Mesh rib = makeBox(0.4f, 3.5f, 0.4f, woodDark);
            appendMesh(props, rib, Mat4::translate({wreckPos.x - 5 + i * 2.6f, wy + 3.4f, wreckPos.z + 2.2f}) * Mat4::rotX(0.5f), {1, 1, 1});
        }
    }

    // Grotto: rock walls around the bowl with an entrance facing the main island,
    // plus an overhead slab so it reads as a cave mouth.
    {
        Vec3 dirToMain = Vec3(-grottoCenter.x, 0, -grottoCenter.z).norm();
        float ang = std::atan2(dirToMain.x, dirToMain.z);
        for (int i = 0; i < 10; i++) {
            float a = ang + (i - 4.5f) * 0.42f;  // gap around ang
            if (std::fabs(i - 4.5f) < 1.2f) continue;
            float wx = grottoCenter.x + std::sin(a) * (grottoRadius + 2);
            float wz = grottoCenter.z + std::cos(a) * (grottoRadius + 2);
            float wy = heightAt(wx, wz);
            Mesh wall = makeBox(6, 7, 2.5f, i % 2 ? rock : rock2);
            appendMesh(props, wall, Mat4::translate({wx, wy + 2.5f, wz}) * Mat4::rotY(a + 1.5708f), {1, 1, 1});
        }
        Mesh slab = makeBox(26, 2.2f, 20, rock2);
        appendMesh(props, slab, Mat4::translate({grottoCenter.x - dirToMain.x * 6, grottoFloorAt(grottoCenter.x, grottoCenter.z) + 5.2f, grottoCenter.z - dirToMain.z * 6}) * Mat4::rotY(ang) * Mat4::rotZ(0.05f), {1, 1, 1});
    }

    // Shrine on the seabed (torii gate, pillars, altar).
    {
        Vec3 sp = {-380, 0, 260};
        float sy = heightAt(sp.x, sp.z);
        Mesh p1 = makeBox(0.7f, 5, 0.7f, red), p2 = makeBox(0.7f, 5, 0.7f, red);
        appendMesh(props, p1, Mat4::translate({sp.x - 2.2f, sy + 2.5f, sp.z}), {1, 1, 1});
        appendMesh(props, p2, Mat4::translate({sp.x + 2.2f, sy + 2.5f, sp.z}), {1, 1, 1});
        appendMesh(props, makeBox(6.5f, 0.5f, 0.9f, red), Mat4::translate({sp.x, sy + 5.2f, sp.z}), {1, 1, 1});
        appendMesh(props, makeBox(5.2f, 0.4f, 0.8f, red), Mat4::translate({sp.x, sy + 4.1f, sp.z}), {1, 1, 1});
        for (int i = 0; i < 4; i++) {
            float a = i * 1.5708f + 0.785f;
            appendMesh(props, makeCylinder(0.35f, 2.2f, 6, stone),
                       Mat4::translate({sp.x + std::cos(a) * 5.5f, sy + 1.1f, sp.z + std::sin(a) * 5.5f}), {1, 1, 1});
        }
        appendMesh(props, makeBox(2.2f, 1.0f, 1.4f, stone), Mat4::translate({sp.x, sy + 0.5f, sp.z}), {1, 1, 1});
    }

    // Village: three stilt huts near the main island center.
    for (int i = 0; i < 3; i++) {
        float a = 0.6f + i * 2.1f;
        Vec3 hp = {villagePos.x + std::cos(a) * (35 + i * 12), 0, villagePos.z + std::sin(a) * (35 + i * 12)};
        float hy = heightAt(hp.x, hp.z);
        for (int s = 0; s < 4; s++) {
            float px = hp.x + (s % 2 ? 1.6f : -1.6f), pz = hp.z + (s / 2 ? 1.6f : -1.6f);
            appendMesh(props, makeCylinder(0.12f, 3.2f, 5, woodDark), Mat4::translate({px, hy + 1.6f, pz}), {1, 1, 1});
        }
        appendMesh(props, makeBox(4.2f, 2.6f, 4.2f, wood), Mat4::translate({hp.x, hy + 4.5f, hp.z}), {1, 1, 1});
        appendMesh(props, makeCone(3.4f, 2.2f, 4, {0.35f, 0.16f, 0.08f}), Mat4::translate({hp.x, hy + 6.9f, hp.z}) * Mat4::rotY(0.785f), {1, 1, 1});
    }

    // Lighthouse on its isle.
    {
        Vec3 lp = {-60, 0, -580};
        float ly = heightAt(lp.x, lp.z);
        for (int i = 0; i < 6; i++)
            appendMesh(props, makeCylinder(3.2f - i * 0.28f, 3.4f, 10, i % 2 ? red : white),
                       Mat4::translate({lp.x, ly + 1.7f + i * 3.4f, lp.z}), {1, 1, 1});
        appendMesh(props, makeCylinder(3.6f, 0.5f, 10, stone), Mat4::translate({lp.x, ly + 21.9f, lp.z}), {1, 1, 1});
        appendMesh(props, makeBox(2.2f, 2.0f, 2.2f, {0.9f, 0.85f, 0.6f}), Mat4::translate({lp.x, ly + 22.9f, lp.z}), {1, 1, 1});
        appendMesh(props, makeCone(2.0f, 1.4f, 8, rock2), Mat4::translate({lp.x, ly + 24.6f, lp.z}), {1, 1, 1});
        pois.push_back({POI::LIGHTHOUSE_BASE, {lp.x, ly, lp.z}, false});
        pois.push_back({POI::LIGHTHOUSE_TOP, {lp.x, ly + 23.6f, lp.z}, false});
    }

    // Dock planks.
    {
        appendMesh(props, makeBox(3, 0.3f, 7, wood), Mat4::translate({dockPos.x + ux * 2, dockPos.y + 0.15f, dockPos.z + uz * 2}) * Mat4::rotY(std::atan2(ux, uz)), {1, 1, 1});
        appendMesh(props, makeBox(0.25f, 2.5f, 0.25f, woodDark), Mat4::translate({dockPos.x + ux * 4.4f, dockPos.y - 1.0f, dockPos.z + uz * 4.4f}), {1, 1, 1});
        appendMesh(props, makeBox(0.25f, 2.5f, 0.25f, woodDark), Mat4::translate({dockPos.x + ux * 4.4f - 0.8f, dockPos.y - 1.0f, dockPos.z + uz * 4.4f - 0.8f}), {1, 1, 1});
    }

    // Drowned figures scattered on the seabed.
    Vec3 dpos[] = {{200, 0, -100}, {350, 0, -260}, {-200, 0, 300}, {-450, 0, 100}, {150, 0, 400}, {600, 0, 100}, {-100, 0, -420}};
    for (Vec3 d : dpos) drownedPos.push_back({d.x, heightAt(d.x, d.z), d.z});
    {
        Mesh body = makeCylinder(0.26f, 1.15f, 6, {0.72f, 0.76f, 0.74f});
        appendMesh(drownedMesh, body, Mat4::translate({0, 0.95f, 0}), {1, 1, 1});
        appendMesh(drownedMesh, makeBox(0.34f, 0.36f, 0.3f, {0.78f, 0.82f, 0.80f}), Mat4::translate({0, 1.72f, 0}), {1, 1, 1});
        appendMesh(drownedMesh, makeBox(0.12f, 0.9f, 0.12f, {0.68f, 0.72f, 0.70f}), Mat4::translate({0.34f, 1.0f, 0}) * Mat4::rotZ(-0.5f), {1, 1, 1});
        appendMesh(drownedMesh, makeBox(0.12f, 0.9f, 0.12f, {0.68f, 0.72f, 0.70f}), Mat4::translate({-0.34f, 1.0f, 0}) * Mat4::rotZ(0.5f), {1, 1, 1});
    }

    // Boat.
    {
        appendMesh(boatMesh, makeBox(4.6f, 0.7f, 1.8f, wood), Mat4::translate({0, 0.35f, 0}), {1, 1, 1});
        Mesh bow = makeCone(0.9f, 2.2f, 4, wood);
        appendMesh(boatMesh, bow, Mat4::translate({2.9f, 0.35f, 0}) * Mat4::rotZ(-1.5708f), {1, 1, 1});
        appendMesh(boatMesh, makeCylinder(0.07f, 5.5f, 5, woodDark), Mat4::translate({0.3f, 3.1f, 0}), {1, 1, 1});
        appendMesh(boatMesh, makeBox(0.06f, 3.4f, 2.4f, {0.85f, 0.82f, 0.70f}), Mat4::translate({0.32f, 3.4f, 0}), {1, 1, 1});
    }

    // Player character.
    {
        appendMesh(playerMesh, makeBox(0.5f, 0.75f, 0.3f, {0.22f, 0.27f, 0.38f}), Mat4::translate({0, 1.05f, 0}), {1, 1, 1});
        appendMesh(playerMesh, makeBox(0.4f, 0.35f, 0.32f, {0.80f, 0.64f, 0.50f}), Mat4::translate({0, 1.6f, 0}), {1, 1, 1});
        appendMesh(playerMesh, makeBox(0.55f, 0.12f, 0.4f, {0.15f, 0.15f, 0.18f}), Mat4::translate({0, 1.82f, 0}), {1, 1, 1});
    }

    pois.push_back({POI::WRECK, {wreckPos.x, heightAt(wreckPos.x, wreckPos.z) + 3.4f, wreckPos.z}, false});
    pois.push_back({POI::GROTTO, {grottoCenter.x, grottoFloorAt(grottoCenter.x, grottoCenter.z) + 0.9f, grottoCenter.z}, false});
    pois.push_back({POI::SHRINE, {-380, heightAt(-380, 260) + 1.6f, 260}, false});
    pois.push_back({POI::VILLAGE, {villagePos.x, heightAt(villagePos.x, villagePos.z) + 1.0f, villagePos.z}, false});
    pois.push_back({POI::DOCK, {dockPos.x, dockPos.y + 1.0f, dockPos.z}, false});
}
