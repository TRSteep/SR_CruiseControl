#pragma once
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
