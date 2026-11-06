// Exercise the actual Present and ResizeBuffers hooks using an offscreen DX11
// swap chain. Images come from the test's GPU buffer, not the user's desktop.
#define AUTOPILOT_RENDER_TEST
#include "../src/Autopilot.cpp"
#include <vector>
#include <stdexcept>

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static int capture(IDXGISwapChain* chain, const wchar_t* filename) {
    ID3D11Texture2D* buffer = nullptr;
    require(SUCCEEDED(chain->GetBuffer(0, IID_PPV_ARGS(&buffer))), "Get buffer");
    D3D11_TEXTURE2D_DESC desc{}; buffer->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; desc.MiscFlags = 0;
    ID3D11Texture2D* staging = nullptr;
    require(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &staging)), "Create staging buffer");
    context->CopyResource(staging, buffer); buffer->Release();
    D3D11_MAPPED_SUBRESOURCE mapped{};
    require(SUCCEEDED(context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped)), "Map rendered buffer");
    std::vector<unsigned char> bgra(desc.Width * desc.Height * 4);
    int colored = 0;
    UINT rightmost = 0, lowest = 0;
    for (UINT y = 0; y < desc.Height; ++y) {
        const auto* row = static_cast<const unsigned char*>(mapped.pData) + y * mapped.RowPitch;
        for (UINT x = 0; x < desc.Width; ++x) {
            const size_t dst = (y * desc.Width + x) * 4;
            bgra[dst] = row[x * 4 + 2]; bgra[dst + 1] = row[x * 4 + 1]; bgra[dst + 2] = row[x * 4]; bgra[dst + 3] = 255;
            if (row[x * 4] > 130 || row[x * 4 + 1] > 130 || row[x * 4 + 2] > 130) {
                ++colored; rightmost = std::max(rightmost, x); lowest = std::max(lowest, y);
            }
        }
    }
    context->Unmap(staging, 0); staging->Release();
    if (colored) require(rightmost > desc.Width * .85f && lowest > desc.Height * .45f && lowest < desc.Height * .65f,
        "HUD stays on the right, above the lower gearbox area after resize");
    BITMAPFILEHEADER header{}; BITMAPINFOHEADER info{};
    header.bfType = 0x4d42; header.bfOffBits = sizeof(header) + sizeof(info);
    header.bfSize = header.bfOffBits + static_cast<DWORD>(bgra.size());
    info.biSize = sizeof(info); info.biWidth = desc.Width; info.biHeight = -static_cast<LONG>(desc.Height);
    info.biPlanes = 1; info.biBitCount = 32; info.biCompression = BI_RGB;
    FILE* file = _wfopen(filename, L"wb"); require(file != nullptr, "Open image output");
    std::fwrite(&header, sizeof(header), 1, file); std::fwrite(&info, sizeof(info), 1, file);
    std::fwrite(bgra.data(), bgra.size(), 1, file); std::fclose(file);
    return colored;
}

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
    auto frame = [&] {
        ID3D11Texture2D* buffer = nullptr; chain->GetBuffer(0, IID_PPV_ARGS(&buffer));
        ID3D11RenderTargetView* target = nullptr;
        hostDevice->CreateRenderTargetView(buffer, nullptr, &target); buffer->Release();
        const float color[] = {.1f, .15f, .2f, 1}; hostContext->ClearRenderTargetView(target, color); target->Release();
        chain->Present(0, 0);
    };
    for (int state = 0; state < 3; ++state) {
        displayState = state; frame(); frame();
        require(imgui && selectedChain == chain, "Real Present hook initializes renderer");
        const auto path = directory + L"hud-state-" + std::to_wstring(state) + L".bmp";
        require(capture(chain, path.c_str()) > 150, "HUD text has visible pixels");
    }
    hudVisible = false; frame();
    require(capture(chain, (directory + L"hud-hidden.bmp").c_str()) == 0, "Hidden HUD draws no pixels");
    hudVisible = true;
    hostContext->OMSetRenderTargets(0, nullptr, nullptr);
    require(SUCCEEDED(chain->ResizeBuffers(1, 1920, 1080, DXGI_FORMAT_UNKNOWN, 0)), "Actual ResizeBuffers hook releases render target");
    frame(); frame();
    require(capture(chain, (directory + L"hud-1080.bmp").c_str()) > 300, "HUD renders after resize");
    stopping = true;
    std::puts("PASS: DX11 hooks, 3 HUD states, visibility, 720p/1080p resize");
    // The plugin intentionally lives until process exit. Do not unload it.
    return 0;
}
