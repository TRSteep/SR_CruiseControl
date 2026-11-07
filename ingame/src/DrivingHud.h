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
    return visible && now >= observedAt && now - observedAt <= 2000;
}

// Visual tracking is deliberately separate from throttle control. Once three
// strong, consistent samples calibrate the row, its pose stays fixed until the
// driving HUD has been absent for two seconds or the UI explicitly resets it.
class StableDrivingHud {
    std::vector<HudAnchor> calibration;
    HudAnchor locked;
    uint64_t acceptedAt = 0, observedAt = 0;
    bool calibrated = false, hasObservation = false;
    static bool consistent(const HudAnchor& a, const HudAnchor& b) {
        return a.scale > 0 && b.scale > 0 &&
            std::abs(a.x - b.x) <= 60 * b.scale && std::abs(a.y - b.y) <= 50 * b.scale &&
            a.scale >= b.scale * .65f && a.scale <= b.scale * 1.55f;
    }
public:
    void reset() { calibration.clear(); locked = {}; acceptedAt = observedAt = 0; calibrated = hasObservation = false; }
    void observe(const HudAnchor& sample, uint64_t now) {
        if (hasObservation && now < observedAt) return;
        observedAt = now; hasObservation = true;
        if (calibrated && now >= acceptedAt && now - acceptedAt > 2000) {
            calibration.clear(); calibrated = false; locked = {};
        }
        if (calibrated) {
            // Hysteresis: a deformed/faded glyph may sustain an already proven
            // session, but cannot establish one on a loading/menu background.
            if (sample.confidence >= .62f && consistent(sample, locked)) {
                acceptedAt = now; locked.confidence = sample.confidence;
            }
            return;
        }
        if (!sample.visible || sample.confidence < .78f) return;
        if (!calibration.empty() && (!consistent(sample, calibration.front()) || now - acceptedAt > 2000)) calibration.clear();
        acceptedAt = now;
        calibration.push_back(sample);
        if (calibration.size() < 3) return;
        auto median = [&](float HudAnchor::*field) {
            float values[3] = {calibration[0].*field, calibration[1].*field, calibration[2].*field};
            std::sort(values, values + 3); return values[1];
        };
        locked = {true, median(&HudAnchor::x), median(&HudAnchor::y), median(&HudAnchor::scale), sample.confidence};
        calibrated = true; calibration.clear();
    }
    HudAnchor current(uint64_t now) const {
        HudAnchor result = locked;
        result.visible = calibrated && drivingHudAvailable(true, acceptedAt, now);
        return result;
    }
    uint64_t lastAcceptedAt() const { return acceptedAt; }
};

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
