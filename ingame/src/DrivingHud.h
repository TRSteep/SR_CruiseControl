#pragma once
#include "HudReference.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace autopilot {
struct HudAnchor {
    bool visible = false;
    float x = 0, y = 0, scale = 1, confidence = 0;
};

inline bool drivingHudAvailable(bool visible, uint64_t observedAt, uint64_t now) {
    return visible && now >= observedAt && now - observedAt <= 500;
}

// Detect the native parking-brake glyph in a small, normalized HUD crop.
// Appearance follows the actual driving HUD, rather than keyboard/menu guesses.
class DrivingHudDetector {
    struct Reference { float values[252]{}; float norm = 0; };
    Reference references[2];
    static float sample(const unsigned char* pixels, int width, int height, float x, float y) {
        x = std::clamp(x, 0.f, static_cast<float>(width - 1));
        y = std::clamp(y, 0.f, static_cast<float>(height - 1));
        int ix = static_cast<int>(x), iy = static_cast<int>(y);
        int nx = std::min(ix + 1, width - 1), ny = std::min(iy + 1, height - 1);
        float fx = x - ix, fy = y - iy;
        return (pixels[iy * width + ix] * (1 - fx) + pixels[iy * width + nx] * fx) * (1 - fy) +
            (pixels[ny * width + ix] * (1 - fx) + pixels[ny * width + nx] * fx) * fy;
    }
    float score(const unsigned char* crop, int x, int y, float w, float h) const {
        float values[252], sum = 0, squared = 0;
        for (int row = 0, i = 0; row < markerRows; ++row)
            for (int col = 0; col < markerColumns; ++col, ++i) {
                float value = sample(crop, cropWidth, cropHeight, x + (col + .5f) * w / markerColumns - .5f,
                    y + (row + .5f) * h / markerRows - .5f);
                values[i] = value; sum += value; squared += value * value;
            }
        const float variance = std::max(0.f, squared - sum * sum / 252.f);
        if (variance < 252.f * 9.f * 9.f) return 0; // Flat/loading images cannot match.
        float best = 0;
        for (const Reference& ref : references) {
            float product = 0;
            for (int i = 0; i < 252; ++i) product += values[i] * ref.values[i];
            best = std::max(best, product / std::sqrt(variance * ref.norm));
        }
        return best;
    }
public:
    static constexpr int cropWidth = 220, cropHeight = 260;
    static constexpr float cropRight = 240, cropBottom = 440;
    DrivingHudDetector() {
        for (int r = 0; r < 2; ++r) {
            float mean = 0;
            for (unsigned char value : markerReferences[r]) mean += value / 252.f;
            for (int i = 0; i < 252; ++i) {
                references[r].values[i] = markerReferences[r][i] - mean;
                references[r].norm += references[r].values[i] * references[r].values[i];
            }
        }
    }
    HudAnchor find(const unsigned char* crop) const {
        HudAnchor best;
        for (float width : {32.f, 34.f, 36.f, 38.f, 40.f}) {
            const float height = width * .75f;
            for (int y = 12; y < cropHeight - height - 12; y += 3)
                for (int x = 45; x < cropWidth - width - 6; x += 3) {
                    float value = score(crop, x, y, width, height);
                    if (value > best.confidence) best = {false, x + width / 2, y + height / 2, width / 36, value};
                }
        }
        if (best.confidence < .55f) return best;
        const HudAnchor coarse = best;
        for (float width = coarse.scale * 36 - 1; width <= coarse.scale * 36 + 1; width += 1)
            for (int y = static_cast<int>(coarse.y - width * .375f) - 2; y <= static_cast<int>(coarse.y - width * .375f) + 2; ++y)
                for (int x = static_cast<int>(coarse.x - width / 2) - 2; x <= static_cast<int>(coarse.x - width / 2) + 2; ++x) {
                    float value = score(crop, x, y, width, width * .75f);
                    if (value > best.confidence) best = {false, x + width / 2, y + width * .375f, width / 36, value};
                }
        best.visible = best.confidence >= .78f;
        return best;
    }
    // CPU-only reference-image tests use the same normalized crop as the GPU path.
    static std::vector<unsigned char> cropGray(const unsigned char* frame, int width, int height) {
        std::vector<unsigned char> crop(cropWidth * cropHeight);
        float scale = height / 1080.f;
        for (int y = 0; y < cropHeight; ++y)
            for (int x = 0; x < cropWidth; ++x)
                crop[y * cropWidth + x] = static_cast<unsigned char>(sample(frame, width, height,
                    width - cropRight * scale + (x + .5f) * scale - .5f,
                    height - cropBottom * scale + (y + .5f) * scale - .5f));
        return crop;
    }
};
}
