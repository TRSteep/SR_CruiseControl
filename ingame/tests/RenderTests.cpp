#include "GpuTestUtils.h"

int main() {
    module = GetModuleHandleW(nullptr);
    directory = L"build\\";
    iniPath = directory + L"RenderTest.ini";
    require(installRenderHooks(), "Install actual DX11 hooks");
    WNDCLASSW wc{}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = module; wc.lpszClassName = L"AutopilotOffscreenTest";
    require(RegisterClassW(&wc) != 0, "Register test window");
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"Autopilot offscreen test", WS_OVERLAPPED,
        0, 0, 1280, 720, nullptr, nullptr, module, nullptr);
    require(window != nullptr, "Create hidden test window");
    DXGI_SWAP_CHAIN_DESC desc{};
    desc.BufferDesc.Width = 1280; desc.BufferDesc.Height = 720;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.BufferCount = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE; desc.OutputWindow = window; desc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
    IDXGISwapChain* chain = nullptr; ID3D11Device* hostDevice = nullptr; ID3D11DeviceContext* hostContext = nullptr;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &desc, &chain, &hostDevice, nullptr, &hostContext);
    if (FAILED(hr)) hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &desc, &chain, &hostDevice, nullptr, &hostContext);
    require(SUCCEEDED(hr), "Create DX11 test device");
    bool simulatedDriving = true, staleHud = false;
    auto frame = [&] {
        ID3D11Texture2D* buffer = nullptr; chain->GetBuffer(0, IID_PPV_ARGS(&buffer));
        ID3D11RenderTargetView* target = nullptr;
        hostDevice->CreateRenderTargetView(buffer, nullptr, &target); buffer->Release();
        const float color[] = {.1f, .15f, .2f, 1}; hostContext->ClearRenderTargetView(target, color); target->Release();
        D3D11_TEXTURE2D_DESC size{};
        ID3D11Texture2D* current = nullptr; chain->GetBuffer(0, IID_PPV_ARGS(&current)); current->GetDesc(&size); current->Release();
        const float scale = size.Height / 1080.f;
        { std::lock_guard<std::mutex> lock(hudAnchorMutex);
            hudAnchor = {simulatedDriving, size.Width - 95 * scale, size.Height - 345 * scale, scale, 1};
            hudAnchorTime = GetTickCount64() - (staleHud ? 3000 : 0);
        }
        chain->Present(0, 0);
    };
    for (int state = 0; state < 3; ++state) {
        displayState = state; frame(); frame();
        require(imgui && selectedChain == chain, "Real Present hook initializes renderer");
        ImGui::SetCurrentContext(imgui);
        const auto& vertices = ImGui::GetForegroundDrawList()->VtxBuffer;
        require(vertices.Size >= 4 && vertices[1].pos.y > vertices[0].pos.y + 1,
            "The complete row's textured background must inherit the native tilt");
        ImGui::SetCurrentContext(nullptr);
        const auto path = directory + L"hud-state-" + std::to_wstring(state) + L".bmp";
        require(capture(chain, path.c_str()) > 150, "HUD text has visible pixels");
    }
    hudVisible = false; frame();
    require(capture(chain, (directory + L"hud-hidden.bmp").c_str()) == 0, "Hidden HUD draws no pixels");
    hudVisible = true;
    simulatedDriving = false; frame();
    require(capture(chain, (directory + L"hud-outside-truck.bmp").c_str()) == 0, "HUD is absent outside driving mode");
    simulatedDriving = true; staleHud = true; frame();
    require(capture(chain, (directory + L"hud-stale.bmp").c_str()) == 0, "Expired driving HUD never leaves floating overlay");
    staleHud = false;
    hostContext->OMSetRenderTargets(0, nullptr, nullptr);
    require(SUCCEEDED(chain->ResizeBuffers(1, 1920, 1080, DXGI_FORMAT_UNKNOWN, 0)), "Actual ResizeBuffers hook releases render target");
    frame(); frame();
    require(capture(chain, (directory + L"hud-1080.bmp").c_str()) > 300, "HUD renders after resize");
    stopping = true;
    std::puts("PASS: DX11 hooks, 3 row states, driving-only visibility, stale expiry, 720p/1080p resize");
    // The plugin intentionally lives until process exit. Do not unload it.
    return 0;
}
