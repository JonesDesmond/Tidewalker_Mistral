#pragma once
#include <cstdint>

// Writes a 24-bit RGB PNG. pix is w*h pixels packed as 0x00RRGGBB.
bool writePNG(const char* path, int w, int h, const uint32_t* pix);
