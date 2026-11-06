#pragma once
#include "math.h"
#include "renderer.h"
#include <vector>
#include <string>

// Tidewalker world: a coastal archipelago whose seabed is exposed at low tide.
struct World {
    static constexpr float SIZE = 1600.0f;   // world is SIZE x SIZE meters
    static constexpr float HALF = SIZE / 2;
    static constexpr int HM = 129;            // heightmap resolution
    static constexpr float TIDE_LOW = -2.0f;
    static constexpr float TIDE_HIGH = 3.0f;

    float hm[HM * HM] = {};

    Mesh terrain;      // static, lit per frame
    Mesh props;        // static props (wreck, shrine, huts, lighthouse, dock, grotto rocks)
    Mesh drownedMesh;  // one pale figure; drawn per drowned position
    Mesh boatMesh;
    Mesh playerMesh;

    std::vector<Vec3> drownedPos;

    // Water surface grid (positions animated per frame by the game).
    static constexpr int WN = 56;
    std::vector<Vec2> waterBase;
    Mesh waterMesh;

    struct POI {
        enum Type { WRECK, GROTTO, SHRINE, LIGHTHOUSE_BASE, LIGHTHOUSE_TOP, VILLAGE, DOCK } type;
        Vec3 pos;
        bool taken = false;
    };
    std::vector<POI> pois;

    Vec3 grottoCenter = {466, 0, -250};
    float grottoRadius = 22.0f;
    Vec3 dockPos = {240, 0, -128};
    Vec3 boatStart = {300, 0, -160};
    Vec3 villagePos = {30, 0, 20};

    void build();
    float heightAt(float x, float z) const;
    bool inGrotto(float x, float z) const;
    float grottoFloorAt(float x, float z) const;

    // Base sea level (no waves) for world time t with the given period (seconds).
    static float tideLevel(float t, float period);
    // Human-readable phase info.
    static void tidePhase(float t, float period, std::string& name, float& secondsLeft);
    static float wave(float x, float z, float t);
};
