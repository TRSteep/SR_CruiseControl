#include "GpuTestUtils.h"
#include <fstream>

struct Fixture {
    unsigned width = 1920, height = 1080;
    bool expected = false;
    std::vector<unsigned char> rgba;
};

static Fixture synthetic(bool driving) {
    Fixture fixture; fixture.expected = driving;
    fixture.rgba.resize(fixture.width * fixture.height * 4, 35);
    for (size_t i = 3; i < fixture.rgba.size(); i += 4) fixture.rgba[i] = 255;
    if (driving) for (int y = 0; y < 28; ++y) for (int x = 0; x < 36; ++x) {
        const unsigned char value = autopilot::markerReferences[0][(y / 2) * 18 + x / 2];
        const size_t offset = ((721 + y) * fixture.width + 1800 + x) * 4;
        fixture.rgba[offset] = fixture.rgba[offset + 1] = fixture.rgba[offset + 2] = value;
    }
    return fixture;
}

int main(int argc, char** argv) {
    std::vector<Fixture> fixtures;
    if (argc == 1) fixtures = {synthetic(true), synthetic(false), synthetic(true)};
    else for (int i = 1; i < argc; ++i) {
        Fixture fixture;
        std::ifstream file(argv[i], std::ios::binary);
        file.read(reinterpret_cast<char*>(&fixture.width), 4); file.read(reinterpret_cast<char*>(&fixture.height), 4);
        require(file && fixture.width <= 4096 && fixture.height <= 4096 && fixture.width >= 640 && fixture.height >= 360, "Invalid reference image");
        fixture.rgba.resize(fixture.width * fixture.height * 4);
        file.read(reinterpret_cast<char*>(fixture.rgba.data()), fixture.rgba.size());
        require(static_cast<bool>(file), "Truncated RGBA image");
        fixture.expected = std::string(argv[i]).find("load") == std::string::npos;
        fixtures.push_back(std::move(fixture));
    }
    module = GetModuleHandleW(nullptr); directory = L"build\\";
    require(installRenderHooks(), "Install real DX11 hooks");
    WNDCLASSW wc{}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = module; wc.lpszClassName = L"AutopilotGpuObserverTest";
    require(RegisterClassW(&wc) != 0, "Register test window");
    HWND window = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPED, 0, 0, 100, 100, nullptr, nullptr, module, nullptr);
    DXGI_SWAP_CHAIN_DESC desc{}; desc.BufferDesc.Width = fixtures[0].width; desc.BufferDesc.Height = fixtures[0].height;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.BufferCount = 1; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SampleDesc.Count = 1; desc.Windowed = TRUE; desc.OutputWindow = window; desc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;
    IDXGISwapChain* chain = nullptr; ID3D11Device* hostDevice = nullptr; ID3D11DeviceContext* hostContext = nullptr;
    HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &desc, &chain, &hostDevice, nullptr, &hostContext);
    if (FAILED(result)) result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &desc, &chain, &hostDevice, nullptr, &hostContext);
    require(SUCCEEDED(result), "Create test device");
    hudSampleEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    HANDLE observer = CreateThread(nullptr, 0, observeDrivingHud, nullptr, 0, nullptr);
    require(observer != nullptr, "Start actual HUD observer");
    autopilot::HudAnchor fixedPose;
    bool havePose = false;
    for (size_t i = 0; i < fixtures.size(); ++i) {
        const auto& fixture = fixtures[i];
        if (i && (fixture.width != fixtures[i-1].width || fixture.height != fixtures[i-1].height))
            require(SUCCEEDED(chain->ResizeBuffers(1, fixture.width, fixture.height, DXGI_FORMAT_UNKNOWN, 0)), "Reset readback after resolution change");
        const ULONGLONG start = GetTickCount64();
        bool completed = false;
        for (int frame = 0; frame < 300; ++frame) {
            ID3D11Texture2D* buffer = nullptr; chain->GetBuffer(0, IID_PPV_ARGS(&buffer));
            hostContext->UpdateSubresource(buffer, 0, nullptr, fixture.rgba.data(), fixture.width * 4, 0); buffer->Release();
            chain->Present(0, 0); Sleep(16);
            std::lock_guard<std::mutex> lock(hudAnchorMutex);
            if (hudObservationTime >= start && hudAnchor.visible == fixture.expected) {
                if (fixture.expected) {
                    if (havePose && fixture.width == fixtures[i-1].width && fixture.height == fixtures[i-1].height)
                        require(hudAnchor.x == fixedPose.x && hudAnchor.y == fixedPose.y && hudAnchor.scale == fixedPose.scale,
                            "Different road-shake frames must keep exactly the same row pose");
                    fixedPose = hudAnchor; havePose = true;
                } else havePose = false;
                std::printf("GPU reference %u: driving=%d, score=%.3f, anchor=(%.1f,%.1f)\n",
                    static_cast<unsigned>(i + 1), hudAnchor.visible, hudAnchor.confidence, hudAnchor.x, hudAnchor.y);
                completed = true; break;
            }
        }
        require(completed, "Actual asynchronous GPU readback/observer did not recognize expected HUD state");
    }
    stopping = true; SetEvent(hudSampleEvent);
    require(WaitForSingleObject(observer, 2000) == WAIT_OBJECT_0, "Observer exits cleanly");
    CloseHandle(observer); CloseHandle(hudSampleEvent); hudSampleEvent = nullptr;
    std::puts("PASS: real GPU crop/readback, background detection, driving/loading/re-entry transitions");
}
