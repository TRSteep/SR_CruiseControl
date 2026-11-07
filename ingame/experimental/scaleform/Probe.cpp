#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include "MinHook.h"
#include "NativeUi.h"

namespace probe {
// Deliberately separate from the released autopilot. These offsets describe
// only the EXE whose complete SHA256 is checked before any hook is installed.
constexpr uintptr_t createRva = 0x10243a0, attachRva = 0x1b61880;
constexpr unsigned char createPrefix[] = {0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x55,0x41,0x54};
constexpr unsigned char attachPrefix[] = {0x4c,0x8b,0xdc,0x4d,0x89,0x4b,0x20,0x4d,0x89,0x43,0x18,0x55};
constexpr char expectedHash[] = "a3cb442ec138eb2eb7e335e5c80e376766550740e49e233ac9eace035e182285";

// Observed game wrapper: two cache words precede its managed GFx fields.
// Never retain, release or manufacture a game-owned value in this observer.
using ObservedValue = nativeui::Value;
static_assert(sizeof(ObservedValue) == 48);
static_assert(offsetof(ObservedValue, type) == 0x18);
using Create = uintptr_t(*)(void*, const void*);
using Attach = bool(*)(void*, void*, ObservedValue*, const char*, const char*, int, const void*);
Create originalCreate = nullptr;
Attach originalAttach = nullptr;
nativeui::Api ui{};
bool enableTestRow = false;

bool read(const void* from, void* to, size_t bytes) {
    SIZE_T copied = 0;
    return from && ReadProcessMemory(GetCurrentProcess(), from, to, bytes, &copied) && copied == bytes;
}
template<size_t N> void textAt(const char* address, char (&output)[N]) {
    output[0] = 0;
    MEMORY_BASIC_INFORMATION region{};
    if (!address || !VirtualQuery(address, &region, sizeof(region)) || region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return;
    const auto end = reinterpret_cast<uintptr_t>(region.BaseAddress) + region.RegionSize;
    const auto available = std::min<size_t>(N - 1, end - reinterpret_cast<uintptr_t>(address));
    if (!read(address, output, available)) { output[0] = 0; return; }
    output[available] = 0;
    for (size_t i = 0; output[i]; ++i) if (static_cast<unsigned char>(output[i]) < 32) { output[i] = 0; break; }
}
template<size_t N> void gameString(const void* address, char (&output)[N]) {
    struct Header { uint32_t length, inlineCapacity; uintptr_t data; } h{};
    output[0] = 0;
    if (!read(address, &h, sizeof(h)) || h.length == 0 || h.length > 4096) return;
    const char* characters = h.inlineCapacity ? reinterpret_cast<const char*>(address) + 8 : reinterpret_cast<const char*>(h.data);
    textAt(characters, output);
}
struct Event {
    DWORD thread; unsigned kind; uintptr_t owner, parent, output;
    uint32_t type; uintptr_t interfacePointer, object;
    int depth; bool success; char symbol[128], instance[128];
};
constexpr unsigned capacity = 4096;
Event events[capacity]{};
std::atomic<bool> ready[capacity]{};
std::atomic<unsigned> claimed{0}, observed{0};
void publish(const Event& event) {
    const unsigned index = claimed.fetch_add(1, std::memory_order_relaxed);
    if (index >= capacity) return;
    events[index] = event; ready[index].store(true, std::memory_order_release);
}
bool relevant(const char* name) {
    // Bounded sample of other symbols establishes whether the hook executes.
    const auto index = observed.fetch_add(1, std::memory_order_relaxed);
    return index < 96 || std::strstr(name, "Hud") || std::strstr(name, "hud") ||
        std::strstr(name, "brake") || std::strstr(name, "Brake") || std::strstr(name, "game");
}
void snapshot(Event& e, const ObservedValue* value) {
    ObservedValue copy{};
    if (read(value, &copy, sizeof(copy))) {
        e.type = copy.type; e.interfacePointer = copy.interfacePointer; e.object = copy.object;
    }
}
uintptr_t createHook(void* owner, const void* linkage) {
    Event e{}; e.kind = 1; e.owner = reinterpret_cast<uintptr_t>(owner); e.thread = GetCurrentThreadId();
    gameString(linkage, e.symbol);
    const auto result = originalCreate(owner, linkage);
    if (!relevant(e.symbol)) return result;
    // The original factory stores its created display object at owner + 0x58.
    snapshot(e, reinterpret_cast<const ObservedValue*>(reinterpret_cast<uintptr_t>(owner) + 0x58));
    publish(e);
    if (!std::strcmp(e.symbol,"defaultHud")) {
        Event test{}; test.kind = 3; test.thread = GetCurrentThreadId();
        std::strcpy(test.symbol,"TEST-disabled");
        if (enableTestRow) {
            ObservedValue hud;
            if (read(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(owner)+0x58),&hud,sizeof(hud))) {
                const auto stage = nativeui::addTestRow(ui,hud);
                test.type = static_cast<unsigned>(stage); test.success = stage == nativeui::Stage::Created;
                std::strcpy(test.symbol,"TEST-result");
            }
        }
        publish(test);
    }
    return result;
}
bool attachHook(void* interfacePointer, void* parent, ObservedValue* result,
                const char* symbol, const char* instance, int depth, const void* init) {
    Event e{}; e.kind = 2; e.owner = reinterpret_cast<uintptr_t>(interfacePointer);
    e.parent = reinterpret_cast<uintptr_t>(parent); e.output = reinterpret_cast<uintptr_t>(result);
    e.thread = GetCurrentThreadId(); e.depth = depth;
    textAt(symbol, e.symbol); textAt(instance, e.instance);
    const bool success = originalAttach(interfacePointer, parent, result, symbol, instance, depth, init);
    if (relevant(e.symbol)) { e.success = success; snapshot(e, result); publish(e); }
    return success;
}

bool hashExe(char (&hex)[65]) {
    wchar_t path[32768]{}; GetModuleFileNameW(nullptr, path, 32768);
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    bool okay = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
    unsigned char buffer[65536], digest[32]; DWORD bytes = 0;
    while (okay) {
        if (!ReadFile(file, buffer, sizeof(buffer), &bytes, nullptr)) { okay = false; break; }
        if (!bytes) break;
        okay = BCryptHashData(hash, buffer, bytes, 0) >= 0;
    }
    if (okay) okay = BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    if (okay) for (unsigned i = 0; i < 32; ++i) std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
    return okay;
}
bool compatible(const unsigned char* image) {
    unsigned char actualCreate[sizeof(createPrefix)], actualAttach[sizeof(attachPrefix)];
    return read(image + createRva, actualCreate, sizeof(actualCreate)) &&
        read(image + attachRva, actualAttach, sizeof(actualAttach)) &&
        !std::memcmp(actualCreate, createPrefix, sizeof(createPrefix)) &&
        !std::memcmp(actualAttach, attachPrefix, sizeof(attachPrefix));
}
bool bindUi(unsigned char* image) {
    struct Entry { uintptr_t rva; unsigned char prefix[4]; };
    const Entry entries[] = {
        {0x1b5d4b0,{0x48,0x89,0x5c,0x24}}, {0x1b5d870,{0x48,0x89,0x5c,0x24}},
        {0x1b5dcb0,{0x4c,0x89,0x44,0x24}}, {0x1b5d0f0,{0x40,0x53,0x48,0x83}},
        {0x1b61560,{0x4c,0x8b,0xdc,0x55}}
    };
    for (const auto& entry:entries) {
        unsigned char bytes[4];
        if (!read(image+entry.rva,bytes,sizeof(bytes)) || std::memcmp(bytes,entry.prefix,sizeof(bytes))) return false;
    }
    ui = {reinterpret_cast<nativeui::Get>(image+0x1b5d4b0),reinterpret_cast<nativeui::Set>(image+0x1b5d870),
        reinterpret_cast<nativeui::Invoke>(image+0x1b5dcb0),reinterpret_cast<nativeui::Release>(image+0x1b5d0f0),
        reinterpret_cast<nativeui::Attach>(image+attachRva),reinterpret_cast<nativeui::Empty>(image+0x1b61560)};
    return true;
}
HMODULE module = nullptr;
DWORD WINAPI run(void*) {
    wchar_t path[32768]{}; GetModuleFileNameW(module, path, 32768);
    std::wstring logPath(path); logPath.resize(logPath.find_last_of(L"\\/") + 1);
    const std::wstring configPath = logPath + L"SnowRunnerScaleformProbe.ini";
    enableTestRow = GetPrivateProfileIntW(L"Probe",L"EnableTestRow",0,configPath.c_str()) == 1;
    logPath += L"SnowRunnerScaleformProbe.log";
    FILE* log = _wfopen(logPath.c_str(), L"w"); if (!log) return 1;
    std::fprintf(log, "Scaleform diagnostic probe 2; native TEST=%u; no input writes.\n",enableTestRow); std::fflush(log);
    char hash[65]{};
    const auto image = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    if (!hashExe(hash) || std::strcmp(hash, expectedHash) || !compatible(image) || !bindUi(image)) {
        std::fprintf(log, "Unsupported EXE. SHA256=%s. No hooks installed.\n", hash); std::fclose(log); return 1;
    }
    std::fprintf(log, "Verified EXE %s\n", hash);
    const auto init = MH_Initialize();
    void* create = image + createRva; void* attach = image + attachRva;
    MH_STATUS a = MH_UNKNOWN, b = MH_UNKNOWN, enabled = MH_UNKNOWN;
    if (init == MH_OK || init == MH_ERROR_ALREADY_INITIALIZED) {
        a = MH_CreateHook(create, reinterpret_cast<void*>(createHook), reinterpret_cast<void**>(&originalCreate));
        b = MH_CreateHook(attach, reinterpret_cast<void*>(attachHook), reinterpret_cast<void**>(&originalAttach));
        if (a == MH_OK && b == MH_OK && MH_QueueEnableHook(create) == MH_OK && MH_QueueEnableHook(attach) == MH_OK)
            enabled = MH_ApplyQueued();
    }
    std::fprintf(log, "Hooks init=%d create=%d attach=%d enabled=%d\n", init,a,b,enabled); std::fflush(log);
    if (enabled != MH_OK) {
        if (a == MH_OK) { MH_DisableHook(create); MH_RemoveHook(create); }
        if (b == MH_OK) { MH_DisableHook(attach); MH_RemoveHook(attach); }
        std::fclose(log); return 1;
    }
    unsigned consumed = 0, ticks = 0;
    for (;;) {
        while (consumed < capacity && ready[consumed].load(std::memory_order_acquire)) {
            const auto& e = events[consumed++];
            std::fprintf(log, "%u thread=%lu kind=%u symbol='%s' instance='%s' owner=%llx parent=%llx output=%llx type=%x iface=%llx object=%llx depth=%d ok=%u\n",
                consumed, static_cast<unsigned long>(e.thread),e.kind,e.symbol,e.instance,
                static_cast<unsigned long long>(e.owner),static_cast<unsigned long long>(e.parent),static_cast<unsigned long long>(e.output),
                e.type,static_cast<unsigned long long>(e.interfacePointer),static_cast<unsigned long long>(e.object),e.depth,e.success);
        }
        if (++ticks % 40 == 0) std::fprintf(log, "Heartbeat observed=%u queued=%u written=%u\n", observed.load(),claimed.load(),consumed);
        std::fflush(log); Sleep(250);
    }
}
}
#ifndef SCALEFORM_PROBE_TEST
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        probe::module = instance; DisableThreadLibraryCalls(instance);
        HANDLE thread = CreateThread(nullptr,0,probe::run,nullptr,0,nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
#endif
