#include "game.h"
#include "renderer.h"
#include "font.h"
#include "png.h"
#include "platform.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>
#ifndef _WIN32
#include <time.h>
#endif

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* name) {
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (ok) g_pass++; else g_fail++;
}

static int runSelfTest() {
    std::printf("tidewalker self-test\n");
    Game g;
    g.opt.headless = true;
    g.init();

    // Relic pickup at every ground-level relic POI (lighthouse top excluded).
    for (size_t i = 0; i < g.world.pois.size(); i++) {
        World::POI& poi = g.world.pois[i];
        if (poi.type == World::POI::DOCK || poi.type == World::POI::LIGHTHOUSE_BASE ||
            poi.type == World::POI::LIGHTHOUSE_TOP) continue;
        g.ppos = poi.pos + Vec3(0.5f, 0.1f, 0.5f);
        g.onUseKey();
    }
    check(g.relics == 4, "4 relics collectible by walking up to POIs");
    check(g.won == false, "not won with 4/5 relics");
    // Climb lighthouse base -> top, then take top relic.
    for (size_t i = 0; i < g.world.pois.size(); i++)
        if (g.world.pois[i].type == World::POI::LIGHTHOUSE_BASE) {
            g.ppos = g.world.pois[i].pos + Vec3(0.5f, 0, 0.5f);
            float before = g.ppos.y;
            g.onUseKey();
            check(g.ppos.y > before + 10.0f, "lighthouse climb teleports player to top");
        }
    for (size_t i = 0; i < g.world.pois.size(); i++)
        if (g.world.pois[i].type == World::POI::LIGHTHOUSE_TOP && !g.world.pois[i].taken) {
            g.ppos = g.world.pois[i].pos + Vec3(0.5f, 0.1f, 0.5f);
            g.onUseKey();
        }
    check(g.relics == 5 && g.won, "5th relic wins the game");

    // Grotto flood hazard: high tide + player inside grotto -> breath drains, horror.
    g.time = 0.06f * g.opt.tidePeriod;
    g.sea = World::tideLevel(g.time, g.opt.tidePeriod);
    g.ppos = {g.world.grottoCenter.x, g.world.grottoFloorAt(g.world.grottoCenter.x, g.world.grottoCenter.z) + 0.5f, g.world.grottoCenter.z};
    g.breath = 100; g.health = 100;
    for (int i = 0; i < 30; i++) g.update(1.0f / 60.0f);
    check(g.breath < 100.0f, "grotto flood drains breath at high tide");
    check(g.sea > g.world.grottoFloorAt(g.world.grottoCenter.x, g.world.grottoCenter.z) + 1.1f, "high tide floods the grotto");

    // Drowned hazard at low tide.
    g.time = 0.56f * g.opt.tidePeriod;
    g.sea = World::tideLevel(g.time, g.opt.tidePeriod);
    g.ppos = g.world.drownedPos[0] + Vec3(1.0f, 0, 0);
    g.ppos.y = g.world.heightAt(g.ppos.x, g.ppos.z);
    g.health = 100; g.breath = 100;
    for (int i = 0; i < 30; i++) g.update(1.0f / 60.0f);
    check(g.health < 100.0f, "drowned figure damages player at low tide");

    // Boat: boards near dock, sails at high tide, aground at low tide.
    g.time = 0.06f * g.opt.tidePeriod;
    g.update(1.0f / 60.0f);  // sync sea with time
    g.aboard = false;
    g.boatPos = g.world.boatStart;
    g.ppos = g.boatPos + Vec3(1.5f, 0, 0);
    g.ppos.y = std::max(g.world.heightAt(g.ppos.x, g.ppos.z), g.sea - 0.55f);
    std::printf("  [diag] boatStart=(%.1f,%.1f) h=%.2f sea=%.2f depth=%.2f db=%.2f dead=%d\n",
                g.boatPos.x, g.boatPos.z, g.world.heightAt(g.boatPos.x, g.boatPos.z), g.sea,
                g.sea - g.world.heightAt(g.boatPos.x, g.boatPos.z),
                std::sqrt((g.ppos.x - g.boatPos.x) * (g.ppos.x - g.boatPos.x) + (g.ppos.z - g.boatPos.z) * (g.ppos.z - g.boatPos.z)),
                (int)g.dead);
    g.onUseKey();
    check(g.aboard, "E near boat boards it at high tide");
    g.keys[K_W] = true;
    Vec3 b0 = g.boatPos;
    for (int i = 0; i < 60; i++) g.update(1.0f / 60.0f);
    check((g.boatPos - b0).len() > 3.0f, "boat sails forward at high tide");
    g.time = 0.56f * g.opt.tidePeriod;  // low tide
    b0 = g.boatPos;
    for (int i = 0; i < 60; i++) g.update(1.0f / 60.0f);
    check((g.boatPos - b0).len() < 0.01f, "boat is aground at low tide");
    g.keys[K_W] = false;

    // Death and respawn (stand next to an exposed drowned figure at low tide).
    g.time = 0.56f * g.opt.tidePeriod;
    g.aboard = false;
    g.keys[K_W] = false;
    g.health = 1.0f;
    g.ppos = g.world.drownedPos[0] + Vec3(0.5f, 0, 0.5f);
    g.ppos.y = g.world.heightAt(g.ppos.x, g.ppos.z);
    for (int i = 0; i < 600 && !g.dead; i++) g.update(1.0f / 60.0f);
    {
        Vec3 dp = g.world.drownedPos[0];
        float d = std::sqrt((g.ppos.x - dp.x) * (g.ppos.x - dp.x) + (g.ppos.z - dp.z) * (g.ppos.z - dp.z));
        std::printf("  [diag] death: health=%.2f sea=%.2f dead=%d aboard=%d dp0.y=%.2f dist=%.2f ppos=(%.1f,%.1f,%.1f)\n",
                    g.health, g.sea, (int)g.dead, (int)g.aboard, dp.y, d, g.ppos.x, g.ppos.y, g.ppos.z);
    }
    check(g.dead, "lethal hazard kills the player");
    g.respawn();
    check(!g.dead && g.health == 100.0f && !g.aboard, "respawn restores state at village");

    // Tide model sanity.
    check(World::tideLevel(0.06f * 360, 360) == World::TIDE_HIGH, "tide high at phase start");
    check(World::tideLevel(0.56f * 360, 360) == World::TIDE_LOW, "tide low at low phase");
    float mid = World::tideLevel(0.30f * 360, 360);
    check(mid < World::TIDE_HIGH && mid > World::TIDE_LOW, "ebb is between high and low");

    std::printf("self-test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}

// Autopilot for --demo: plays the game headless — walks to the wreck at low
// tide, takes the relic, goes to the boat, boards when there is water, sails
// back toward the village.
static void demoDrive(Game& g) {
    for (int i = 0; i < K_COUNT; i++) g.keys[i] = false;
    Vec3 wreck = {280, 0, -150};
    bool wreckTaken = false;
    for (auto& p : g.world.pois)
        if (p.type == World::POI::WRECK) { wreck = p.pos; wreckTaken = p.taken; }
    Vec3 target = wreckTaken ? g.world.boatStart : wreck;
    if (g.aboard) {
        // sail out to open water first, then return to the dock
        static bool reachedWaypoint = false;
        Vec3 waypoint = {450, 0, -300};
        if (!reachedWaypoint) {
            target = waypoint;
            float dw = std::sqrt((g.boatPos.x - waypoint.x) * (g.boatPos.x - waypoint.x) +
                                 (g.boatPos.z - waypoint.z) * (g.boatPos.z - waypoint.z));
            if (dw < 15.0f) reachedWaypoint = true;
        } else {
            target = g.world.dockPos;
        }
    }
    float px = g.aboard ? g.boatPos.x : g.ppos.x;
    float pz = g.aboard ? g.boatPos.z : g.ppos.z;
    float dx = target.x - px, dz = target.z - pz;
    float dist = std::sqrt(dx * dx + dz * dz);
    if (g.aboard) {
        float want = std::atan2(dx, dz);
        float diff = want - g.boatYaw;
        while (diff > 3.14159265f) diff -= 2 * 3.14159265f;
        while (diff < -3.14159265f) diff += 2 * 3.14159265f;
        g.keys[K_A] = diff > 0.08f;
        g.keys[K_D] = diff < -0.08f;
        g.keys[K_W] = dist > 6.0f;
        if (dist <= 6.0f) g.onUseKey();  // disembark near the dock
    } else {
        g.camYaw = std::atan2(dx, dz);
        g.keys[K_W] = dist > 3.5f;
        g.keys[K_SHIFT] = dist > 20.0f;
        if (dist <= 3.5f) g.onUseKey();  // take relic / board boat
    }
}

static void usage() {
    std::printf(
        "Tidewalker - open-world tide adventure\n"
        "Usage: tidewalker [options]\n"
        "  --width N --height N     window size (default 1280x720)\n"
        "  --ssaa F                 supersampling scale (default 1.5, 1 = off)\n"
        "  --tide-period S          seconds per tide cycle (default 360)\n"
        "  --warp F                 time warp (default 1)\n"
        "  --tod F                  start time of day 0..1, 0.25=noon (default 0.30)\n"
        "  --phase high|low|ebb|flood   jump to a tide phase\n"
        "  --headless               no window; render frames to disk\n"
        "  --frames N --out DIR --every K   headless frame dump (default 60 frames, every 10)\n"
        "  --bench                  headless fps benchmark (no PNG output)\n"
        "  --selftest               run headless logic tests and exit\n"
        "  --demo                   autopilot: plays itself headless (walk to wreck,\n"
        "                           loot relic, board boat, sail) while dumping frames\n"
        "  --cam x,y,z,yaw,pitch    fixed camera (headless or windowed)\n"
        "  --player x,z             spawn player at x,z\n");
}

int main(int argc, char** argv) {
    g_font.init();
    GameOptions opt;
    int frames = 60;
    int every = 10;
    bool bench = false;
    bool selftest = false;
    bool demo = false;
    std::string outDir = "frames";

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) { std::fprintf(stderr, "missing value for %s\n", name); std::exit(2); }
            return argv[++i];
        };
        if (a == "--width") opt.winW = std::atoi(next("--width"));
        else if (a == "--height") opt.winH = std::atoi(next("--height"));
        else if (a == "--ssaa") opt.ssaa = (float)std::atof(next("--ssaa"));
        else if (a == "--tide-period") opt.tidePeriod = (float)std::atof(next("--tide-period"));
        else if (a == "--warp") opt.warp = (float)std::atof(next("--warp"));
        else if (a == "--tod") opt.tod = (float)std::atof(next("--tod"));
        else if (a == "--phase") opt.startPhase = next("--phase");
        else if (a == "--headless") opt.headless = true;
        else if (a == "--frames") frames = std::atoi(next("--frames"));
        else if (a == "--out") outDir = next("--out");
        else if (a == "--every") every = std::atoi(next("--every"));
        else if (a == "--bench") { bench = true; opt.headless = true; }
        else if (a == "--selftest") selftest = true;
        else if (a == "--demo") { demo = true; opt.headless = true; }
        else if (a == "--cam") {
            opt.camOverride = true;
            std::sscanf(next("--cam"), "%f,%f,%f,%f,%f", &opt.camX, &opt.camY, &opt.camZ, &opt.camYaw, &opt.camPitch);
        } else if (a == "--player") {
            opt.playerOverride = true;
            std::sscanf(next("--player"), "%f,%f", &opt.playerX, &opt.playerZ);
        } else if (a == "--help" || a == "-h") { usage(); return 0; }
        else { std::fprintf(stderr, "unknown option: %s\n", a.c_str()); usage(); return 2; }
    }

    if (selftest) return runSelfTest();

    Game game;
    game.opt = opt;
    game.init();
    if (demo) game.physScale = opt.warp > 1.0f ? opt.warp : 1.0f;

    // Jump to a tide phase if requested.
    if (!opt.startPhase.empty()) {
        float p = opt.tidePeriod;
        float frac = 0.06f;
        if (opt.startPhase == "high") frac = 0.06f;
        else if (opt.startPhase == "ebb") frac = 0.30f;
        else if (opt.startPhase == "low") frac = 0.56f;
        else if (opt.startPhase == "flood") frac = 0.80f;
        game.time = frac * p;
        game.sea = World::tideLevel(game.time, p);
    }

    int iw = (int)(opt.winW * opt.ssaa), ih = (int)(opt.winH * opt.ssaa);
    Renderer r(iw, ih);
    std::vector<uint32_t> fb((size_t)opt.winW * opt.winH);

    if (opt.headless) {
        std::error_code ec;
        std::filesystem::create_directories(outDir, ec);
        double t0 = 0;
        struct timespec ts0, ts1;
        clock_gettime(CLOCK_MONOTONIC, &ts0);
        t0 = ts0.tv_sec + ts0.tv_nsec * 1e-9;
        int written = 0;
        int lastRelics = game.relics;
        bool lastAboard = game.aboard;
        for (int f = 0; f < frames; f++) {
            if (demo) demoDrive(game);
            game.update(1.0f / 60.0f);
            if (demo) {
                if (game.relics != lastRelics) {
                    std::printf("[demo t=%6.1fs] took relic %d/5 at (%.0f, %.0f)\n",
                                game.time, game.relics, game.ppos.x, game.ppos.z);
                    lastRelics = game.relics;
                }
                if (game.aboard != lastAboard) {
                    std::printf("[demo t=%6.1fs] %s (sea=%.2f)\n", game.time,
                                game.aboard ? "boarded the boat" : "disembarked", game.sea);
                    lastAboard = game.aboard;
                }
            }
            game.render(r);
            r.present(fb.data(), opt.winW, opt.winH, game.vignetteAmt, (uint32_t)f);
            if (!bench && (f % every == 0 || f == frames - 1)) {
                char path[512];
                std::snprintf(path, sizeof path, "%s/frame_%04d.png", outDir.c_str(), f);
                if (writePNG(path, opt.winW, opt.winH, fb.data())) written++;
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &ts1);
        double dt = (ts1.tv_sec + ts1.tv_nsec * 1e-9) - t0;
        std::printf("headless: %d frames in %.2fs -> %.1f fps (internal %dx%d, window %dx%d, ssaa %.2f), %d PNGs -> %s\n",
                    frames, dt, frames / dt, iw, ih, opt.winW, opt.winH, opt.ssaa, written, outDir.c_str());
        std::printf("tide: sea=%.2f phase-left=%.1fs relics=%d health=%.0f\n",
                    game.sea, [&]{ std::string n; float l; World::tidePhase(game.time, game.opt.tidePeriod, n, l); return l; }(),
                    game.relics, game.health);
        return 0;
    }

    Platform* plat = createPlatform();
    if (!plat->init("Tidewalker", opt.winW, opt.winH)) {
        std::fprintf(stderr, "tidewalker: failed to open a window. Use --headless to render frames without a display.\n");
        return 1;
    }
    double last = plat->now();
    while (!game.quitRequested) {
        InputEvent e;
        while (plat->poll(e)) {
            if (e.type == InputEvent::Quit) game.quitRequested = true;
            else game.setKey(e.key, e.type == InputEvent::KeyDown);
        }
        double now = plat->now();
        float dt = (float)(now - last);
        last = now;
        if (dt > 0.05f) dt = 0.05f;
        if (dt <= 0) dt = 1e-4f;
        game.update(dt);
        game.render(r);
        r.present(fb.data(), opt.winW, opt.winH, game.vignetteAmt, (uint32_t)game.frameNo);
        plat->present(fb.data(), opt.winW, opt.winH);
        double spent = plat->now() - now;
        if (spent < 1.0 / 60.0) plat->sleepMs((int)((1.0 / 60.0 - spent) * 1000));
    }
    delete plat;
    return 0;
}
