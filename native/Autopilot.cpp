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
#include "MinHook.h"
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "CruiseState.h"

namespace {
HMODULE module;
std::wstring directory, iniPath;
std::mutex logMutex;
std::atomic<int> displayState{0}; // 0 off, 1 on, 2 input error
std::atomic<bool> ownsInput{false};
std::atomic<bool> stopping{false};
unsigned toggleKey = VK_F6;
float rightMargin = 40, bottomMargin = 430, hudScale = 1;
bool hudVisible = true;
HHOOK keyboardHook;
autopilot::KeyboardPolicy* keyboard;
constexpr ULONG_PTR throttleTag = 0x53524150;
std::atomic<bool> manualRequested{false}, cancelRequested{false};
std::atomic<WNDPROC> previousWindowProc{nullptr};

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
        if (scan == 0x11 && !extended) manualRequested = true;
        if (scan == 0x1f || scan == 0x31 || scan == 0x32 || vk == VK_ESCAPE || vk == VK_RETURN || vk == VK_F1 || vk == VK_F4)
            cancelRequested = true;
    }
    // The low-level hook must forward F6 so Windows updates its async state.
    // Swallow only the game's window messages, after that update has happened.
    if ((message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP)
        && vk == toggleKey && !modifiersDown()) return 0;
    const WNDPROC original = previousWindowProc.load();
    return original ? CallWindowProcW(original, window, message, vk, data) : DefWindowProcW(window, message, vk, data);
}

void pollInput(autopilot::ToggleEdge& toggle) {
    const bool focused = gameFocused();
    const bool brake = isDown(MapVirtualKeyW(0x1f, MAPVK_VSC_TO_VK_EX));
    if (manualRequested.exchange(false)) cruise.manualThrottle();
    if (cancelRequested.exchange(false) || !focused || brake || isDown(MapVirtualKeyW(0x31, MAPVK_VSC_TO_VK_EX)) ||
        isDown(VK_ESCAPE) || isDown(VK_RETURN) || isDown(VK_F1) || isDown(VK_F4)) {
        if (cruise.active) log("Autopilot cancelled by brake/engine/menu/focus.");
        cruise.cancel();
    }
    const bool modifiers = modifiersDown();
    if (autopilot::pollToggle(cruise, toggle, isDown(toggleKey), focused, brake, modifiers)) {
        log(cruise.active ? "F-key detected: autopilot ON." : cruise.inputFailed ? "F-key detected: input failed." :
            modifiers || brake || cruise.ownsThrottle ? "F-key detected: activation blocked by brake/modifiers/pending release." : "F-key detected: autopilot OFF.");
    }
    cruise.retryRelease();
    publishState();
}

void loadConfig() {
    toggleKey = GetPrivateProfileIntW(L"Autopilot", L"ToggleKey", VK_F6, iniPath.c_str());
    if (toggleKey < VK_F2 || toggleKey > VK_F12 || toggleKey == VK_F4) toggleKey = VK_F6;
    rightMargin = static_cast<float>(std::min(3000u, GetPrivateProfileIntW(L"HUD", L"Right", 40, iniPath.c_str())));
    bottomMargin = static_cast<float>(std::min(2000u, GetPrivateProfileIntW(L"HUD", L"Bottom", 430, iniPath.c_str())));
    hudScale = std::clamp(GetPrivateProfileIntW(L"HUD", L"ScalePercent", 100, iniPath.c_str()) / 100.f, .5f, 2.f);
    hudVisible = GetPrivateProfileIntW(L"HUD", L"Visible", 1, iniPath.c_str()) != 0;
}
void saveHud() {
    auto write = [](const wchar_t* key, int value) {
        const std::wstring text = std::to_wstring(value);
        WritePrivateProfileStringW(L"HUD", key, text.c_str(), iniPath.c_str());
    };
    write(L"Right", static_cast<int>(rightMargin));
    write(L"Bottom", static_cast<int>(bottomMargin));
    write(L"ScalePercent", static_cast<int>(hudScale * 100 + .5f));
    write(L"Visible", hudVisible ? 1 : 0);
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
UINT renderWidth, renderHeight;

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
    std::wstring fontPath = std::wstring(windows) + L"\\Fonts\\segoeui.ttf";
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, fontPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(bytes, '\0');
    WideCharToMultiByte(CP_UTF8, 0, fontPath.c_str(), -1, &utf8[0], bytes, nullptr, nullptr);
    if (!io.Fonts->AddFontFromFileTTF(utf8.c_str(), 20.f, nullptr, io.Fonts->GetGlyphRangesCyrillic()))
        io.Fonts->AddFontDefault();
    const bool win32 = ImGui_ImplWin32_Init(desc.OutputWindow);
    const bool dx11 = win32 && ImGui_ImplDX11_Init(device, context);
    if (!dx11 || !makeTarget(chain)) {
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
        rightMargin = std::clamp(rightMargin + (left ? 10.f : 0) - (right ? 10.f : 0), 0.f, 3000.f);
        bottomMargin = std::clamp(bottomMargin + (up ? 10.f : 0) - (down ? 10.f : 0), 0.f, 2000.f);
        hudScale = std::clamp(hudScale + (larger ? .1f : 0) - (smaller ? .1f : 0), .5f, 2.f);
        if (hide) hudVisible = !hudVisible;
        if (left || right || up || down || larger || smaller || hide) saveHud();
    }
    if (!hudVisible) return;
    const ImGuiIO& io = ImGui::GetIO();
    const float scale = std::clamp(io.DisplaySize.y / 1080.f, .5f, 3.f);
    const float textScale = scale * hudScale;
    const float width = 256 * textScale, height = 65 * textScale;
    const float x = std::clamp(io.DisplaySize.x - width - rightMargin * scale, 0.f, std::max(0.f, io.DisplaySize.x - width));
    const float y = std::clamp(io.DisplaySize.y - height - bottomMargin * scale, 0.f, std::max(0.f, io.DisplaySize.y - height));
    const ImVec2 pos(x, y);
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(pos, ImVec2(x + width, y + height), IM_COL32(12, 17, 21, 190), 4 * textScale);
    const int state = displayState.load();
    const ImU32 color = state == 2 ? IM_COL32(255, 125, 90, 255) : state == 1 ? IM_COL32(128, 225, 151, 255) : IM_COL32(220, 224, 226, 255);
    draw->AddRectFilled(pos, ImVec2(x + 3 * textScale, y + height), color);
    char title[96];
    std::snprintf(title, sizeof(title), "АВТОПИЛОТ   [F%u]", toggleKey - VK_F1 + 1);
    draw->AddText(ImGui::GetFont(), 20 * textScale, ImVec2(x + 13 * textScale, y + 8 * textScale), IM_COL32_WHITE, title);
    draw->AddText(ImGui::GetFont(), 17 * textScale, ImVec2(x + 13 * textScale, y + 35 * textScale), color,
        state == 2 ? "ОШИБКА ВВОДА" : state == 1 ? "ВКЛЮЧЁН" : "ВЫКЛЮЧЕН");
}

HRESULT STDMETHODCALLTYPE presentHook(IDXGISwapChain* chain, UINT sync, UINT flags) {
    if (!(flags & DXGI_PRESENT_TEST) && !stopping.load()) {
        std::lock_guard<std::mutex> lock(renderMutex);
        ImGuiContext* previous = ImGui::GetCurrentContext();
        if ((!imgui && initializeRenderer(chain)) || (imgui && chain == selectedChain)) {
            ImGui::SetCurrentContext(imgui);
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
    log("Native autopilot v2 starting (x64, D3D11, async F-key polling).");
    loadConfig();
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
        // Only SendInput here, no thread joins, UI, locks or graphics cleanup.
        if (ownsInput.load()) sendThrottle(false);
    }
    return TRUE;
}
#endif
