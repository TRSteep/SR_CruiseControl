#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace autopilot {
// Original procedural brush mask. No textures extracted from the game are
// redistributed: ragged edges, ink grain and chipped areas are generated here.
inline std::vector<unsigned char> makePanelArtwork(int width = 512, int height = 112) {
    auto noise = [](int x, int y) {
        uint32_t n = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + 19u;
        n = (n ^ (n >> 13)) * 1274126177u;
        return ((n ^ (n >> 16)) & 65535) / 65535.f;
    };
    std::vector<unsigned char> pixels(width * height * 4);
    for (int py = 0; py < height; ++py) for (int px = 0; px < width; ++px) {
        const float x = px * 218.f / (width - 1) - 12;
        const float y = py * 46.f / (height - 1) - 3;
        const int ix = static_cast<int>(std::floor(x)), iy = static_cast<int>(std::floor(y));
        const float left = 1.5f + 3 * std::sin(y * 1.6f) + 6 * noise(1, iy);
        const float right = 202 - 2 * noise(7, iy);
        const float top = 2 * noise(ix, 11);
        const float bottom = 40 - 2.5f * noise(ix, 29);
        float alpha = std::clamp(std::min({x - left, right - x, y - top, bottom - y}) * 1.5f, 0.f, 1.f);
        if (x < left && x > -11 && y > 0 && y < 40 && noise(ix / 2, iy) > .64f)
            alpha = std::max(alpha, .6f * noise(ix, iy));
        if ((x < 32 || y < 5 || y > 35) && noise(ix / 2, iy / 2) > .68f && noise(ix, iy) > .50f)
            alpha *= .08f;
        alpha *= .88f + .12f * noise(ix, iy);
        const size_t offset = (py * width + px) * 4;
        pixels[offset] = pixels[offset + 1] = pixels[offset + 2] = 255;
        pixels[offset + 3] = static_cast<unsigned char>(255 * alpha);
    }
    return pixels;
}
}
