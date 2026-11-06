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
    if (!autopilot::drivingHudAvailable(true, 1000, 1200) || autopilot::drivingHudAvailable(true, 1000, 1501) ||
        autopilot::drivingHudAvailable(false, 1000, 1100) || autopilot::drivingHudAvailable(true, 1000, 900))
        throw std::runtime_error("Stale or missing HUD must disable autopilot");
    uint32_t random = 42;
    for (auto& pixel : blank) { random = random * 1664525u + 1013904223u; pixel = random >> 24; }
    if (detector.find(blank.data()).visible) throw std::runtime_error("Background clutter incorrectly visible");
    std::puts("PASS: absent, flat, stale and cluttered HUD stay disabled");
}
