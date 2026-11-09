#pragma once
#include "renderer.h"

// Procedurally generated textures (0xAARRGGBB), created once at startup.
Tex makeTexSand();    // 128 tan rippled sand
Tex makeTexGrass();   // 128 green with blade noise
Tex makeTexRock();    // 128 grey-brown cracked rock
Tex makeTexWood();    // 128 plank wood with grain
Tex makeTexStone();   // 128 stone blocks with mortar
Tex makeTexLeaf();    // 64 alpha-tested grass blades
Tex makeTexKelp();    // 64 alpha-tested kelp fronds
Tex makeTexClouds();  // 512x256 fbm cloud layer
