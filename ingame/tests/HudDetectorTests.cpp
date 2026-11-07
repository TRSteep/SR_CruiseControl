#include "../src/DrivingHud.h"
#include <cstdio>
#include <fstream>
#include <stdexcept>

int main(int argc, char** argv) {
    autopilot::DrivingHudDetector detector;
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            std::ifstream file(argv[i], std::ios::binary);
            uint32_t width = 0, height = 0;
            file.read(reinterpret_cast<char*>(&width), 4); file.read(reinterpret_cast<char*>(&height), 4);
            if (!file || width > 10000 || height > 10000 || !width || !height) throw std::runtime_error("Invalid image");
            std::vector<unsigned char> pixels(width * height);
            file.read(reinterpret_cast<char*>(pixels.data()), pixels.size());
            if (!file) throw std::runtime_error("Truncated image");
            auto crop = detector.cropGray(pixels.data(), width, height);
            auto anchor = detector.find(crop.data());
            std::printf("%s: visible=%d score=%.3f center=(%.1f,%.1f) scale=%.3f\n", argv[i], anchor.visible,
                anchor.confidence, anchor.x, anchor.y, anchor.scale);
            const bool expected = std::string(argv[i]).find("load") == std::string::npos;
            if (anchor.visible != expected) return 1;
        }
        return 0;
    }
    std::vector<unsigned char> blank(detector.cropWidth * detector.cropHeight, 45);
    if (detector.find(blank.data()).visible) throw std::runtime_error("Empty HUD incorrectly visible");
    if (!autopilot::drivingHudAvailable(true, 1000, 1200) || autopilot::drivingHudAvailable(true, 1000, 3001) ||
        autopilot::drivingHudAvailable(false, 1000, 1100) || autopilot::drivingHudAvailable(true, 1000, 900))
        throw std::runtime_error("Stale or missing HUD must disable autopilot");
    uint32_t random = 42;
    for (auto& pixel : blank) { random = random * 1664525u + 1013904223u; pixel = random >> 24; }
    if (detector.find(blank.data()).visible) throw std::runtime_error("Background clutter incorrectly visible");
    autopilot::StableDrivingHud tracker;
    tracker.observe({true, 1800, 735, 1, .9f}, 1000);
    tracker.observe({true, 1808, 739, 1.05f, .85f}, 1120);
    if (tracker.current(1120).visible) throw std::runtime_error("One-off recognition cannot establish driving mode");
    tracker.observe({true, 1797, 731, .97f, .84f}, 1240);
    const auto fixed = tracker.current(1240);
    if (!fixed.visible || fixed.x != 1800 || fixed.y != 735 || fixed.scale != 1)
        throw std::runtime_error("HUD calibration must use a stable median");
    const float scores[] = {.768f,.791f,.778f,.787f,.738f,.794f,.717f,.814f,.766f,.863f,.606f,.797f,.752f,.855f,.636f};
    uint64_t now = 1240;
    for (int i = 0; i < 90; ++i) {
        now += 120;
        const float score = scores[i % 15];
        tracker.observe({score >= .78f, 1800.f + (i % 9 - 4) * 5, 735.f + (i % 7 - 3) * 4,
            1.f + (i % 5 - 2) * .03f, score}, now);
        const auto stable = tracker.current(now);
        if (!stable.visible || stable.x != fixed.x || stable.y != fixed.y || stable.scale != fixed.scale)
            throw std::runtime_error("Road shake/real log confidence dips must neither move nor hide the row");
    }
    const uint64_t seen = tracker.lastAcceptedAt();
    tracker.observe({}, now + 120);
    if (!tracker.current(now + 120).visible) throw std::runtime_error("Single missing frame must preserve row");
    tracker.observe({}, seen + 2100);
    if (tracker.current(seen + 2100).visible) throw std::runtime_error("Sustained absence must hide row");
    tracker.observe({false, 1800, 735, 1, .74f}, seen + 2220);
    if (tracker.current(seen + 2220).visible) throw std::runtime_error("Weak match cannot reopen a driving session");
    for (int i = 0; i < 3; ++i) tracker.observe({true, 1810, 730, 1, .9f}, seen + 2400 + i * 120);
    if (!tracker.current(seen + 2640).visible) throw std::runtime_error("Returning to driving must recalibrate");
    tracker.reset();
    if (tracker.current(seen + 2640).visible) throw std::runtime_error("Menu/resolution reset must hide immediately");
    std::puts("PASS: HUD calibration, fixed pose, 90 road-shake samples, confidence hysteresis, missing-frame grace, expiry and re-entry");
}
