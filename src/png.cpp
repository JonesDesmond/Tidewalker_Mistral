#include "png.h"
#include <cstdio>
#include <vector>
#include <zlib.h>

static void putU32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x >> 24)); v.push_back((uint8_t)(x >> 16));
    v.push_back((uint8_t)(x >> 8)); v.push_back((uint8_t)x);
}
static void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    putU32(out, (uint32_t)data.size());
    size_t tpos = out.size();
    for (int i = 0; i < 4; i++) out.push_back((uint8_t)type[i]);
    out.insert(out.end(), data.begin(), data.end());
    putU32(out, crc32(crc32(0, Z_NULL, 0), &out[tpos], (uInt)(4 + data.size())));
}

bool writePNG(const char* path, int w, int h, const uint32_t* pix) {
    std::vector<uint8_t> raw;
    raw.reserve((size_t)w * h * 3 + h);
    for (int y = 0; y < h; y++) {
        raw.push_back(0);  // filter: none
        for (int x = 0; x < w; x++) {
            uint32_t p = pix[(size_t)y * w + x];
            raw.push_back((uint8_t)(p >> 16));
            raw.push_back((uint8_t)(p >> 8));
            raw.push_back((uint8_t)p);
        }
    }
    uLongf compLen = compressBound((uLong)raw.size());
    std::vector<uint8_t> comp(compLen);
    if (compress2(comp.data(), &compLen, raw.data(), (uLong)raw.size(), 9) != Z_OK) return false;
    comp.resize(compLen);

    std::vector<uint8_t> out;
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    out.insert(out.end(), sig, sig + 8);
    std::vector<uint8_t> ihdr;
    putU32(ihdr, (uint32_t)w); putU32(ihdr, (uint32_t)h);
    ihdr.push_back(8); ihdr.push_back(2); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", comp);
    chunk(out, "IEND", {});

    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}
