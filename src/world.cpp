#include "world.h"
#include "mesh.h"
#include "texgen.h"
#include <cmath>
#include <algorithm>

struct Island { float x, z, r, peak; };
static const Island ISLANDS[] = {
    {0, 0, 280, 15},
    {560, -300, 190, 11},
    {-600, -200, 160, 13},
    {-280, 540, 180, 10},
    {320, 580, 150, 9},
    {-60, -580, 130, 8},
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

void World::buildTextures() {
    texSand = makeTexSand();
    texGrass = makeTexGrass();
    texRock = makeTexRock();
    texWood = makeTexWood();
    texStone = makeTexStone();
    texLeaf = makeTexLeaf();
    texKelp = makeTexKelp();
    texClouds = makeTexClouds();
}

void World::build() {
    buildTextures();

    // ---------------- heightmap ----------------
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

    // ---------------- terrain mesh (normals, AO, splat weights, uv) ----------------
    const int TN = 100;
    const float TILE = 14.0f;
    for (int j = 0; j <= TN; j++)
        for (int i = 0; i <= TN; i++) {
            float x = -HALF + (float)i / TN * SIZE;
            float z = -HALF + (float)j / TN * SIZE;
            float h = heightAt(x, z);
            float hx = heightAt(x + 3, z) - heightAt(x - 3, z);
            float hz = heightAt(x, z + 3) - heightAt(x, z - 3);
            Vec3 n = Vec3(-hx / 6.0f, 1, -hz / 6.0f).norm();
            float slope = std::sqrt(hx * hx + hz * hz) / 6.0f;
            // ambient occlusion from concavity
            float avg = (heightAt(x + 7, z) + heightAt(x - 7, z) + heightAt(x, z + 7) + heightAt(x, z - 7)) / 4;
            float ao = clampf(1.0f - (avg - h) * 0.28f, 0.45f, 1.0f);
            float var = 0.9f + fbm(x * 0.05f, z * 0.05f, 3) * 0.2f;
            // splat weights: sand / grass / rock
            float wSand = 1.0f - smoothstepf(TIDE_LOW + 0.8f, TIDE_LOW + 2.2f, h);
            float wRock = smoothstepf(0.45f, 0.8f, slope) + smoothstepf(9.0f, 14.0f, h) * 0.7f;
            float wGrass = clampf(1.0f - wSand - wRock, 0, 1);
            float s = wSand + wGrass + wRock;
            terrain.addVert({x, h, z}, {ao * var, ao * var, ao * var}, n, {x / TILE, z / TILE});
            terrain.splat.back() = {wSand / s, wGrass / s, wRock / s};
        }
    for (int j = 0; j < TN; j++)
        for (int i = 0; i < TN; i++) {
            int a = j * (TN + 1) + i, b = a + 1, c = a + TN + 1, d = c + 1;
            terrain.addTri(a, c, b); terrain.addTri(b, c, d);
        }
    terrain.tex = &texSand; terrain.tex2 = &texGrass; terrain.tex3 = &texRock;

    // ---------------- water ----------------
    for (int j = 0; j <= WN; j++)
        for (int i = 0; i <= WN; i++)
            waterBase.push_back({-HALF + (float)i / WN * SIZE, -HALF + (float)j / WN * SIZE});
    for (int j = 0; j <= WN; j++)
        for (int i = 0; i <= WN; i++)
            waterMesh.addVert({waterBase[j * (WN + 1) + i].x, 0, waterBase[j * (WN + 1) + i].y},
                              {1, 1, 1}, {0, 1, 0}, {0, 0});
    for (int j = 0; j < WN; j++)
        for (int i = 0; i < WN; i++) {
            int a = j * (WN + 1) + i, b = a + 1, c = a + WN + 1, d = c + 1;
            waterMesh.addTri(a, c, b, 0.85f); waterMesh.addTri(b, c, d, 0.85f);
        }
    waterMesh.water = true;

    // ---------------- dock / boat placement ----------------
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

    Vec3 rock = {0.9f, 0.88f, 0.85f}, wood = {0.95f, 0.9f, 0.85f};
    Vec3 red = {0.9f, 0.25f, 0.2f}, white = {0.95f, 0.93f, 0.88f};
    Vec3 stone = {0.9f, 0.89f, 0.85f};
    Vec3 woodDarkTint = {0.55f, 0.45f, 0.4f};

    // ---------------- wreck (wood) ----------------
    {
        float wy = heightAt(wreckPos.x, wreckPos.z);
        appendMesh(propsWood, makeBox(16, 3.2f, 5.5f, wood), Mat4::translate({wreckPos.x, wy + 2.2f, wreckPos.z}) * Mat4::rotY(0.5f) * Mat4::rotZ(0.06f), {1, 1, 1});
        appendMesh(propsWood, makeBox(4, 2.4f, 4.5f, wood), Mat4::translate({wreckPos.x + 8.5f, wy + 2.6f, wreckPos.z - 1.5f}) * Mat4::rotY(0.5f) * Mat4::rotZ(-0.12f), {1, 1, 1});
        appendMesh(propsWood, makeCylinder(0.25f, 11, 6, wood), Mat4::translate({wreckPos.x - 2, wy + 6.5f, wreckPos.z + 0.5f}) * Mat4::rotZ(0.35f), {1, 1, 1});
        appendMesh(propsWood, makeBox(3.4f, 0.2f, 2.6f, wood), Mat4::translate({wreckPos.x - 2, wy + 11.4f, wreckPos.z + 3.4f}) * Mat4::rotZ(0.35f) * Mat4::rotY(0.3f), {1, 1, 1});
        for (int i = 0; i < 5; i++)
            appendMesh(propsWood, makeBox(0.4f, 3.5f, 0.4f, wood),
                       Mat4::translate({wreckPos.x - 5 + i * 2.6f, wy + 3.4f, wreckPos.z + 2.2f}) * Mat4::rotX(0.5f), woodDarkTint);
        // broken bow ribs
        for (int i = 0; i < 3; i++)
            appendMesh(propsWood, makeBox(0.35f, 2.6f, 0.35f, wood),
                       Mat4::translate({wreckPos.x + 6 + i * 1.8f, wy + 2.8f, wreckPos.z - 2.4f}) * Mat4::rotZ(-0.6f) * Mat4::rotX(0.3f), woodDarkTint);
    }

    // ---------------- grotto (rock) ----------------
    {
        Vec3 dirToMain = Vec3(-grottoCenter.x, 0, -grottoCenter.z).norm();
        float ang = std::atan2(dirToMain.x, dirToMain.z);
        for (int i = 0; i < 10; i++) {
            if (std::fabs(i - 4.5f) < 1.2f) continue;
            float a = ang + (i - 4.5f) * 0.42f;
            float wx = grottoCenter.x + std::sin(a) * (grottoRadius + 2);
            float wz = grottoCenter.z + std::cos(a) * (grottoRadius + 2);
            float wy = heightAt(wx, wz);
            appendMesh(propsRock, makeRock(4.5f, i * 7 + 1, rock), Mat4::translate({wx, wy + 2.5f, wz}) * Mat4::rotY(a), {1, 1, 1});
        }
        appendMesh(propsRock, makeBox(26, 2.2f, 20, rock),
                   Mat4::translate({grottoCenter.x - dirToMain.x * 6, grottoFloorAt(grottoCenter.x, grottoCenter.z) + 5.2f, grottoCenter.z - dirToMain.z * 6}) * Mat4::rotY(ang) * Mat4::rotZ(0.05f), {0.9f, 0.9f, 0.9f});
        // extra boulders inside
        for (int i = 0; i < 4; i++)
            appendMesh(propsRock, makeRock(1.2f + hash01(i, 3) * 1.2f, i * 13 + 5, rock),
                       Mat4::translate({grottoCenter.x + (hash01(i, 9) - 0.5f) * 24, grottoFloorAt(grottoCenter.x + (hash01(i, 9) - 0.5f) * 24, grottoCenter.z + (hash01(i, 4) - 0.5f) * 24) + 0.4f, grottoCenter.z + (hash01(i, 4) - 0.5f) * 24}), {0.85f, 0.85f, 0.85f});
    }

    // ---------------- shrine (stone + red torii) ----------------
    {
        Vec3 sp = {-380, 0, 260};
        float sy = heightAt(sp.x, sp.z);
        appendMesh(propsRed, makeBox(0.7f, 5, 0.7f, red), Mat4::translate({sp.x - 2.2f, sy + 2.5f, sp.z}), {1, 1, 1});
        appendMesh(propsRed, makeBox(0.7f, 5, 0.7f, red), Mat4::translate({sp.x + 2.2f, sy + 2.5f, sp.z}), {1, 1, 1});
        appendMesh(propsRed, makeBox(6.5f, 0.5f, 0.9f, red), Mat4::translate({sp.x, sy + 5.2f, sp.z}), {1, 1, 1});
        appendMesh(propsRed, makeBox(5.2f, 0.4f, 0.8f, red), Mat4::translate({sp.x, sy + 4.1f, sp.z}), {1, 1, 1});
        for (int i = 0; i < 4; i++) {
            float a = i * 1.5708f + 0.785f;
            appendMesh(propsStone, makeCylinder(0.35f, 2.2f, 6, stone),
                       Mat4::translate({sp.x + std::cos(a) * 5.5f, sy + 1.1f, sp.z + std::sin(a) * 5.5f}), {1, 1, 1});
        }
        appendMesh(propsStone, makeBox(2.2f, 1.0f, 1.4f, stone), Mat4::translate({sp.x, sy + 0.5f, sp.z}), {1, 1, 1});
    }

    // ---------------- village huts (wood + emissive windows) ----------------
    for (int i = 0; i < 3; i++) {
        float a = 0.6f + i * 2.1f;
        Vec3 hp = {villagePos.x + std::cos(a) * (35 + i * 12), 0, villagePos.z + std::sin(a) * (35 + i * 12)};
        float hy = heightAt(hp.x, hp.z);
        for (int s = 0; s < 4; s++) {
            float px = hp.x + (s % 2 ? 1.6f : -1.6f), pz = hp.z + (s / 2 ? 1.6f : -1.6f);
            appendMesh(propsWood, makeCylinder(0.12f, 3.2f, 5, wood), Mat4::translate({px, hy + 1.6f, pz}), woodDarkTint);
        }
        appendMesh(propsWood, makeBox(4.2f, 2.6f, 4.2f, wood), Mat4::translate({hp.x, hy + 4.5f, hp.z}), {1, 1, 1});
        appendMesh(propsWood, makeCone(3.4f, 2.2f, 4, wood), Mat4::translate({hp.x, hy + 6.9f, hp.z}) * Mat4::rotY(0.785f), {0.6f, 0.35f, 0.3f});
        // glowing window on the front face
        appendMesh(propsEmissive, makeBox(0.7f, 0.8f, 0.1f, {1, 1, 1}),
                   Mat4::translate({hp.x, hy + 4.6f, hp.z + 2.12f}), {1, 1, 1});
    }

    // ---------------- lighthouse (stone + red + emissive lamp) ----------------
    {
        Vec3 lp = {-60, 0, -580};
        float ly = heightAt(lp.x, lp.z);
        for (int i = 0; i < 6; i++)
            appendMesh(i % 2 ? propsRed : propsStone, makeCylinder(3.2f - i * 0.28f, 3.4f, 10, white),
                       Mat4::translate({lp.x, ly + 1.7f + i * 3.4f, lp.z}), {1, 1, 1});
        appendMesh(propsStone, makeCylinder(3.6f, 0.5f, 10, stone), Mat4::translate({lp.x, ly + 21.9f, lp.z}), {1, 1, 1});
        appendMesh(propsEmissive, makeBox(2.2f, 2.0f, 2.2f, {1, 1, 1}), Mat4::translate({lp.x, ly + 22.9f, lp.z}), {1, 1, 1});
        appendMesh(propsRock, makeCone(2.0f, 1.4f, 8, rock), Mat4::translate({lp.x, ly + 24.6f, lp.z}), {0.4f, 0.4f, 0.42f});
        pois.push_back({POI::LIGHTHOUSE_BASE, {lp.x, ly, lp.z}, false});
        pois.push_back({POI::LIGHTHOUSE_TOP, {lp.x, ly + 23.6f, lp.z}, false});
    }

    // ---------------- dock (wood) ----------------
    {
        appendMesh(propsWood, makeBox(3, 0.3f, 7, wood), Mat4::translate({dockPos.x + ux * 2, dockPos.y + 0.15f, dockPos.z + uz * 2}) * Mat4::rotY(std::atan2(ux, uz)), {1, 1, 1});
        appendMesh(propsWood, makeBox(0.25f, 2.5f, 0.25f, wood), Mat4::translate({dockPos.x + ux * 4.4f, dockPos.y - 1.0f, dockPos.z + uz * 4.4f}), woodDarkTint);
        appendMesh(propsWood, makeBox(0.25f, 2.5f, 0.25f, wood), Mat4::translate({dockPos.x + ux * 4.4f - 0.8f, dockPos.y - 1.0f, dockPos.z + uz * 4.4f - 0.8f}), woodDarkTint);
    }

    // ---------------- scattered details ----------------
    detailGrass.tex = &texLeaf;
    detailRock.tex = &texRock;
    detailKelp.tex = &texKelp;
    Mesh tuft = makeTuft(0.9f, {1, 1, 1});
    tuft.tex = &texLeaf;
    Mesh kelp = makeTuft(2.2f, {1, 1, 1});
    kelp.tex = &texKelp;
    for (int i = 0; i < 1800; i++) {
        float x = (hash01(i, 101) - 0.5f) * 2 * HALF;
        float z = (hash01(i, 202) - 0.5f) * 2 * HALF;
        float h = heightAt(x, z);
        float vd = std::sqrt(x * x + z * z);
        if (inGrotto(x, z)) continue;
        float yaw = hash01(i, 303) * 6.28f;
        float sc = 0.7f + hash01(i, 404) * 0.8f;
        if (h > TIDE_HIGH + 0.4f && h < 13.0f && vd > 95 && fbm(x * 0.02f, z * 0.02f, 2) > 0.42f) {
            appendMesh(detailGrass, tuft, Mat4::translate({x, h, z}) * Mat4::rotY(yaw) * Mat4::scale({sc, sc, sc}), {1, 1, 1});
        } else if (h < TIDE_LOW - 0.3f && h > -7.5f && fbm(x * 0.03f + 9, z * 0.03f, 3) > 0.55f) {
            appendMesh(detailKelp, kelp, Mat4::translate({x, h, z}) * Mat4::rotY(yaw) * Mat4::scale({sc, sc * 1.4f, sc}), {0.9f, 1.0f, 0.9f});
        } else if (h > TIDE_LOW - 0.5f && h < TIDE_HIGH + 2.5f && hash01(i, 505) > 0.6f) {
            appendMesh(detailRock, makeRock(0.5f + hash01(i, 606) * 0.9f, i, {0.85f, 0.83f, 0.8f}),
                       Mat4::translate({x, h + 0.1f, z}) * Mat4::rotY(yaw) * Mat4::scale({sc, sc * 0.7f, sc}), {1, 1, 1});
        }
    }
    // rocks on island slopes
    for (int i = 0; i < 300; i++) {
        float x = (hash01(i, 707) - 0.5f) * 2 * HALF;
        float z = (hash01(i, 808) - 0.5f) * 2 * HALF;
        float h = heightAt(x, z);
        float hx = heightAt(x + 3, z) - heightAt(x - 3, z);
        float hz = heightAt(x, z + 3) - heightAt(x, z - 3);
        float slope = std::sqrt(hx * hx + hz * hz) / 6.0f;
        if (h > TIDE_HIGH && slope > 0.5f) {
            float s = 0.8f + hash01(i, 909) * 2.2f;
            appendMesh(detailRock, makeRock(s, i * 3 + 11, {0.8f, 0.78f, 0.75f}),
                       Mat4::translate({x, h, z}) * Mat4::rotY(hash01(i, 111) * 6.28f), {1, 1, 1});
        }
    }

    // ---------------- drowned (pale, cloaked, arms out) ----------------
    {
        Vec3 pale = {0.8f, 0.84f, 0.86f};
        appendMesh(drownedMesh, makeCylinder(0.26f, 1.15f, 6, pale), Mat4::translate({0, 0.95f, 0}), {1, 1, 1});
        appendMesh(drownedMesh, makeCone(0.45f, 1.3f, 6, pale), Mat4::translate({0, 0.7f, 0}), {0.75f, 0.8f, 0.85f});
        appendMesh(drownedMesh, makeBox(0.34f, 0.36f, 0.3f, pale), Mat4::translate({0, 1.72f, 0}), {1.05f, 1.05f, 1.05f});
        appendMesh(drownedMesh, makeBox(0.12f, 0.95f, 0.12f, pale), Mat4::translate({0.4f, 1.15f, 0.1f}) * Mat4::rotZ(-0.9f), {1, 1, 1});
        appendMesh(drownedMesh, makeBox(0.12f, 0.95f, 0.12f, pale), Mat4::translate({-0.4f, 1.15f, 0.1f}) * Mat4::rotZ(0.9f), {1, 1, 1});
    }
    Vec3 dpos[] = {{200, 0, -100}, {350, 0, -260}, {-200, 0, 300}, {-450, 0, 100}, {150, 0, 400}, {600, 0, 100}, {-100, 0, -420}};
    for (Vec3 d : dpos) drownedPos.push_back({d.x, heightAt(d.x, d.z), d.z});

    // ---------------- boat (wood) ----------------
    {
        appendMesh(boatMesh, makeBox(4.6f, 0.7f, 1.8f, wood), Mat4::translate({0, 0.35f, 0}), {1, 1, 1});
        appendMesh(boatMesh, makeCone(0.9f, 2.2f, 4, wood), Mat4::translate({2.9f, 0.35f, 0}) * Mat4::rotZ(-1.5708f), {1, 1, 1});
        appendMesh(boatMesh, makeCylinder(0.07f, 5.5f, 5, wood), Mat4::translate({0.3f, 3.1f, 0}), woodDarkTint);
        appendMesh(boatMesh, makeBox(0.06f, 3.4f, 2.4f, {0.92f, 0.88f, 0.75f}), Mat4::translate({0.32f, 3.4f, 0}), {1, 1, 1});
    }
    boatMesh.tex = &texWood;

    // ---------------- player parts ----------------
    Vec3 coat = {0.25f, 0.32f, 0.45f}, coatDark = {0.18f, 0.23f, 0.33f}, skin = {0.82f, 0.66f, 0.52f};
    appendMesh(playerLegL, makeBox(0.18f, 0.75f, 0.2f, coatDark), Mat4::translate({-0.12f, -0.375f, 0}), {1, 1, 1});
    appendMesh(playerLegR, makeBox(0.18f, 0.75f, 0.2f, coatDark), Mat4::translate({0.12f, -0.375f, 0}), {1, 1, 1});
    appendMesh(playerArmL, makeBox(0.13f, 0.65f, 0.15f, coat), Mat4::translate({-0.32f, -0.35f, 0}), {1, 1, 1});
    appendMesh(playerArmR, makeBox(0.13f, 0.65f, 0.15f, coat), Mat4::translate({0.32f, -0.35f, 0}), {1, 1, 1});
    appendMesh(playerTorso, makeBox(0.5f, 0.75f, 0.3f, coat), Mat4::translate({0, 1.05f, 0}), {1, 1, 1});
    appendMesh(playerTorso, makeBox(0.52f, 0.2f, 0.32f, coatDark), Mat4::translate({0, 0.85f, 0}), {1, 1, 1});  // belt
    appendMesh(playerHead, makeBox(0.4f, 0.35f, 0.32f, skin), Mat4::translate({0, 1.62f, 0}), {1, 1, 1});
    appendMesh(playerHead, makeBox(0.55f, 0.1f, 0.42f, {0.12f, 0.12f, 0.15f}), Mat4::translate({0, 1.83f, 0}), {1, 1, 1});  // hat brim
    appendMesh(playerHead, makeBox(0.42f, 0.16f, 0.36f, {0.12f, 0.12f, 0.15f}), Mat4::translate({0, 1.9f, 0}), {1, 1, 1});  // hat top
    // lantern in right hand
    appendMesh(playerArmR, makeBox(0.12f, 0.14f, 0.12f, {1, 0.8f, 0.4f}), Mat4::translate({0.32f, -0.72f, 0}), {1, 1, 1});

    blob = makeBlob(1.0f);

    // ---------------- POIs ----------------
    pois.push_back({POI::WRECK, {wreckPos.x, heightAt(wreckPos.x, wreckPos.z) + 3.4f, wreckPos.z}, false});
    pois.push_back({POI::GROTTO, {grottoCenter.x, grottoFloorAt(grottoCenter.x, grottoCenter.z) + 0.9f, grottoCenter.z}, false});
    pois.push_back({POI::SHRINE, {-380, heightAt(-380, 260) + 1.6f, 260}, false});
    pois.push_back({POI::VILLAGE, {villagePos.x, heightAt(villagePos.x, villagePos.z) + 1.0f, villagePos.z}, false});
    pois.push_back({POI::DOCK, {dockPos.x, dockPos.y + 1.0f, dockPos.z}, false});

    propsWood.tex = &texWood;
    propsRock.tex = &texRock;
    propsStone.tex = &texStone;
    propsRed.tex = &texWood;
    propsEmissive.emissive = true;
}
