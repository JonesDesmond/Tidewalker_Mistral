#pragma once
#include <cstdint>

// 5x7 bitmap font. Glyphs are defined as 7 rows of 5 chars ('#' = on) in font.cpp
// and converted to 5 column-bytes (bit 0 = top row) at startup.
struct Font5x7 {
    uint8_t cols[128][5];
    bool loaded = false;
    void init();
    // Returns column bytes for c (unknown chars map to '?').
    const uint8_t* glyph(char c) const { return cols[(uint8_t)c]; }
};
extern Font5x7 g_font;
