#include "game.h"
#include "mesh.h"
#include "renderer.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

static const float PI = 3.14159265358979f;

Game::Game() : tod(0.30f) {}

void Game::init() {
    world.build();
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

    // Tide phase bell + message.
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

    if (!dead) {
        if (aboard) {
            if (keys[K_A]) boatYaw += 1.7f * dt;
            if (keys[K_D]) boatYaw -= 1.7f * dt;
            float th = keys[K_W] ? 1.0f : (keys[K_S] ? -0.35f : 0.0f);
            float depthB = sea - world.heightAt(boatPos.x, boatPos.z);
            if (th != 0.0f) {
                if (depthB > 0.5f) {
                    Vec3 bf = {std::sin(boatYaw), 0, std::cos(boatYaw)};
                    boatPos += bf * (th * 9.0f * dt);
                } else if (msgT <= 0) say("AGROUND - WAIT FOR THE FLOOD", 2.0f);
            }
            boatPos.x = clampf(boatPos.x, -World::HALF + 12, World::HALF - 12);
            boatPos.z = clampf(boatPos.z, -World::HALF + 12, World::HALF - 12);
            ppos = {boatPos.x, sea + 0.85f + 0.06f * std::sin(time * 2.2f), boatPos.z};
            pvel = {};
        } else {
            updatePlayer(dt);
        }
        hazards(dt);
    }

    // Key rising edges.
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
    if (dir.len() > 0.01f) dir = dir.norm();

    if (depth > 0.3f) {
        // Swimming.
        float swimSp = keys[K_SHIFT] ? 7.5f : 5.0f;
        if (keys[K_SHIFT]) stamina = std::max(0.0f, stamina - 18.0f * dt);
        ppos.x += dir.x * swimSp * dt;
        ppos.z += dir.z * swimSp * dt;
        float targetY = sea - 0.55f + 0.08f * std::sin(time * 2.0f);
        ppos.y = lerpf(ppos.y, targetY, 1.0f - std::exp(-6.0f * dt));
        pvel = {};
    } else {
        // Walking.
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
    }
    ppos.x = clampf(ppos.x, -World::HALF + 10, World::HALF - 10);
    ppos.z = clampf(ppos.z, -World::HALF + 10, World::HALF - 10);
    if (dir.len() > 0.01f) pyaw = std::atan2(dir.x, dir.z);
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
    if (health <= 0 && !dead) {
        dead = true;
        health = 0;
    }
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
            ppos = world.pois[bi + 1].pos;  // top
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

static Vec3 lightVertex(const Vec3& base, const Vec3& n, const Vec3& p,
                        const Vec3& sunDir, float daylight, const Vec3& camPos,
                        float fogDens, const Vec3& fogCol, const Vec3& lanternPos, float lanternI) {
    float nd = std::max(0.0f, n.dot(sunDir));
    Vec3 c = base * (0.10f + 0.22f * daylight + daylight * 0.85f * nd);
    Vec3 ld = lanternPos - p;
    float d2 = ld.dot(ld);
    float att = lanternI / (1.0f + d2 * 0.055f);
    c += Vec3(1.0f, 0.72f, 0.42f) * (att * std::max(0.0f, n.dot(ld.norm())));
    float dist = (p - camPos).len();
    float f = 1.0f - std::exp(-(dist * fogDens) * (dist * fogDens));
    return lerpf3(c, fogCol, f);
}

void Game::render(Renderer& r) {
    // Camera.
    Vec3 target;
    Vec3 camPos;
    if (opt.camOverride) {
        camPos = {opt.camX, opt.camY, opt.camZ};
        float cp = std::cos(opt.camPitch), spt = std::sin(opt.camPitch);
        target = camPos + Vec3(std::sin(opt.camYaw) * cp, spt, std::cos(opt.camYaw) * cp) * 10.0f;
    } else {
        Vec3 f = {std::sin(camYaw), 0, std::cos(camYaw)};
        camPos = ppos - f * (std::cos(camPitch) * 9.0f);
        camPos.y = ppos.y + 3.2f - std::sin(camPitch) * 9.0f;
        target = ppos + Vec3(0, 1.6f, 0);
    }
    Mat4 proj = Mat4::perspective(60.0f * PI / 180.0f, (float)r.W / r.H, 0.5f, 4000.0f);
    Mat4 view = Mat4::lookAt(camPos, target, {0, 1, 0});
    Mat4 vp = proj * view;

    bool camUnder = camPos.y < sea;

    // Sky.
    Vec3 skyTopD = {0.30f, 0.50f, 0.75f}, skyHorD = {0.74f, 0.79f, 0.83f};
    Vec3 duskHor = {0.95f, 0.52f, 0.24f};
    Vec3 skyTopN = {0.015f, 0.03f, 0.07f}, skyHorN = {0.07f, 0.10f, 0.16f};
    float dusk = smoothstepf(0.25f, 0.02f, sunDir.y) * (1.0f - smoothstepf(-0.10f, -0.30f, sunDir.y));
    Vec3 skyTop = lerpf3(lerpf3(skyTopN, skyTopD, daylight), skyTopN * 0.6f, dusk * 0.3f);
    Vec3 skyHor = lerpf3(lerpf3(skyHorN, skyHorD, daylight), duskHor, dusk);
    r.vgradient(rgb(skyTop.x, skyTop.y, skyTop.z), rgb(skyHor.x, skyHor.y, skyHor.z));

    // Stars.
    if (daylight < 0.5f) {
        float sa = (0.5f - daylight) * 2.0f;
        for (int i = 0; i < 240; i++) {
            int x = hash2i(i * 7 + 1, 3) % r.W;
            int y = hash2i(i * 13 + 5, 11) % (r.H / 2);
            if (hash01(i, 99) > 0.55f) r.rect(x, y, hash2i(i, 7) % 3 ? 1 : 2, 1, 0xCCCCFF, sa * 0.9f);
        }
    }
    r.resetDepth();

    // Sun / moon disc.
    {
        Vec4 s = xform4(vp, camPos + sunDir * 2000.0f);
        if (s.w > 0) {
            float sx = (s.x / s.w * 0.5f + 0.5f) * r.W;
            float sy = (1.0f - (s.y / s.w * 0.5f + 0.5f)) * r.H;
            if (daylight > 0.02f) {
                int rad = (int)(r.H * 0.035f);
                r.circle((int)sx, (int)sy, rad * 3, rgb(1.0f, 0.85f, 0.5f) , 0.18f * daylight);
                r.circle((int)sx, (int)sy, rad, rgb(1.0f, 0.92f, 0.70f), 0.95f * daylight);
            }
        }
        if (daylight < 0.25f) {
            Vec4 m = xform4(vp, camPos - sunDir * 2000.0f);
            if (m.w > 0) {
                float mx = (m.x / m.w * 0.5f + 0.5f) * r.W;
                float my = (1.0f - (m.y / m.w * 0.5f + 0.5f)) * r.H;
                r.circle((int)mx, (int)my, (int)(r.H * 0.02f), rgb(0.72f, 0.78f, 0.88f), (0.25f - daylight) * 3.0f);
            }
        }
    }

    // Lighting environment.
    Vec3 fogCol = camUnder ? Vec3(0.02f, 0.11f, 0.15f) : skyHor;
    float fogDens = camUnder ? 0.05f : lerpf(0.0042f, 0.0026f, daylight);
    Vec3 lanternPos = ppos + Vec3(0, 1.7f, 0);
    float lanternI = lerpf(1.7f, 0.5f, daylight);

    auto litMesh = [&](const Mesh& m, const Mat4& model) {
        std::vector<Vec3> lit(m.pos.size());
        for (size_t i = 0; i < m.pos.size(); i++) {
            Vec4 wp = xform4(model, m.pos[i]);
            lit[i] = lightVertex(m.col[i], m.nrm[i], {wp.x / wp.w, wp.y / wp.w, wp.z / wp.w},
                                 sunDir, daylight, camPos, fogDens, fogCol, lanternPos, lanternI);
        }
        return lit;
    };

    // Terrain.
    {
        auto lit = litMesh(world.terrain, Mat4::ident());
        r.drawMesh(world.terrain, vp, lit.data(), 1.0f, false);
    }
    // Static props.
    {
        auto lit = litMesh(world.props, Mat4::ident());
        r.drawMesh(world.props, vp, lit.data(), 1.0f, false);
    }
    // Drowned (visible only when the tide is out).
    for (const Vec3& dp : world.drownedPos) {
        float expose = clampf((dp.y + 1.7f - sea) / 1.2f, 0.0f, 1.0f);
        if (expose <= 0.02f) continue;
        auto lit = litMesh(world.drownedMesh, Mat4::translate(dp));
        r.drawMesh(world.drownedMesh, vp * Mat4::translate(dp), lit.data(), expose, false);
    }
    // Relic markers (gold, pulsing).
    {
        Mesh relic = makeCone(0.32f, 0.8f, 6, {1.0f, 0.8f, 0.25f});
        float pulse = 0.75f + 0.25f * std::sin(time * 3.0f);
        for (const World::POI& poi : world.pois) {
            if (poi.taken || poi.type == World::POI::DOCK || poi.type == World::POI::LIGHTHOUSE_BASE) continue;
            std::vector<Vec3> lit(relic.pos.size(), Vec3(1.0f, 0.8f * pulse, 0.3f));
            Mat4 m = Mat4::translate({poi.pos.x, poi.pos.y + 0.5f + 0.15f * std::sin(time * 2.0f), poi.pos.z});
            r.drawMesh(relic, vp * m, lit.data(), pulse, true);
        }
    }
    // Boat.
    {
        float bob = 0.06f * std::sin(time * 2.2f);
        Mat4 m = Mat4::translate({boatPos.x, sea + bob, boatPos.z}) * Mat4::rotY(boatYaw);
        auto lit = litMesh(world.boatMesh, m);
        r.drawMesh(world.boatMesh, vp * m, lit.data(), 1.0f, false);
    }
    // Player.
    {
        Mat4 m = Mat4::translate(ppos) * Mat4::rotY(pyaw);
        auto lit = litMesh(world.playerMesh, m);
        r.drawMesh(world.playerMesh, vp * m, lit.data(), 1.0f, false);
    }
    // Lighthouse beam at night.
    if (daylight < 0.35f) {
        for (size_t i = 0; i < world.pois.size(); i++)
            if (world.pois[i].type == World::POI::LIGHTHOUSE_TOP) {
                Vec3 lp = world.pois[i].pos + Vec3(0, 0.5f, 0);
                float a = time * 0.7f;
                Vec3 dir = {std::sin(a), 0.06f, std::cos(a)};
                Mesh beam;
                int v0 = beam.addVert(lp, {1, 1, 1}, {0, 1, 0});
                int v1 = beam.addVert(lp + dir * 55 + Vec3(0, -1.5f, 0), {1, 1, 1}, {0, 1, 0});
                int v2 = beam.addVert(lp + dir * 55 + Vec3(0, 1.5f, 0), {1, 1, 1}, {0, 1, 0});
                int v3 = beam.addVert(lp + Vec3(-dir.z, 0, dir.x) * 1.5f, {1, 1, 1}, {0, 1, 0});
                int v4 = beam.addVert(lp - Vec3(-dir.z, 0, dir.x) * 1.5f, {1, 1, 1}, {0, 1, 0});
                beam.addTri(v0, v1, v2, 0.10f * (1.0f - daylight));
                beam.addTri(v3, v1, v2, 0.06f * (1.0f - daylight));
                std::vector<Vec3> lit(beam.pos.size(), Vec3(1.0f, 0.95f, 0.7f));
                r.drawMesh(beam, vp, lit.data(), 1.0f, true);
            }
    }
    // Water surface.
    {
        for (size_t i = 0; i < world.waterMesh.pos.size(); i++) {
            Vec2 b = world.waterBase[i];
            float y = sea + World::wave(b.x, b.y, time);
            world.waterMesh.pos[i] = {b.x, y, b.y};
            float g = world.heightAt(b.x, b.y);
            float depth = sea - g;
            Vec3 shallow = {0.22f, 0.34f, 0.33f}, deep = {0.02f, 0.08f, 0.13f};
            Vec3 c = lerpf3(shallow, deep, smoothstepf(0.3f, 3.0f, depth));
            c = lerpf3(c, skyHor * 0.8f, 0.15f);
            float foam = 1.0f - smoothstepf(0.04f, 0.38f, std::fabs(sea - g));
            c = lerpf3(c, {0.92f, 0.96f, 0.96f}, foam * 0.85f);
            c = c * (0.22f + 0.78f * daylight);
            world.waterMesh.col[i] = c;
        }
        std::vector<Vec3> lit = world.waterMesh.col;
        r.drawMesh(world.waterMesh, vp, lit.data(), 0.82f, true);
    }
    // Underwater overlay.
    if (camUnder) r.rect(0, 0, r.W, r.H, rgb(0.02f, 0.10f, 0.14f), 0.35f);

    drawHUD(r);

    // Vignette amount for present().
    float nightV = lerpf(0.30f, 0.12f, daylight);
    vignetteAmt = nightV + (horror ? 0.18f + 0.08f * std::sin(time * 7.0f) : 0.0f);
    if (camUnder) vignetteAmt += 0.25f;
    if (dead) vignetteAmt += 0.3f;
}

void Game::drawHUD(Renderer& r) {
    std::string phaseName; float left;
    World::tidePhase(time, opt.tidePeriod, phaseName, left);
    int mm = (int)left / 60, ss = (int)left % 60;

    // Tide gauge (left).
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

    // Top-right: relics + bars.
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

    // Interact prompt (center).
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

    // Message line.
    if (msgT > 0 && !msg.empty()) {
        int w = r.textW(msg.c_str(), 2);
        r.rect(r.W / 2 - w / 2 - 10, r.H - 86, w + 20, 26, 0x000000, 0.45f);
        r.text(r.W / 2 - w / 2, r.H - 80, msg.c_str(), 2, 0xFFE9A8);
    }

    // Controls.
    const char* ctl = "WASD MOVE  SPACE JUMP  SHIFT SPRINT  ARROWS CAM  E USE  T WARP  R RESPAWN  ESC QUIT";
    r.text(16, r.H - 22, ctl, 1, 0x8FA8C8, 0.9f);

    // Horror vignette edges.
    if (horror) {
        float a = 0.22f + 0.12f * std::sin(time * 7.0f);
        r.rect(0, 0, r.W, 10, 0x900000, a);
        r.rect(0, r.H - 10, r.W, 10, 0x900000, a);
        r.rect(0, 0, 10, r.H, 0x900000, a);
        r.rect(r.W - 10, 0, 10, r.H, 0x900000, a);
    }

    // Death / victory overlays.
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
