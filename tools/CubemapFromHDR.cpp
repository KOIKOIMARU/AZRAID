#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <DirectXTex.h>
#include <DirectXMath.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct Float4 {
    float x;
    float y;
    float z;
    float w;
};

DirectX::XMFLOAT3 Normalize(const DirectX::XMFLOAT3& v)
{
    const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length <= 0.0f) {
        return { 0.0f, 0.0f, 1.0f };
    }
    return { v.x / length, v.y / length, v.z / length };
}

DirectX::XMFLOAT3 FacePixelToDirection(size_t face, float u, float v)
{
    DirectX::XMFLOAT3 direction{};

    switch (face) {
    case 0: direction = { 1.0f, -v, -u }; break; // +X
    case 1: direction = { -1.0f, -v, u }; break; // -X
    case 2: direction = { u, 1.0f, v }; break;   // +Y
    case 3: direction = { u, -1.0f, -v }; break; // -Y
    case 4: direction = { u, -v, 1.0f }; break;  // +Z
    case 5: direction = { -u, -v, -1.0f }; break;// -Z
    default: direction = { 0.0f, 0.0f, 1.0f }; break;
    }

    return Normalize(direction);
}

Float4 ReadPixel(
    const DirectX::Image& image,
    size_t x,
    size_t y)
{
    if (image.format == DXGI_FORMAT_R32G32B32A32_FLOAT) {
        const auto* row =
            reinterpret_cast<const Float4*>(image.pixels + y * image.rowPitch);
        return row[x];
    }

    if (image.format == DXGI_FORMAT_R32G32B32_FLOAT) {
        struct Float3 {
            float x;
            float y;
            float z;
        };

        const auto* row =
            reinterpret_cast<const Float3*>(image.pixels + y * image.rowPitch);
        const auto pixel = row[x];
        return { pixel.x, pixel.y, pixel.z, 1.0f };
    }

    return { 1.0f, 0.0f, 1.0f, 1.0f };
}

Float4 SampleEquirectBilinear(
    const DirectX::Image& image,
    const DirectX::XMFLOAT3& direction)
{
    const float theta = std::atan2(direction.z, direction.x);
    const float phi = std::acos(std::clamp(direction.y, -1.0f, 1.0f));

    float uf = (theta + kPi) / (2.0f * kPi);
    float vf = phi / kPi;

    uf = uf - std::floor(uf);
    vf = std::clamp(vf, 0.0f, 1.0f);

    const float srcX = uf * static_cast<float>(image.width - 1);
    const float srcY = vf * static_cast<float>(image.height - 1);

    const size_t x0 = static_cast<size_t>(std::floor(srcX));
    const size_t y0 = static_cast<size_t>(std::floor(srcY));
    const size_t x1 = (x0 + 1) % image.width;
    const size_t y1 = std::min(y0 + 1, image.height - 1);

    const float tx = srcX - static_cast<float>(x0);
    const float ty = srcY - static_cast<float>(y0);

    const Float4 c00 = ReadPixel(image, x0, y0);
    const Float4 c10 = ReadPixel(image, x1, y0);
    const Float4 c01 = ReadPixel(image, x0, y1);
    const Float4 c11 = ReadPixel(image, x1, y1);

    const auto lerp = [](float a, float b, float t) {
        return a + (b - a) * t;
    };

    Float4 result{};
    result.x = lerp(lerp(c00.x, c10.x, tx), lerp(c01.x, c11.x, tx), ty);
    result.y = lerp(lerp(c00.y, c10.y, tx), lerp(c01.y, c11.y, tx), ty);
    result.z = lerp(lerp(c00.z, c10.z, tx), lerp(c01.z, c11.z, tx), ty);
    result.w = 1.0f;
    return result;
}

void WriteFace(
    const DirectX::Image& source,
    size_t faceIndex,
    DirectX::Image& destination)
{
    for (size_t y = 0; y < destination.height; ++y) {
        auto* row =
            reinterpret_cast<Float4*>(destination.pixels + y * destination.rowPitch);

        for (size_t x = 0; x < destination.width; ++x) {
            const float u =
                (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(destination.width)) - 1.0f;
            const float v =
                (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(destination.height)) - 1.0f;

            const DirectX::XMFLOAT3 direction =
                FacePixelToDirection(faceIndex, u, v);

            row[x] = SampleEquirectBilinear(source, direction);
        }
    }
}

} // namespace

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 3) {
        std::wcerr
            << L"Usage: CubemapFromHDR <input.hdr> <output.dds> [faceSize]\n";
        return 1;
    }

    const std::filesystem::path inputPath = argv[1];
    const std::filesystem::path outputPath = argv[2];
    const size_t faceSize =
        (argc >= 4) ? static_cast<size_t>(_wtoi(argv[3])) : 1024;

    DirectX::ScratchImage hdrImage;
    DirectX::TexMetadata hdrMetadata{};
    HRESULT hr = DirectX::LoadFromHDRFile(
        inputPath.c_str(),
        &hdrMetadata,
        hdrImage);
    if (FAILED(hr)) {
        std::wcerr << L"LoadFromHDRFile failed: 0x" << std::hex << hr << L"\n";
        return 1;
    }

    const DirectX::Image* srcImage = hdrImage.GetImage(0, 0, 0);
    if (!srcImage) {
        std::wcerr << L"Source image is null.\n";
        return 1;
    }

    if (srcImage->format != DXGI_FORMAT_R32G32B32_FLOAT &&
        srcImage->format != DXGI_FORMAT_R32G32B32A32_FLOAT) {
        std::wcerr << L"Unsupported HDR source format: " << srcImage->format << L"\n";
        return 1;
    }

    DirectX::ScratchImage cubeFloat;
    hr = cubeFloat.InitializeCube(
        DXGI_FORMAT_R32G32B32A32_FLOAT,
        faceSize,
        faceSize,
        1,
        1);
    if (FAILED(hr)) {
        std::wcerr << L"InitializeCube failed: 0x" << std::hex << hr << L"\n";
        return 1;
    }

    for (size_t face = 0; face < 6; ++face) {
        const DirectX::Image* faceImage = cubeFloat.GetImage(0, face, 0);
        if (!faceImage) {
            std::wcerr << L"Face image is null at index " << face << L"\n";
            return 1;
        }

        DirectX::Image writable = *faceImage;
        WriteFace(*srcImage, face, writable);
    }

    DirectX::ScratchImage cubeHalf;
    hr = DirectX::Convert(
        cubeFloat.GetImages(),
        cubeFloat.GetImageCount(),
        cubeFloat.GetMetadata(),
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        DirectX::TEX_FILTER_DEFAULT,
        DirectX::TEX_THRESHOLD_DEFAULT,
        cubeHalf);
    if (FAILED(hr)) {
        std::wcerr << L"Convert to half float failed: 0x" << std::hex << hr << L"\n";
        return 1;
    }

    DirectX::ScratchImage mipChain;
    hr = DirectX::GenerateMipMaps(
        cubeHalf.GetImages(),
        cubeHalf.GetImageCount(),
        cubeHalf.GetMetadata(),
        DirectX::TEX_FILTER_DEFAULT,
        0,
        mipChain);
    if (FAILED(hr)) {
        std::wcerr << L"GenerateMipMaps failed: 0x" << std::hex << hr << L"\n";
        return 1;
    }

    hr = DirectX::SaveToDDSFile(
        mipChain.GetImages(),
        mipChain.GetImageCount(),
        mipChain.GetMetadata(),
        DirectX::DDS_FLAGS_FORCE_DX10_EXT,
        outputPath.c_str());
    if (FAILED(hr)) {
        std::wcerr << L"SaveToDDSFile failed: 0x" << std::hex << hr << L"\n";
        return 1;
    }

    std::wcout << L"Saved cubemap DDS: " << outputPath << L"\n";
    return 0;
}
