#include <Windows.h>
#include <DirectXTex.h>

#include <filesystem>
#include <iomanip>
#include <iostream>

namespace {

bool CheckResult(HRESULT hr, const char* operation)
{
    if (SUCCEEDED(hr)) {
        return true;
    }

    std::cerr << operation << " failed: 0x"
              << std::hex << std::uppercase << static_cast<unsigned long>(hr)
              << '\n';
    return false;
}

}

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 3) {
        std::wcerr << L"Usage: TextureConverter <input.png> <output.dds>\n";
        return 2;
    }

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(comResult)) {
        return 3;
    }

    const std::filesystem::path inputPath = argv[1];
    const bool isNormalMap =
        inputPath.stem().wstring().find(L"_Normal") != std::wstring::npos;
    const auto wicFlags = isNormalMap ?
        DirectX::WIC_FLAGS_NONE :
        DirectX::WIC_FLAGS_FORCE_SRGB;

    DirectX::TexMetadata sourceMetadata{};
    DirectX::ScratchImage sourceImage;
    HRESULT hr = DirectX::LoadFromWICFile(
        argv[1],
        wicFlags,
        &sourceMetadata,
        sourceImage);
    if (!CheckResult(hr, "LoadFromWICFile")) {
        CoUninitialize();
        return 4;
    }

    DirectX::ScratchImage mipImages;
    hr = DirectX::GenerateMipMaps(
        sourceImage.GetImages(),
        sourceImage.GetImageCount(),
        sourceMetadata,
        isNormalMap ? DirectX::TEX_FILTER_DEFAULT : DirectX::TEX_FILTER_SRGB,
        0,
        mipImages);
    if (!CheckResult(hr, "GenerateMipMaps")) {
        CoUninitialize();
        return 5;
    }

    DirectX::ScratchImage compressedImages;
    const auto compressFlags = static_cast<DirectX::TEX_COMPRESS_FLAGS>(
        DirectX::TEX_COMPRESS_BC7_QUICK |
        DirectX::TEX_COMPRESS_PARALLEL);
    hr = DirectX::Compress(
        mipImages.GetImages(),
        mipImages.GetImageCount(),
        mipImages.GetMetadata(),
        isNormalMap ? DXGI_FORMAT_BC7_UNORM : DXGI_FORMAT_BC7_UNORM_SRGB,
        compressFlags,
        DirectX::TEX_THRESHOLD_DEFAULT,
        compressedImages);
    if (!CheckResult(hr, "Compress")) {
        CoUninitialize();
        return 6;
    }

    const std::filesystem::path outputPath = argv[2];
    std::filesystem::create_directories(outputPath.parent_path());
    hr = DirectX::SaveToDDSFile(
        compressedImages.GetImages(),
        compressedImages.GetImageCount(),
        compressedImages.GetMetadata(),
        DirectX::DDS_FLAGS_NONE,
        outputPath.c_str());
    if (!CheckResult(hr, "SaveToDDSFile")) {
        CoUninitialize();
        return 7;
    }

    std::wcout << argv[1] << L" -> " << argv[2] << L'\n';
    CoUninitialize();
    return 0;
}
