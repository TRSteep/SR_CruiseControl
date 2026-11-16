#pragma once
// Generated from reviewed Epic 44.1. RIP displacements and relative branches are masked.
// Candidate signatures identify code; they do not prove ABI compatibility on another build.
namespace srcompat {
struct Symbol { const char* name; const char* pattern; };
inline constexpr Symbol symbols[] = {
    {"create","48 89 5C 24 18 48 89 74 24 20 55 41 54 41 55 41 56 41 57 48 8D 6C 24 C9 48 81 EC D0 00 00 00 8B 41 70 4C 8D 25 ?? ?? ?? ?? 25 8F 00 00 00 4C 8D 2D ?? ?? ?? ?? 45 33 FF 4C 8B F2 48 8B F1 83 F8 02"},
    {"attach","4C 8B DC 4D 89 4B 20 4D 89 43 18 55 56 41 57 49 8D 6B D8 48 81 EC 10 01 00 00 48 8B 41 08 49 8B F1 48 8B 4A 28 4C 8B 78 18 8B 81 90 00 00 00 83 E8 1F 83 F8 05 0F 83 ?? ?? ?? ?? 0F B6 81 88 00 00 00"},
    {"get","48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 41 54 41 55 41 56 41 57 48 8D 6C 24 D1 48 81 EC 90 00 00 00 48 8B 41 08 4C 8B F2 49 8B D0 49 8B F9 4D 8B E0 4C 8B 78 18 49 8B 8F B8 03 00 00 49 8B B7 88 00 00 00"},
    {"set","48 89 5C 24 20 4C 89 44 24 18 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 70 FC FF FF 48 81 EC 90 04 00 00 48 8B 41 08 48 8B FA 49 8B D0 4D 8B E1 4D 8B F0 4C 8B 78 18 49 8B 8F B8 03 00 00 49 8B B7 88 00 00 00"},
    {"invoke","4C 89 44 24 18 55 53 56 57 41 54 41 55 41 56 48 8D 6C 24 E0 48 81 EC 20 01 00 00 48 8B 41 08 4C 8B F2 49 8B D1 49 8B F0 48 8B 78 18 48 89 7D 68 48 8B 8F B8 03 00 00 4C 8B AF 88 00 00 00 E8 ?? ?? ?? ??"},
    {"release","40 53 48 83 EC 20 8B 42 18 48 8B DA 25 8F 00 00 00 83 F8 06 74 ?? 83 F8 07 74 ?? 76 ?? 83 F8 0A 76 ?? 83 F8 0B 75 ?? 49 83 E0 FD 41 8B 40 08 A9 FF FF 3F 00 74 ?? FF C8 49 8B C8 41 89 40 08 E8 ?? ?? ?? ??"},
    {"empty","4C 8B DC 55 56 41 56 41 57 48 8B EC 48 83 EC 78 48 8B 41 08 4D 8B F1 48 8B 4A 28 4D 8B F8 48 8B 70 18 8B 81 90 00 00 00 83 E8 1F 83 F8 05 0F 83 ?? ?? ?? ?? 0F B6 81 88 00 00 00 C0 E8 05 F6 D0"},
    {"retain","48 89 5C 24 08 57 48 83 EC 20 8B 42 18 48 8B DA 25 8F 00 00 00 48 8B F9 83 F8 06 74 ?? 83 F8 07 74 ?? 76 ?? 83 F8 0A 76 ?? 83 F8 0B 75 ?? 49 83 E0 FD 41 8B 40 08 FF C0 25 FF FF FF 8F 41 89 40 08"},
    {"advance","44 88 4C 24 20 44 89 44 24 18 41 57 48 81 EC E0 00 00 00 8B 81 F0 52 00 00 41 0F B6 D1 C1 E8 14 4C 8B F9 0F 29 B4 24 A0 00 00 00 0F 28 F1 A8 01 74 ?? 84 D2 74 ?? B2 01 E8 ?? ?? ?? ?? F3 0F 10 05 ?? ?? ?? ??"},
    {"destroy","48 89 5C 24 08 57 48 83 EC 20 8B FA 48 8B D9 E8 ?? ?? ?? ?? 40 F6 C7 01 74 ?? 40 F6 C7 04 75 ?? 48 8B 0D ?? ?? ?? ?? 48 8B D3 48 8B 01 FF 50 60 48 8B C3 48 8B 5C 24 30 48 83 C4 20 5F C3"},
};
}
