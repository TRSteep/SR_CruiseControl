#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <atomic>
#include <mutex>
#include <string>
#include <algorithm>
#include <cstdio>
#include <vector>
#include "MinHook.h"
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "CruiseState.h"
#include "DrivingHud.h"
#include "PanelArtwork.h"

namespace {
HMODULE module;
std::wstring directory, iniPath;
std::mutex logMutex;
std::atomic<int> displayState{0}; // 0 off, 1 on, 2 input error
std::atomic<bool> ownsInput{false};
std::atomic<bool> stopping{false};
unsigned toggleKey = VK_F6;
float offsetX = 0, offsetY = 0, hudScale = 1;
bool hudVisible = true;
bool followNativeMotion = true;
float tiltOffset = 0;
HHOOK keyboardHook;
autopilot::KeyboardPolicy* keyboard;
constexpr ULONG_PTR throttleTag = 0x53524150;
std::atomic<bool> manualRequested{false}, cancelRequested{false};
std::atomic<WNDPROC> previousWindowProc{nullptr};
std::mutex hudAnchorMutex, hudSampleMutex;
autopilot::HudAnchor hudAnchor;
ULONGLONG hudAnchorTime = 0;
ULONGLONG hudObservationTime = 0;
std::atomic<bool> hudResetRequested{false};
std::atomic<ULONGLONG> hudReacquireAfter{0};
std::atomic<unsigned> hudGeneration{0};
HANDLE hudSampleEvent;
struct HudSample {
    std::vector<unsigned char> pixels;
    float left = 0, top = 0, scale = 1;
    unsigned generation = 0;
    ULONGLONG stamp = 0;
};
HudSample pendingHudSample;
void log(const char* message);

autopilot::HudAnchor currentHudAnchor() {
    std::lock_guard<std::mutex> lock(hudAnchorMutex);
    autopilot::HudAnchor result = hudAnchor;
    result.visible = autopilot::drivingHudAvailable(result.visible, hudAnchorTime, GetTickCount64());
    return result;
}

bool hudCanActivate() {
    std::lock_guard<std::mutex> lock(hudAnchorMutex);
    const ULONGLONG now = GetTickCount64();
    return hudAnchor.visible && now >= hudAnchorTime && now - hudAnchorTime <= 500;
}

void resetHudTracking(ULONGLONG cooldown = 0) {
    ++hudGeneration;
    if (cooldown) hudReacquireAfter = GetTickCount64() + cooldown;
    std::lock_guard<std::mutex> lock(hudAnchorMutex);
    hudAnchor = {}; hudAnchorTime = hudObservationTime = 0;
}

bool menuKey(unsigned vk, unsigned scan) {
    return scan == 0x32 || vk == VK_ESCAPE || vk == VK_RETURN || vk == VK_F1 || vk == VK_F4;
}

DWORD WINAPI observeDrivingHud(void*) {
    autopilot::DrivingHudDetector detector;
    autopilot::StableDrivingHud tracker;
    autopilot::NativeHudMotion motion;
    unsigned generation = hudGeneration.load();
    bool wasVisible = false;
    while (!stopping.load()) {
        if (WaitForSingleObject(hudSampleEvent, 500) != WAIT_OBJECT_0) continue;
        HudSample sample;
        { std::lock_guard<std::mutex> lock(hudSampleMutex); std::swap(sample, pendingHudSample); }
        if (sample.pixels.empty() || sample.generation != hudGeneration.load() || sample.stamp < hudReacquireAfter.load()) continue;
        if (sample.generation != generation) { tracker.reset(); motion.reset(); generation = sample.generation; }
        auto match = detector.find(sample.pixels.data());
        match.x = sample.left + match.x * sample.scale;
        match.y = sample.top + match.y * sample.scale;
        match.scale *= sample.scale;
        tracker.observe(match, sample.stamp);
        const auto stable = tracker.current(sample.stamp);
        const auto visual = followNativeMotion ? motion.update(match, stable) : stable;
        if (sample.generation != hudGeneration.load()) continue;
        {
            std::lock_guard<std::mutex> lock(hudAnchorMutex);
            if (sample.generation != hudGeneration.load()) continue;
            hudAnchor = visual; hudAnchorTime = tracker.lastAcceptedAt(); hudObservationTime = sample.stamp;
        }
        // Log transitions only; never save gameplay frames or arbitrary key input.
        if (wasVisible != stable.visible) {
            char message[128];
            std::snprintf(message, sizeof(message), "Stable driving HUD %s (confidence %.3f).", stable.visible ? "visible" : "hidden", match.confidence);
            log(message);
            wasVisible = stable.visible;
        }
    }
    return 0;
}

void log(const char* message) {
    std::lock_guard<std::mutex> lock(logMutex);
    FILE* file = _wfopen((directory + L"SnowRunnerAutopilot.log").c_str(), L"a");
    if (!file) return;
    SYSTEMTIME time; GetLocalTime(&time);
    std::fprintf(file, "%04u-%02u-%02u %02u:%02u:%02u %s\n", time.wYear, time.wMonth,
        time.wDay, time.wHour, time.wMinute, time.wSecond, message);
    std::fclose(file);
}
bool isDown(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; }
bool gameFocused() {
#ifdef AUTOPILOT_RENDER_TEST
    return true; // Offscreen graphics test only; absent from the ASI build.
#else
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
#endif
}
bool modifiersDown() {
    return isDown(VK_SHIFT) || isDown(VK_CONTROL) || isDown(VK_MENU) || isDown(VK_LWIN) || isDown(VK_RWIN);
}
bool sendThrottle(bool down) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = 0x11;
    input.ki.dwFlags = KEYEVENTF_SCANCODE | (down ? 0 : KEYEVENTF_KEYUP);
    input.ki.dwExtraInfo = throttleTag;
    return SendInput(1, &input, sizeof(input)) == 1;
}
autopilot::CruiseState cruise(sendThrottle); // Accessed only on the input thread.
void publishState() {
    ownsInput = cruise.ownsThrottle;
    displayState = cruise.inputFailed ? 2 : cruise.active ? 1 : 0;
}
LRESULT CALLBACK keyboardProc(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION && keyboard) {
        const auto& key = *reinterpret_cast<KBDLLHOOKSTRUCT*>(data);
        if (!(key.flags & (LLKHF_INJECTED | LLKHF_UP)) && menuKey(key.vkCode, key.scanCode)) hudResetRequested = true;
        const bool swallow = keyboard->handle(key.vkCode, key.scanCode, key.flags,
            gameFocused(), isDown(MapVirtualKeyW(0x1f, MAPVK_VSC_TO_VK_EX)), modifiersDown());
        publishState();
        if (swallow) return 1;
    }
    return CallNextHookEx(keyboardHook, code, message, data);
}

LRESULT CALLBACK gameWindowProc(HWND window, UINT message, WPARAM vk, LPARAM data) {
    if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) &&
        static_cast<ULONG_PTR>(GetMessageExtraInfo()) != throttleTag) {
        const unsigned scan = (static_cast<ULONG_PTR>(data) >> 16) & 0xff;
        const bool extended = (static_cast<ULONG_PTR>(data) & (1u << 24)) != 0;
        if (menuKey(static_cast<unsigned>(vk), scan)) hudResetRequested = true;
        if (scan == 0x11 && !extended) manualRequested = true;
        if (scan == 0x1f || scan == 0x31 || scan == 0x32 || vk == VK_ESCAPE || vk == VK_RETURN || vk == VK_F1 || vk == VK_F4)
            cancelRequested = true;
    }
    // The low-level hook must forward F6 so Windows updates its async state.
    // Swallow only the game's window messages, after that update has happened.
    if ((message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP)
        && vk == toggleKey && !modifiersDown() && (displayState.load() == 1 || hudCanActivate())) return 0;
    const WNDPROC original = previousWindowProc.load();
    return original ? CallWindowProcW(original, window, message, vk, data) : DefWindowProcW(window, message, vk, data);
}

void pollInput(autopilot::ToggleEdge& toggle) {
    if (hudResetRequested.exchange(false)) resetHudTracking(500);
    autopilot::CruiseInput input;
    input.focused = gameFocused(); input.drivingHud = hudCanActivate();
    input.brake = isDown(MapVirtualKeyW(0x1f, MAPVK_VSC_TO_VK_EX));
    input.engine = isDown(MapVirtualKeyW(0x31, MAPVK_VSC_TO_VK_EX));
    input.cancel = cancelRequested.exchange(false) || isDown(VK_ESCAPE) || isDown(VK_RETURN) || isDown(VK_F1) || isDown(VK_F4);
    input.manual = manualRequested.exchange(false); input.toggle = isDown(toggleKey); input.modifiers = modifiersDown();
    const bool wasActive = cruise.active;
    if (autopilot::updateCruiseInput(cruise, toggle, input)) {
        log(cruise.active ? "F-key detected: autopilot ON." : cruise.inputFailed ? "F-key detected: input failed." :
            input.modifiers || input.brake || cruise.ownsThrottle ? "F-key detected: activation blocked by brake/modifiers/pending release." : "F-key detected: autopilot OFF.");
    }
    if (wasActive && !cruise.active) log("Autopilot cancelled by explicit input or focus (not HUD recognition).");
    publishState();
}

void loadConfig() {
    toggleKey = GetPrivateProfileIntW(L"Autopilot", L"ToggleKey", VK_F6, iniPath.c_str());
    if (toggleKey < VK_F2 || toggleKey > VK_F12 || toggleKey == VK_F4) toggleKey = VK_F6;
    offsetX = static_cast<float>(std::clamp(static_cast<int>(GetPrivateProfileIntW(L"HUD", L"OffsetX", 0, iniPath.c_str())), -1000, 1000));
    offsetY = static_cast<float>(std::clamp(static_cast<int>(GetPrivateProfileIntW(L"HUD", L"OffsetY", 0, iniPath.c_str())), -1000, 1000));
    hudScale = std::clamp(GetPrivateProfileIntW(L"HUD", L"ScalePercent", 100, iniPath.c_str()) / 100.f, .5f, 2.f);
    hudVisible = GetPrivateProfileIntW(L"HUD", L"Visible", 1, iniPath.c_str()) != 0;
    followNativeMotion = GetPrivateProfileIntW(L"HUD", L"FollowNativeMotion", 1, iniPath.c_str()) != 0;
    tiltOffset = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"HUD", L"TiltOffsetDegrees", 0, iniPath.c_str())), -15, 15) * .0174532925f;
}
void saveHud() {
    auto write = [](const wchar_t* key, int value) {
        const std::wstring text = std::to_wstring(value);
        WritePrivateProfileStringW(L"HUD", key, text.c_str(), iniPath.c_str());
    };
    write(L"OffsetX", static_cast<int>(offsetX));
    write(L"OffsetY", static_cast<int>(offsetY));
    write(L"ScalePercent", static_cast<int>(hudScale * 100 + .5f));
    write(L"Visible", hudVisible ? 1 : 0);
    write(L"FollowNativeMotion", followNativeMotion ? 1 : 0);
    write(L"TiltOffsetDegrees", static_cast<int>(std::round(tiltOffset * 57.29578f)));
}

using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using ResizeFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
PresentFn originalPresent;
ResizeFn originalResize;
std::mutex renderMutex;
IDXGISwapChain* selectedChain;
ID3D11Device* device;
ID3D11DeviceContext* context;
ID3D11RenderTargetView* renderTarget;
ImGuiContext* imgui;
ID3D11ShaderResourceView* panelTexture;
autopilot::HudPoseSmoother hudPoseSmoother;
unsigned drawnHudGeneration = 0;
UINT renderWidth, renderHeight;
ID3D11Texture2D* hudReadback[2]{};
bool hudReadbackPending[2]{};
ULONGLONG hudReadbackTime[2]{}, lastHudCapture = 0;
unsigned hudReadbackGeneration[2]{};
int hudReadbackWidth = 0, hudReadbackHeight = 0;

bool createPanelTexture() {
    const auto pixels = autopilot::makePanelArtwork();
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = 512; desc.Height = 112; desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{}; data.pSysMem = pixels.data(); data.SysMemPitch = desc.Width * 4;
    ID3D11Texture2D* texture = nullptr;
    if (FAILED(device->CreateTexture2D(&desc, &data, &texture))) return false;
    const HRESULT result = device->CreateShaderResourceView(texture, nullptr, &panelTexture);
    texture->Release();
    return SUCCEEDED(result);
}

void releaseHudReadback() {
    for (int i = 0; i < 2; ++i) {
        if (hudReadback[i]) { hudReadback[i]->Release(); hudReadback[i] = nullptr; }
        hudReadbackPending[i] = false;
    }
    hudReadbackWidth = hudReadbackHeight = 0;
    resetHudTracking();
}

void captureDrivingHud(IDXGISwapChain* chain) {
    if (!hudSampleEvent || renderWidth < 640 || renderHeight < 360) return;
    const float scale = renderHeight / 1080.f;
    const float left = renderWidth - autopilot::DrivingHudDetector::cropRight * scale;
    const float top = renderHeight - autopilot::DrivingHudDetector::cropBottom * scale;
    const UINT x = static_cast<UINT>(std::max(0.f, std::floor(left)));
    const UINT y = static_cast<UINT>(std::max(0.f, std::floor(top)));
    const UINT width = static_cast<UINT>(std::ceil(autopilot::DrivingHudDetector::cropWidth * scale)) + 1;
    const UINT height = static_cast<UINT>(std::ceil(autopilot::DrivingHudDetector::cropHeight * scale)) + 1;
    if (x + width > renderWidth || y + height > renderHeight) return;
    ID3D11Texture2D* buffer = nullptr;
    if (FAILED(chain->GetBuffer(0, IID_PPV_ARGS(&buffer)))) return;
    D3D11_TEXTURE2D_DESC desc{}; buffer->GetDesc(&desc);
    const bool bgra = desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    const bool rgba = desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    if ((!bgra && !rgba) || desc.SampleDesc.Count != 1) { buffer->Release(); return; }
    if (hudReadbackWidth != static_cast<int>(width) || hudReadbackHeight != static_cast<int>(height)) {
        releaseHudReadback();
        desc.Width = width; desc.Height = height; desc.MipLevels = 1; desc.ArraySize = 1;
        desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
        for (auto& texture : hudReadback) {
            if (FAILED(device->CreateTexture2D(&desc, nullptr, &texture))) { releaseHudReadback(); buffer->Release(); return; }
        }
        hudReadbackWidth = width; hudReadbackHeight = height;
    }
    for (int i = 0; i < 2; ++i) {
        if (!hudReadbackPending[i]) continue;
        D3D11_MAPPED_SUBRESOURCE map{};
        // Never wait for GPU readback in Present. Busy frames are retried later.
        if (FAILED(context->Map(hudReadback[i], 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &map))) continue;
        HudSample sample;
        sample.pixels.resize(autopilot::DrivingHudDetector::cropWidth * autopilot::DrivingHudDetector::cropHeight);
        sample.left = left; sample.top = top; sample.scale = scale;
        sample.generation = hudReadbackGeneration[i]; sample.stamp = hudReadbackTime[i];
        auto gray = [&](int px, int py) {
            px = std::clamp(px, 0, static_cast<int>(width) - 1); py = std::clamp(py, 0, static_cast<int>(height) - 1);
            const auto* pixel = static_cast<const unsigned char*>(map.pData) + py * map.RowPitch + px * 4;
            return (77 * pixel[bgra ? 2 : 0] + 150 * pixel[1] + 29 * pixel[bgra ? 0 : 2]) / 256.f;
        };
        for (int cy = 0; cy < autopilot::DrivingHudDetector::cropHeight; ++cy)
            for (int cx = 0; cx < autopilot::DrivingHudDetector::cropWidth; ++cx) {
                const float sx = left - x + (cx + .5f) * scale - .5f;
                const float sy = top - y + (cy + .5f) * scale - .5f;
                const int ix = static_cast<int>(std::floor(sx)), iy = static_cast<int>(std::floor(sy));
                const float fx = sx - ix, fy = sy - iy;
                sample.pixels[cy * autopilot::DrivingHudDetector::cropWidth + cx] = static_cast<unsigned char>(
                    (gray(ix, iy) * (1 - fx) + gray(ix + 1, iy) * fx) * (1 - fy) +
                    (gray(ix, iy + 1) * (1 - fx) + gray(ix + 1, iy + 1) * fx) * fy);
            }
        context->Unmap(hudReadback[i], 0); hudReadbackPending[i] = false;
        { std::lock_guard<std::mutex> lock(hudSampleMutex); std::swap(pendingHudSample, sample); }
        SetEvent(hudSampleEvent);
    }
    if (GetTickCount64() - lastHudCapture >= 120) {
        for (int i = 0; i < 2; ++i) if (!hudReadbackPending[i]) {
            const D3D11_BOX box{x, y, 0, x + width, y + height, 1};
            context->CopySubresourceRegion(hudReadback[i], 0, 0, 0, 0, buffer, 0, &box);
            hudReadbackTime[i] = lastHudCapture = GetTickCount64(); hudReadbackPending[i] = true;
            hudReadbackGeneration[i] = hudGeneration.load();
            break;
        }
    }
    buffer->Release();
}

bool makeTarget(IDXGISwapChain* chain) {
    ID3D11Texture2D* buffer = nullptr;
    if (FAILED(chain->GetBuffer(0, IID_PPV_ARGS(&buffer)))) return false;
    D3D11_TEXTURE2D_DESC desc{};
    buffer->GetDesc(&desc); renderWidth = desc.Width; renderHeight = desc.Height;
    const HRESULT result = device->CreateRenderTargetView(buffer, nullptr, &renderTarget);
    buffer->Release();
    return SUCCEEDED(result);
}
bool initializeRenderer(IDXGISwapChain* chain) {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(chain->GetDesc(&desc)) || !IsWindow(desc.OutputWindow)) return false;
    if (FAILED(chain->GetDevice(IID_PPV_ARGS(&device)))) return false;
    device->GetImmediateContext(&context);
    imgui = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;
    wchar_t windows[MAX_PATH]{};
    GetWindowsDirectoryW(windows, MAX_PATH);
    std::wstring fontPath = directory + L"TTLakesCompressed-Medium.ttf";
    if (GetFileAttributesW(fontPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        fontPath = std::wstring(windows) + L"\\Fonts\\segoeui.ttf";
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, fontPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(bytes, '\0');
    WideCharToMultiByte(CP_UTF8, 0, fontPath.c_str(), -1, &utf8[0], bytes, nullptr, nullptr);
    if (!io.Fonts->AddFontFromFileTTF(utf8.c_str(), 20.f, nullptr, io.Fonts->GetGlyphRangesCyrillic()))
        io.Fonts->AddFontDefault();
    const bool win32 = ImGui_ImplWin32_Init(desc.OutputWindow);
    const bool dx11 = win32 && ImGui_ImplDX11_Init(device, context);
    const bool artwork = dx11 && createPanelTexture();
    if (!artwork || !makeTarget(chain)) {
        if (panelTexture) { panelTexture->Release(); panelTexture = nullptr; }
        if (dx11) ImGui_ImplDX11_Shutdown();
        if (win32) ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(imgui); imgui = nullptr;
        context->Release(); context = nullptr;
        device->Release(); device = nullptr;
        log("Renderer initialization failed; will retry.");
        return false;
    }
    selectedChain = chain;
    previousWindowProc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(desc.OutputWindow, GWLP_WNDPROC));
    SetLastError(0);
    if (!SetWindowLongPtrW(desc.OutputWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(gameWindowProc)) && GetLastError())
        log("Window input fallback unavailable; async F-key polling remains enabled.");
    log("D3D11 HUD initialized.");
    return true;
}

bool pressed(int key) {
    static bool previous[256]{};
    const bool down = isDown(key);
    const bool edge = down && !previous[key];
    previous[key] = down;
    return edge;
}
void renderHud() {
    const bool left = pressed(VK_LEFT), right = pressed(VK_RIGHT);
    const bool up = pressed(VK_UP), down = pressed(VK_DOWN);
    const bool larger = pressed(VK_PRIOR), smaller = pressed(VK_NEXT), hide = pressed('H');
    if (isDown(VK_CONTROL) && isDown(VK_MENU)) {
        offsetX = std::clamp(offsetX - (left ? 2.f : 0) + (right ? 2.f : 0), -1000.f, 1000.f);
        offsetY = std::clamp(offsetY - (up ? 2.f : 0) + (down ? 2.f : 0), -1000.f, 1000.f);
        hudScale = std::clamp(hudScale + (larger ? .1f : 0) - (smaller ? .1f : 0), .5f, 2.f);
        if (hide) hudVisible = !hudVisible;
        if (left || right || up || down || larger || smaller || hide) saveHud();
    }
    const auto target = currentHudAnchor();
    if (drawnHudGeneration != hudGeneration.load()) { hudPoseSmoother.reset(); drawnHudGeneration = hudGeneration.load(); }
    if (!hudVisible || !target.visible) { hudPoseSmoother.reset(); return; }
    const ImGuiIO& io = ImGui::GetIO();
    const auto anchor = followNativeMotion ? hudPoseSmoother.advance(target, io.DeltaTime) : target;
    const float scale = std::clamp(io.DisplaySize.y / 1080.f, .5f, 3.f);
    const float textScale = anchor.scale * hudScale;
    const float width = 202 * textScale, height = 40 * textScale;
    const float x = std::clamp(anchor.x - 178 * anchor.scale + offsetX * scale, 0.f, std::max(0.f, io.DisplaySize.x - 235 * textScale));
    const float y = std::clamp(anchor.y - (20 + 48) * anchor.scale + offsetY * scale, 0.f, std::max(0.f, io.DisplaySize.y - height));
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const int firstVertex = draw->VtxBuffer.Size;
    const int state = displayState.load();
    draw->AddImage(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(panelTexture)),
        ImVec2(x - 12 * textScale, y - 3 * textScale), ImVec2(x + width + 4 * textScale, y + height + 3 * textScale),
        ImVec2(0, 0), ImVec2(1, 1), IM_COL32(10, 13, 12, state == 1 ? 235 : 165));
    const ImU32 color = state == 2 ? IM_COL32(239, 116, 78, 235) : state == 1 ? IM_COL32(213, 224, 122, 235) : IM_COL32(210, 213, 209, 200);
    draw->AddText(ImGui::GetFont(), (state == 2 ? 17 : 20) * textScale,
        ImVec2(x + 19 * textScale, y + 8 * textScale), color, state == 2 ? "ОШИБКА ВВОДА" : "АВТОПИЛОТ");
    const ImVec2 center(x + 178 * textScale, y + 20 * textScale);
    draw->PathArcTo(center, 11 * textScale, 2.35f, 7.08f, 22); draw->PathStroke(color, 0, 2 * textScale);
    draw->AddLine(center, ImVec2(center.x + 6 * textScale, center.y - 6 * textScale), color, 2 * textScale);
    draw->AddCircleFilled(center, 2 * textScale, color);
    const ImVec2 keyPos(x + 207 * textScale, y + 8 * textScale);
    draw->AddRectFilled(keyPos, ImVec2(keyPos.x + 24 * textScale, keyPos.y + 24 * textScale), IM_COL32(216, 222, 222, 200), 2 * textScale);
    char keyLabel[8]; std::snprintf(keyLabel, sizeof(keyLabel), "F%u", toggleKey - VK_F1 + 1);
    const ImVec2 keySize = ImGui::GetFont()->CalcTextSizeA(16 * textScale, FLT_MAX, 0, keyLabel);
    draw->AddText(ImGui::GetFont(), 16 * textScale, ImVec2(keyPos.x + (24 * textScale - keySize.x) / 2,
        keyPos.y + (24 * textScale - keySize.y) / 2), IM_COL32(43, 55, 59, 255), keyLabel);
    // Rotate the complete row together: texture, text, gauge and key hint.
    const float angle = std::clamp(anchor.angle + tiltOffset, -.26f, .26f);
    const float cosine = std::cos(angle), sine = std::sin(angle);
    const ImVec2 pivot(anchor.x + offsetX * scale, anchor.y + offsetY * scale);
    for (int i = firstVertex; i < draw->VtxBuffer.Size; ++i) {
        const float vx = draw->VtxBuffer[i].pos.x - pivot.x, vy = draw->VtxBuffer[i].pos.y - pivot.y;
        draw->VtxBuffer[i].pos = ImVec2(pivot.x + vx * cosine - vy * sine, pivot.y + vx * sine + vy * cosine);
    }
}

HRESULT STDMETHODCALLTYPE presentHook(IDXGISwapChain* chain, UINT sync, UINT flags) {
    if (!(flags & DXGI_PRESENT_TEST) && !stopping.load()) {
        std::lock_guard<std::mutex> lock(renderMutex);
        ImGuiContext* previous = ImGui::GetCurrentContext();
        if ((!imgui && initializeRenderer(chain)) || (imgui && chain == selectedChain)) {
            ImGui::SetCurrentContext(imgui);
            captureDrivingHud(chain); // Read the game's frame before our own row is drawn.
            if ((renderTarget || makeTarget(chain)) && gameFocused()) {
                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();
                // Use the actual render buffer, including after resolution/DPI changes.
                ImGui::GetIO().DisplaySize = ImVec2(static_cast<float>(renderWidth), static_cast<float>(renderHeight));
                ImGui::NewFrame();
                renderHud();
                ImGui::Render();
                ID3D11RenderTargetView* oldTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
                ID3D11DepthStencilView* oldDepth = nullptr;
                context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldTargets, &oldDepth);
                context->OMSetRenderTargets(1, &renderTarget, nullptr);
                ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, oldTargets, oldDepth);
                for (auto* target : oldTargets) if (target) target->Release();
                if (oldDepth) oldDepth->Release();
            }
        }
        ImGui::SetCurrentContext(previous);
    }
    return originalPresent(chain, sync, flags);
}
HRESULT STDMETHODCALLTYPE resizeHook(IDXGISwapChain* chain, UINT count, UINT width, UINT height, DXGI_FORMAT format, UINT flags) {
    std::lock_guard<std::mutex> lock(renderMutex);
    ImGuiContext* previous = ImGui::GetCurrentContext();
    if (chain == selectedChain && imgui) {
        ImGui::SetCurrentContext(imgui);
        ImGui_ImplDX11_InvalidateDeviceObjects();
        releaseHudReadback();
        if (renderTarget) { renderTarget->Release(); renderTarget = nullptr; }
    }
    const HRESULT result = originalResize(chain, count, width, height, format, flags);
    ImGui::SetCurrentContext(previous);
    return result;
}

bool installRenderHooks() {
    const wchar_t* name = L"SnowRunnerAutopilotProbe";
    WNDCLASSW wc{}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = module; wc.lpszClassName = name;
    if (!RegisterClassW(&wc)) return false;
    HWND window = CreateWindowExW(0, name, L"", WS_OVERLAPPED, 0, 0, 100, 100, nullptr, nullptr, module, nullptr);
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferCount = 1; desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = window; desc.SampleDesc.Count = 1; desc.Windowed = TRUE;
    IDXGISwapChain* chain = nullptr;
    ID3D11Device* probeDevice = nullptr;
    ID3D11DeviceContext* probeContext = nullptr;
    HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &desc, &chain, &probeDevice, nullptr, &probeContext);
    if (FAILED(result)) result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &desc, &chain, &probeDevice, nullptr, &probeContext);
    bool installed = false;
    if (SUCCEEDED(result)) {
        void** table = *reinterpret_cast<void***>(chain);
        const MH_STATUS init = MH_Initialize();
        if ((init == MH_OK || init == MH_ERROR_ALREADY_INITIALIZED) &&
            MH_CreateHook(table[8], reinterpret_cast<void*>(&presentHook), reinterpret_cast<void**>(&originalPresent)) == MH_OK &&
            MH_CreateHook(table[13], reinterpret_cast<void*>(&resizeHook), reinterpret_cast<void**>(&originalResize)) == MH_OK) {
            // Queue only our hooks: never enable or disable hooks owned by other mods.
            MH_QueueEnableHook(table[8]); MH_QueueEnableHook(table[13]);
            installed = MH_ApplyQueued() == MH_OK;
        }
    }
    if (probeContext) probeContext->Release();
    if (probeDevice) probeDevice->Release();
    if (chain) chain->Release();
    if (window) DestroyWindow(window);
    UnregisterClassW(name, module);
    log(installed ? "D3D11 Present/Resize hooks installed." : "D3D11 hooks unavailable; autopilot input will remain disabled.");
    return installed;
}

DWORD WINAPI run(void*) {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(module, path, MAX_PATH);
    directory = path;
    directory.resize(directory.find_last_of(L"\\/") + 1);
    iniPath = directory + L"SnowRunnerAutopilot.ini";
    log("Native autopilot v5 starting (ragged tilted row; smoothed native HUD motion; independent throttle).");
    loadConfig();
    hudSampleEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!hudSampleEvent) { log("HUD observer event failed; input stays disabled."); return 1; }
    HANDLE observer = CreateThread(nullptr, 0, observeDrivingHud, nullptr, 0, nullptr);
    if (!observer) { log("HUD observer thread failed; input stays disabled."); return 1; }
    CloseHandle(observer);
    if (!installRenderHooks()) return 1;
    // Activation is polled independently of the global keyboard hook. A game
    // startup stall can cause Windows to silently remove a low-level hook.
    autopilot::KeyboardPolicy policy(cruise, 0);
    keyboard = &policy;
    keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardProc, module, 0);
    if (!keyboardHook) log("Low-level hook unavailable; using window input and async polling.");
    autopilot::ToggleEdge toggle(isDown(toggleKey));
    log("Input ready: independent F-key polling, W/S/N window fallback.");
    MSG message{};
    bool quit = false;
    while (!quit) {
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 8, QS_ALLINPUT);
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) { quit = true; break; }
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        if (!quit) pollInput(toggle);
    }
    cruise.cancel(); publishState();
    if (keyboardHook) UnhookWindowsHookEx(keyboardHook);
    keyboard = nullptr;
    log("Input thread stopped.");
    return 0;
}
}

#ifndef AUTOPILOT_RENDER_TEST
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        module = instance;
        DisableThreadLibraryCalls(instance);
        // Work starts after the loader lock is released; never wait in DllMain.
        HANDLE thread = CreateThread(nullptr, 0, run, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    } else if (reason == DLL_PROCESS_DETACH) {
        stopping = true;
        if (hudSampleEvent) SetEvent(hudSampleEvent);
        // Only SendInput here, no thread joins, UI, locks or graphics cleanup.
        if (ownsInput.load()) sendThrottle(false);
    }
    return TRUE;
}
#endif
