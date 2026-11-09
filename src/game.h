#pragma once
#include "world.h"
#include "platform.h"
#include "math.h"
#include <string>

struct GameOptions {
    int winW = 1280, winH = 720;
    float ssaa = 1.25f;         // internal render scale (supersampling)
    float tidePeriod = 360.0f;  // seconds per full tide cycle
    float warp = 1.0f;          // time warp multiplier (T toggles x8)
    float tod = 0.30f;          // start time of day, 0..1 (0.25 = noon)
    float dayLength = 900.0f;   // seconds per full day
    bool headless = false;
    // headless scripting
    bool camOverride = false;
    float camX = 0, camY = 10, camZ = 0, camYaw = 0, camPitch = -0.3f;
    bool playerOverride = false;
    float playerX = 0, playerZ = 0;
    std::string startPhase;     // "", high, low, ebb, flood
};

class Game {
public:
    World world;
    GameOptions opt;
    bool keys[K_COUNT] = {};
    bool quitRequested = false;

    float time = 0;
    float physScale = 1.0f;   // demo mode: scale physics dt to match time warp
    float tod;
    float sea = World::TIDE_LOW;
    Vec3 sunDir{0, 1, 0};
    float daylight = 1.0f;

    Vec3 ppos{40, 6, 40}, pvel;
    float pyaw = 0;
    bool aboard = false;
    Vec3 boatPos, boatYawV;
    float boatYaw = 0;
    float health = 100, stamina = 100, breath = 100;
    bool dead = false, won = false;
    int relics = 0;
    float camYaw = 2.06f, camPitch = -0.32f;
    float walkPhase = 0, swimPhase = 0;
    std::string msg;
    float msgT = 0;
    bool horror = false;
    float vignetteAmt = 0.35f;
    int frameNo = 0;

    Game();
    void init();
    void setKey(int k, bool down);
    void update(float dt);
    void render(IRenderer& r);
    void say(const std::string& s, float dur = 4.0f);
    void respawn();
    void onUseKey();

private:
    bool prevKeys[K_COUNT] = {};
    int lastPhaseIdx = -1;
    void updatePlayer(float dt);
    void hazards(float dt);
    void drawHUD(IRenderer& r);
};
