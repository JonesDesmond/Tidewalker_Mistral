#pragma once
#include "math.h"
#include "renderer.h"
#include <vector>
#include <string>

// Tidewalker world: a coastal archipelago whose seabed is exposed at low tide.
struct World {
    static constexpr float SIZE = 1600.0f;
    static constexpr float HALF = SIZE / 2;
    static constexpr int HM = 129;
    static constexpr float TIDE_LOW = -2.0f;
    static constexpr float TIDE_HIGH = 3.0f;

    float hm[HM * HM] = {};

    // Textures (generated once).
    Tex texSand, texGrass, texRock, texWood, texStone, texLeaf, texKelp, texClouds;

    Mesh terrain;        // splat: sand/grass/rock
    Mesh detailGrass;    // alphaTest leaf tufts
    Mesh detailRock;
    Mesh detailKelp;
    Mesh propsWood, propsRock, propsStone, propsRed, propsEmissive;
    Mesh drownedMesh;
    Mesh boatMesh;
    Mesh blob;           // unit blob shadow
    // Player body parts (pivot at hip/shoulder for limb swing).
    Mesh playerLegL, playerLegR, playerArmL, playerArmR, playerTorso, playerHead;

    std::vector<Vec3> drownedPos;

    static constexpr int WN = 64;
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
    void buildTextures();
    float heightAt(float x, float z) const;
    bool inGrotto(float x, float z) const;
    float grottoFloorAt(float x, float z) const;

    static float tideLevel(float t, float period);
    static void tidePhase(float t, float period, std::string& name, float& secondsLeft);
    static float wave(float x, float z, float t);
};
