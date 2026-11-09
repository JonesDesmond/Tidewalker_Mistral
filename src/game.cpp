#include "game.h"
#include "mesh.h"
#include "renderer.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

static const float PI = 3.14159265358979f;
static const World* g_worldPtr = nullptr;
static float heightCb(float x, float z) { return g_worldPtr->heightAt(x, z); }
static float waveCb(float x, float z, float t) { return World::wave(x, z, t); }

Game::Game() : tod(0.30f) {}

void Game::init() {
    world.build();
    g_worldPtr = &world;
    tod = opt.tod;
    boatPos = world.boatStart;
    ppos = {world.villagePos.x + 10, 0, world.villagePos.z + 6};
    ppos.y = world.heightAt(ppos.x, ppos.z);
    if (opt.playerOverride) {
        ppos.x = opt.playerX; ppos.z = opt.playerZ;
        ppos.y = std::max(world.heightAt(ppos.x, ppos.z), sea);
    }
    sea = World::tideLevel(time, opt.tidePeriod);
    say("LOW TIDE COMING - THE SEABED AWAITS", 5.0f);
}

void Game::say(const std::string& s, float dur) { msg = s; msgT = dur; }

void Game::setKey(int k, bool down) {
    if (k >= 0 && k < K_COUNT) keys[k] = down;
}

void Game::respawn() {
    ppos = {world.villagePos.x + 10, 0, world.villagePos.z + 6};
    ppos.y = world.heightAt(ppos.x, ppos.z);
    pvel = {};
    health = stamina = breath = 100;
    dead = false;
    aboard = false;
    boatPos = world.boatStart;
    boatYaw = 0;
    say("BACK AT THE STILT VILLAGE", 3.0f);
}

static float dist2D(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

void Game::update(float dt) {
    time += dt * opt.warp;
    tod = std::fmod(tod + dt * opt.warp / opt.dayLength, 1.0f);
    sea = World::tideLevel(time, opt.tidePeriod);

    float ang = tod * 2 * PI;
    sunDir = Vec3(std::cos(ang), std::sin(ang), 0.28f).norm();
    daylight = smoothstepf(-0.07f, 0.20f, sunDir.y);

    float p = std::fmod(time, opt.tidePeriod) / opt.tidePeriod;
    int idx = p < 0.125f ? 0 : (p < 0.5f ? 1 : (p < 0.625f ? 2 : 3));
    if (idx != lastPhaseIdx) {
        lastPhaseIdx = idx;
        std::string name; float left;
        World::tidePhase(time, opt.tidePeriod, name, left);
        say(name, 4.0f);
    }

    if (keys[K_LEFT]) camYaw -= 1.9f * dt;
    if (keys[K_RIGHT]) camYaw += 1.9f * dt;
    if (keys[K_UP]) camPitch = clampf(camPitch + 1.2f * dt, -1.0f, 0.35f);
    if (keys[K_DOWN]) camPitch = clampf(camPitch - 1.2f * dt, -1.0f, 0.35f);

    float pdt = dt * physScale;
    if (!dead) {
        if (aboard) {
            if (keys[K_A]) boatYaw += 1.7f * pdt;
            if (keys[K_D]) boatYaw -= 1.7f * pdt;
            float th = keys[K_W] ? 1.0f : (keys[K_S] ? -0.35f : 0.0f);
            float depthB = sea - world.heightAt(boatPos.x, boatPos.z);
            if (th != 0.0f) {
                if (depthB > 0.5f) {
                    Vec3 bf = {std::sin(boatYaw), 0, std::cos(boatYaw)};
                    boatPos += bf * (th * 9.0f * pdt);
                } else if (msgT <= 0) say("AGROUND - WAIT FOR THE FLOOD", 2.0f);
            }
            boatPos.x = clampf(boatPos.x, -World::HALF + 12, World::HALF - 12);
            boatPos.z = clampf(boatPos.z, -World::HALF + 12, World::HALF - 12);
            ppos = {boatPos.x, sea + 0.85f + 0.06f * std::sin(time * 2.2f), boatPos.z};
            pvel = {};
        } else {
            updatePlayer(pdt);
        }
        hazards(pdt);
    }

    if (keys[K_E] && !prevKeys[K_E]) onUseKey();
    if (keys[K_T] && !prevKeys[K_T]) {
        opt.warp = opt.warp > 1.0f ? 1.0f : 8.0f;
        say(opt.warp > 1.0f ? "TIME WARP x8" : "TIME WARP x1", 1.5f);
    }
    if (keys[K_R] && !prevKeys[K_R]) respawn();
    if (keys[K_ESC] && !prevKeys[K_ESC]) quitRequested = true;

    for (int i = 0; i < K_COUNT; i++) prevKeys[i] = keys[i];
    if (msgT > 0) msgT -= dt;
    frameNo++;
}

void Game::updatePlayer(float dt) {
    float ground = world.heightAt(ppos.x, ppos.z);
    float depth = sea - ground;
    Vec3 f = {std::sin(camYaw), 0, std::cos(camYaw)};
    Vec3 right = {-f.z, 0, f.x};
    Vec3 dir = {};
    if (keys[K_W]) dir += f;
    if (keys[K_S]) dir -= f;
    if (keys[K_D]) dir += right;
    if (keys[K_A]) dir -= right;
    bool moving = dir.len() > 0.01f;
    if (moving) dir = dir.norm();

    if (depth > 0.3f) {
        float swimSp = keys[K_SHIFT] ? 7.5f : 5.0f;
        if (keys[K_SHIFT]) stamina = std::max(0.0f, stamina - 18.0f * dt);
        ppos.x += dir.x * swimSp * dt;
        ppos.z += dir.z * swimSp * dt;
        float targetY = sea - 0.55f + 0.08f * std::sin(time * 2.0f);
        ppos.y = lerpf(ppos.y, targetY, 1.0f - std::exp(-6.0f * dt));
        pvel = {};
        if (moving) swimPhase += swimSp * dt * 1.5f;
    } else {
        stamina = std::min(100.0f, stamina + 14.0f * dt);
        float sp = keys[K_SHIFT] ? 9.0f : 7.0f;
        if (keys[K_SHIFT]) stamina = std::max(0.0f, stamina - 6.0f * dt);
        ppos.x += dir.x * sp * dt;
        ppos.z += dir.z * sp * dt;
        pvel.y -= 24.0f * dt;
        ppos.y += pvel.y * dt;
        if (ppos.y <= ground) {
            ppos.y = ground;
            pvel.y = 0;
            if (keys[K_SPACE]) pvel.y = 8.5f;
        }
        if (moving && ppos.y <= ground + 0.01f) walkPhase += sp * dt * 1.6f;
    }
    ppos.x = clampf(ppos.x, -World::HALF + 10, World::HALF - 10);
    ppos.z = clampf(ppos.z, -World::HALF + 10, World::HALF - 10);
    if (moving) pyaw = std::atan2(dir.x, dir.z);
}

void Game::hazards(float dt) {
    horror = false;
    bool submerged = sea > ppos.y + 1.15f;
    bool grottoFlood = world.inGrotto(ppos.x, ppos.z) &&
                       sea > world.grottoFloorAt(ppos.x, ppos.z) + 1.1f;
    if (submerged || grottoFlood) {
        breath = std::max(0.0f, breath - 34.0f * dt);
        if (grottoFlood) { horror = true; if (msgT <= 0) say("THE GROTTO IS FLOODING - GET OUT", 1.5f); }
        if (breath <= 0) {
            health -= 30.0f * dt;
            horror = true;
            if (msgT <= 0) say("YOU CANNOT BREATHE", 1.5f);
        }
    } else {
        breath = std::min(100.0f, breath + 45.0f * dt);
    }
    if (!aboard && sea < -0.5f) {
        for (const Vec3& dp : world.drownedPos) {
            if (sea < dp.y + 1.5f && dist2D(ppos, dp) < 3.0f) {
                health -= 9.0f * dt;
                horror = true;
                if (msgT <= 0) say("THE DROWNED REACH FOR YOU", 2.0f);
            }
        }
    }
    if (health <= 0 && !dead) { dead = true; health = 0; }
}

void Game::onUseKey() {
    if (dead) return;
    if (aboard) {
        aboard = false;
        Vec3 f = {std::sin(boatYaw), 0, std::cos(boatYaw)};
        Vec3 side = {-f.z, 0, f.x};
        ppos = boatPos + side * 1.6f;
        float g = world.heightAt(ppos.x, ppos.z);
        ppos.y = std::max(g, sea - 0.55f);
        say("BACK IN THE WATER", 2.0f);
        return;
    }
    float db = dist2D(ppos, boatPos);
    if (db < 5.0f) {
        float depthB = sea - world.heightAt(boatPos.x, boatPos.z);
        if (depthB > 0.3f) {
            aboard = true;
            say("ABOARD - W/S SAIL, A/D STEER, E DISEMBARK", 3.5f);
        } else {
            say("THE BOAT IS BEACHED - WAIT FOR THE FLOOD", 3.0f);
        }
        return;
    }
    float best = 4.5f;
    int bi = -1;
    for (size_t i = 0; i < world.pois.size(); i++) {
        const World::POI& poi = world.pois[i];
        float d = dist2D(ppos, poi.pos);
        if (d < best && std::fabs(ppos.y - poi.pos.y) < 3.5f) { best = d; bi = (int)i; }
    }
    if (bi < 0) return;
    World::POI& poi = world.pois[bi];
    switch (poi.type) {
        case World::POI::LIGHTHOUSE_BASE:
            ppos = world.pois[bi + 1].pos;
            ppos.y += 0.2f;
            say("YOU CLIMB THE LIGHTHOUSE", 2.5f);
            break;
        case World::POI::DOCK:
            break;
        default:
            if (!poi.taken) {
                poi.taken = true;
                relics++;
                const char* names[] = {"WRECK RELIC", "GROTTO RELIC", "SHRINE RELIC", "LIGHTHOUSE RELIC", "VILLAGE RELIC"};
                const char* nm = "RELIC";
                switch (poi.type) {
                    case World::POI::WRECK: nm = names[0]; break;
                    case World::POI::GROTTO: nm = names[1]; break;
                    case World::POI::SHRINE: nm = names[2]; break;
                    case World::POI::LIGHTHOUSE_TOP: nm = names[3]; break;
                    case World::POI::VILLAGE: nm = names[4]; break;
                    default: break;
                }
                char buf[96];
                std::snprintf(buf, sizeof buf, "%s RECOVERED (%d/5)", nm, relics);
                say(buf, 4.0f);
                if (relics >= 5) won = true;
            }
            break;
    }
}

// ---------- rendering ----------

void Game::render(Renderer& r) {
    Vec3 target, camPos;
    if (opt.camOverride) {
        camPos = {opt.camX, opt.camY, opt.camZ};
        float cp = std::cos(opt.camPitch), sp = std::sin(opt.camPitch);
        target = camPos + Vec3(std::sin(opt.camYaw) * cp, sp, std::cos(opt.camYaw) * cp) * 10.0f;
    } else {
        Vec3 f = {std::sin(camYaw), 0, std::cos(camYaw)};
        camPos = ppos - f * (std::cos(camPitch) * 9.0f);
        camPos.y = ppos.y + 3.2f - std::sin(camPitch) * 9.0f;
        target = ppos + Vec3(0, 1.6f, 0);
    }
    Mat4 proj = Mat4::perspective(60.0f * PI / 180.0f, (float)r.W / r.H, 0.5f, 4000.0f);
    Mat4 view = Mat4::lookAt(camPos, target, {0, 1, 0});
    Mat4 vp = proj * view;
    Mat4 ident = Mat4::ident();

    bool camUnder = camPos.y < sea;

    // sky colors
    Vec3 skyTopD = {0.30f, 0.50f, 0.75f}, skyHorD = {0.74f, 0.79f, 0.83f};
    Vec3 duskHor = {0.95f, 0.52f, 0.24f};
    Vec3 skyTopN = {0.015f, 0.03f, 0.07f}, skyHorN = {0.07f, 0.10f, 0.16f};
    float dusk = smoothstepf(0.25f, 0.02f, sunDir.y) * (1.0f - smoothstepf(-0.10f, -0.30f, sunDir.y));
    Vec3 skyTop = lerpf3(lerpf3(skyTopN, skyTopD, daylight), skyTopN * 0.6f, dusk * 0.3f);
    Vec3 skyHor = lerpf3(lerpf3(skyHorN, skyHorD, daylight), duskHor, dusk);
    float night = 1.0f - daylight;

    int sunX = -1, sunY = -1, moonX = -1, moonY = -1;
    {
        Vec4 s = xform4(vp, camPos + sunDir * 2000.0f);
        if (s.w > 0) { sunX = (int)((s.x / s.w * 0.5f + 0.5f) * r.W); sunY = (int)((1 - (s.y / s.w * 0.5f + 0.5f)) * r.H); }
        Vec4 m = xform4(vp, camPos - sunDir * 2000.0f);
        if (m.w > 0) { moonX = (int)((m.x / m.w * 0.5f + 0.5f) * r.W); moonY = (int)((1 - (m.y / m.w * 0.5f + 0.5f)) * r.H); }
    }
    r.skyPass(rgb(skyTop.x, skyTop.y, skyTop.z), rgb(skyHor.x, skyHor.y, skyHor.z),
              &world.texClouds, lerpf(0.12f, 0.7f, daylight),
              sunX, sunY, sunX >= 0 ? daylight : 0.0f, rgb(1.0f, 0.88f, 0.62f),
              moonX, moonY, 0xBCC8E8, night, time, dusk);
    r.resetDepth();

    // light environment
    r.L.sunDir = sunDir;
    r.L.sunI = daylight * 1.15f;
    r.L.sunCol = lerpf3({0.55f, 0.60f, 0.75f}, {1.0f, 0.93f, 0.80f}, daylight) + dusk * Vec3(0.4f, 0.1f, -0.1f);
    r.L.amb = camUnder ? 0.07f : 0.10f + 0.17f * daylight;
    r.L.fogCol = camUnder ? Vec3(0.02f, 0.11f, 0.15f) : skyHor;
    r.L.fogDens = camUnder ? 0.05f : lerpf(0.0042f, 0.0026f, daylight);
    r.L.lanPos = ppos + Vec3(0, 1.7f, 0);
    r.L.lanI = lerpf(2.6f, 0.5f, daylight);
    r.L.lanCol = {1.0f, 0.72f, 0.42f};
    r.L.camPos = camPos;
    r.L.sea = sea;
    r.L.waveT = time;
    (void)heightCb; (void)waveCb;

    r.beginOpaque();
    // terrain + details + props
    r.drawMesh(world.terrain, vp, ident, {1, 1, 1}, 1.0f);
    r.drawMesh(world.detailGrass, vp, ident, {1, 1, 1}, 1.0f);
    r.drawMesh(world.detailKelp, vp, ident, {1, 1, 1}, 1.0f);
    r.drawMesh(world.detailRock, vp, ident, {1, 1, 1}, 1.0f);
    r.drawMesh(world.propsWood, vp, ident, {1, 1, 1}, 1.0f);
    r.drawMesh(world.propsRock, vp, ident, {1, 1, 1}, 1.0f);
    r.drawMesh(world.propsStone, vp, ident, {1, 1, 1}, 1.0f);
    r.drawMesh(world.propsRed, vp, ident, {1, 1, 1}, 1.0f);
    float winGlow = lerpf(0.10f, 1.0f, night);
    r.drawMesh(world.propsEmissive, vp, ident, {winGlow, 0.72f * winGlow, 0.38f * winGlow}, 1.0f);

    // blob shadows
    auto shadow = [&](const Vec3& p, float rad) {
        float g = world.heightAt(p.x, p.z);
        float hAbove = std::max(0.0f, p.y - g);
        Vec3 sp = {p.x - sunDir.x * hAbove * 0.7f, g + 0.06f, p.z - sunDir.z * hAbove * 0.7f};
        Mat4 m = Mat4::translate(sp) * Mat4::scale({rad, 1, rad});
        r.drawMesh(world.blob, vp * m, m, {0, 0, 0}, 0.35f);
    };
    shadow(ppos, 0.9f);
    shadow(boatPos, 2.6f);
    for (const Vec3& dp : world.drownedPos) {
        float expose = clampf((dp.y + 2.2f - sea) / 1.0f, 0.0f, 1.0f);
        if (expose > 0.02f) shadow(dp, 0.8f);
    }

    // drowned (pale, faintly self-lit)
    for (const Vec3& dp : world.drownedPos) {
        float expose = clampf((dp.y + 2.2f - sea) / 1.0f, 0.0f, 1.0f);
        if (expose <= 0.02f) continue;
        float b = 0.35f + 0.65f * expose;
        Mat4 m = Mat4::translate(dp);
        r.drawMesh(world.drownedMesh, vp * m, m, {0.72f * b, 0.78f * b, 0.85f * b}, b);
    }

    r.beginOver();
    // relic markers (emissive gold, pulsing)
    {
        static Mesh relic = [] { Mesh m = makeCone(0.32f, 0.8f, 6, {1, 1, 1}); m.emissive = true; return m; }();
        float pulse = 0.75f + 0.25f * std::sin(time * 3.0f);
        for (const World::POI& poi : world.pois) {
            if (poi.taken || poi.type == World::POI::DOCK || poi.type == World::POI::LIGHTHOUSE_BASE) continue;
            Mat4 m = Mat4::translate({poi.pos.x, poi.pos.y + 0.5f + 0.15f * std::sin(time * 2.0f), poi.pos.z});
            r.drawMesh(relic, vp * m, m, {1.0f * pulse, 0.8f * pulse, 0.25f * pulse}, pulse);
        }
    }

    // boat
    {
        float bob = 0.06f * std::sin(time * 2.2f);
        Mat4 m = Mat4::translate({boatPos.x, sea + bob, boatPos.z}) * Mat4::rotY(boatYaw);
        r.drawMesh(world.boatMesh, vp * m, m, {1, 1, 1}, 1.0f);
    }

    // player (limbs swing while walking/swimming)
    {
        bool onLand = sea - world.heightAt(ppos.x, ppos.z) <= 0.3f;
        float sw = onLand ? std::sin(walkPhase) * 0.55f : std::sin(swimPhase) * 0.45f;
        Mat4 base = Mat4::translate(ppos) * Mat4::rotY(pyaw);
        Vec3 one = {1, 1, 1};
        Mat4 hip = Mat4::translate({0, 0.75f, 0});
        Mat4 sh = Mat4::translate({0, 1.42f, 0});
        Mat4 legL = base * hip * Mat4::rotX(sw) * Mat4::translate({0, -0.75f, 0});
        Mat4 legR = base * hip * Mat4::rotX(-sw) * Mat4::translate({0, -0.75f, 0});
        Mat4 armL = base * sh * Mat4::rotX(-sw * 0.8f) * Mat4::translate({0, -1.42f, 0});
        Mat4 armR = base * sh * Mat4::rotX(sw * 0.8f) * Mat4::translate({0, -1.42f, 0});
        r.drawMesh(world.playerLegL, vp * legL, legL, one, 1.0f);
        r.drawMesh(world.playerLegR, vp * legR, legR, one, 1.0f);
        r.drawMesh(world.playerArmL, vp * armL, armL, one, 1.0f);
        r.drawMesh(world.playerArmR, vp * armR, armR, one, 1.0f);
        r.drawMesh(world.playerTorso, vp * base, base, one, 1.0f);
        r.drawMesh(world.playerHead, vp * base, base, one, 1.0f);
    }
    r.endOpaque();

    // lighthouse beam at night (emissive translucent)
    if (daylight < 0.5f) {
        for (size_t i = 0; i < world.pois.size(); i++)
            if (world.pois[i].type == World::POI::LIGHTHOUSE_TOP) {
                Vec3 lp = world.pois[i].pos + Vec3(0, 0.5f, 0);
                float a = time * 0.7f;
                Vec3 dir = {std::sin(a), 0.06f, std::cos(a)};
                Mesh beam;
                beam.emissive = true;
                int v0 = beam.addVert(lp, {1, 1, 1}, {0, 1, 0}, {0, 0});
                int v1 = beam.addVert(lp + dir * 55 + Vec3(0, -1.5f, 0), {1, 1, 1}, {0, 1, 0}, {0, 0});
                int v2 = beam.addVert(lp + dir * 55 + Vec3(0, 1.5f, 0), {1, 1, 1}, {0, 1, 0}, {0, 0});
                int v3 = beam.addVert(lp + Vec3(-dir.z, 0, dir.x) * 2.5f, {1, 1, 1}, {0, 1, 0}, {0, 0});
                beam.addTri(v0, v1, v2, 0.20f * (1.0f - daylight));
                beam.addTri(v3, v1, v2, 0.12f * (1.0f - daylight));
                r.drawMesh(beam, vp, ident, {1.0f, 0.95f, 0.7f}, 1.0f);
            }
    }

    // water surface: per-vertex position, wave normal, depth/foam color
    for (size_t i = 0; i < world.waterMesh.pos.size(); i++) {
        Vec2 b = world.waterBase[i];
        float x = b.x, z = b.y;
        float y = sea + World::wave(x, z, time);
        world.waterMesh.pos[i] = {x, y, z};
        const float e = 0.6f;
        float dnx = -(World::wave(x + e, z, time) - World::wave(x, z, time)) / e;
        float dnz = -(World::wave(x, z + e, time) - World::wave(x, z, time)) / e;
        world.waterMesh.nrm[i] = Vec3(dnx, 1, dnz).norm();
        float h0 = world.heightAt(x, z);
        float depth = sea - h0;
        Vec3 deep = {0.015f, 0.075f, 0.115f}, shallow = {0.20f, 0.33f, 0.31f};
        Vec3 c = lerpf3(shallow, deep, smoothstepf(0.25f, 3.0f, depth));
        float foam = (1.0f - smoothstepf(0.03f, 0.5f, std::fabs(sea - h0))) * 0.85f;
        world.waterMesh.col[i] = lerpf3(c, {0.92f, 0.96f, 0.97f}, foam);
    }
    r.drawMesh(world.waterMesh, vp, ident, {1, 1, 1}, 1.0f);
    r.endOver();

    if (camUnder) r.rect(0, 0, r.W, r.H, rgb(0.02f, 0.10f, 0.14f), 0.35f);

    drawHUD(r);

    float nightV = lerpf(0.30f, 0.12f, daylight);
    vignetteAmt = nightV + (horror ? 0.18f + 0.08f * std::sin(time * 7.0f) : 0.0f);
    if (camUnder) vignetteAmt += 0.25f;
    if (dead) vignetteAmt += 0.3f;
}

void Game::drawHUD(Renderer& r) {
    std::string phaseName; float left;
    World::tidePhase(time, opt.tidePeriod, phaseName, left);
    int mm = (int)left / 60, ss = (int)left % 60;

    int gx = 26, gy = 96, gw = 16, gh = 150;
    r.rect(gx - 4, gy - 24, gw + 8 + 150, gh + 52, 0x000000, 0.38f);
    r.rect(gx, gy, gw, gh, 0x0A1420, 0.85f);
    float lf = clampf((sea - World::TIDE_LOW) / (World::TIDE_HIGH - World::TIDE_LOW), 0, 1);
    uint32_t lc = rgb(lerpf(0.15f, 0.45f, lf), lerpf(0.35f, 0.75f, lf), lerpf(0.55f, 1.0f, lf));
    r.rect(gx, gy + (int)(gh * (1 - lf)), gw, (int)(gh * lf) + 1, lc, 0.9f);
    r.line(gx - 3, gy + (int)(gh * (1 - lf)), gx + gw + 3, gy + (int)(gh * (1 - lf)), 0xFFFFFF);
    r.text(gx - 2, gy - 18, "HIGH", 1, 0xBFD8FF);
    r.text(gx - 2, gy + gh + 6, "LOW", 1, 0x7FA8CF);
    r.text(gx + gw + 8, gy + 4, phaseName.c_str(), 1, 0xEAF2FF);
    char tbuf[32];
    std::snprintf(tbuf, sizeof tbuf, "%02d:%02d", mm, ss);
    r.text(gx + gw + 8, gy + 18, tbuf, 1, 0x9FB8D8);
    const char* mode = aboard ? "SAILING" : (sea - world.heightAt(ppos.x, ppos.z) > 0.3f ? "SWIMMING" : "WALKING");
    r.text(gx + gw + 8, gy + 32, mode, 1, 0x9FB8D8);

    int bx = r.W - 210;
    r.rect(bx - 10, 14, 200, 92, 0x000000, 0.38f);
    char rbuf[32];
    std::snprintf(rbuf, sizeof rbuf, "RELICS %d/5", relics);
    r.text(bx, 22, rbuf, 2, won ? 0xFFD860 : 0xEAF2FF);
    auto bar = [&](int y, const char* label, float v, uint32_t c) {
        r.text(bx, y, label, 1, 0xBFD0E8);
        r.rect(bx, y + 10, 180, 8, 0x101820, 0.9f);
        r.rect(bx, y + 10, (int)(180 * clampf(v / 100.0f, 0, 1)), 8, c, 0.95f);
    };
    bar(40, "HEALTH", health, health > 30 ? 0xE05050 : 0xFF2020);
    bar(58, "STAMINA", stamina, 0x50C060);
    bar(76, "BREATH", breath, 0x40B8D8);

    {
        std::string prompt;
        if (!aboard && !dead) {
            float db = dist2D(ppos, boatPos);
            if (db < 5.0f) prompt = "E - BOARD THE BOAT";
            else {
                float best = 4.5f;
                int bi = -1;
                for (size_t i = 0; i < world.pois.size(); i++) {
                    const World::POI& poi = world.pois[i];
                    float d = dist2D(ppos, poi.pos);
                    if (d < best && std::fabs(ppos.y - poi.pos.y) < 3.5f) { best = d; bi = (int)i; }
                }
                if (bi >= 0) {
                    const World::POI& poi = world.pois[bi];
                    if (poi.type == World::POI::LIGHTHOUSE_BASE) prompt = "E - CLIMB THE LIGHTHOUSE";
                    else if (poi.type != World::POI::DOCK && !poi.taken) prompt = "E - TAKE THE RELIC";
                }
            }
        } else if (aboard && !dead) prompt = "E - DISEMBARK";
        if (!prompt.empty()) {
            int w = r.textW(prompt.c_str(), 2);
            r.text(r.W / 2 - w / 2, r.H / 2 + 60, prompt.c_str(), 2, 0xFFF2C0);
        }
    }

    if (msgT > 0 && !msg.empty()) {
        int w = r.textW(msg.c_str(), 2);
        r.rect(r.W / 2 - w / 2 - 10, r.H - 86, w + 20, 26, 0x000000, 0.45f);
        r.text(r.W / 2 - w / 2, r.H - 80, msg.c_str(), 2, 0xFFE9A8);
    }

    const char* ctl = "WASD MOVE  SPACE JUMP  SHIFT SPRINT  ARROWS CAM  E USE  T WARP  R RESPAWN  ESC QUIT";
    r.text(16, r.H - 22, ctl, 1, 0x8FA8C8, 0.9f);

    if (horror) {
        float a = 0.22f + 0.12f * std::sin(time * 7.0f);
        r.rect(0, 0, r.W, 10, 0x900000, a);
        r.rect(0, r.H - 10, r.W, 10, 0x900000, a);
        r.rect(0, 0, 10, r.H, 0x900000, a);
        r.rect(r.W - 10, 0, 10, r.H, 0x900000, a);
    }

    if (dead) {
        r.rect(0, 0, r.W, r.H, 0x300000, 0.55f);
        const char* t1 = "THE TIDE TOOK YOU";
        const char* t2 = "PRESS R TO RETURN TO THE VILLAGE";
        r.text(r.W / 2 - r.textW(t1, 4) / 2, r.H / 2 - 30, t1, 4, 0xFF5040);
        r.text(r.W / 2 - r.textW(t2, 2) / 2, r.H / 2 + 14, t2, 2, 0xFFB0A0);
    }
    if (won) {
        const char* t1 = "ALL FIVE RELICS RECOVERED";
        const char* t2 = "THE ARCHIPELAGO REMEMBERS YOU";
        r.text(r.W / 2 - r.textW(t1, 3) / 2, 60, t1, 3, 0xFFD860);
        r.text(r.W / 2 - r.textW(t2, 2) / 2, 92, t2, 2, 0xFFE9A8);
    }
}
