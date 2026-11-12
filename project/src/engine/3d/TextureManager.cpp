// ===== TextureManager.cpp =====
// テクスチャを読み込んでGPUに転送し、描画で使うSRVとともに再利用できるようにする

#include "engine/3d/TextureManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

TextureManager* TextureManager::instance_ = nullptr;

TextureManager* TextureManager::GetInstance() {
    if (!instance_) {
        instance_ = new TextureManager();
    }
    return instance_;
}

// テクスチャの確保とビューの登録に使う描画システムを受け取る
void TextureManager::Initialize(
    DirectXCommon* dxCommon,
    SrvManager* srvManager) {

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    textureDatas_.clear();
    CreateSolidTexture2D("resources/default_normal.png", 128, 128, 255, 255);
}

// 別スレッドで画像を読み込み中なら完了を待ち、管理インスタンスを破棄する
void TextureManager::Destroy() {
    if (instance_ && instance_->pendingImage_.valid()) { instance_->pendingImage_.wait(); }
    delete instance_;
    instance_ = nullptr;
}

// 同じ画像を二重に作らないよう登録状況を確認し、必要な場合は準備を完了させる
void TextureManager::LoadTexture(const std::string& filePath, bool forceSrgb) {

    // 同じ画像が準備済みなら、登録済みのテクスチャを使う
    if (textureDatas_.contains(filePath)) {
        return;
    }




    // 読み込み
    DirectX::ScratchImage mipImages =
        DirectXCommon::LoadTexture(filePath, forceSrgb);
    InstallTexture(filePath, mipImages);
}

// 画像の読み込みとGPU登録を段階的に行い、準備が完了したら成功を返す
bool TextureManager::PreloadTextureStep(const std::string& filePath, bool forceSrgb)
{
    if (textureDatas_.contains(filePath) && pendingImagePath_ != filePath) { return true; }
    if (!pendingImage_.valid()) {
        pendingImagePath_ = filePath;
        pendingImage_ = std::async(std::launch::async, [filePath, forceSrgb]() {
            // WICを使うCPU workerにもCOM初期化が必要　GPU・SRV・command listには触れない
            const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(hr)) { throw std::runtime_error("Texture worker COM initialization failed"); }
            try {
                auto image = DirectXCommon::LoadTexture(filePath, forceSrgb);
                CoUninitialize();
                return image;
            } catch (...) { CoUninitialize(); throw; }
        });
        return false;
    }
    assert(pendingImagePath_ == filePath);
    if (pendingImagePath_ != filePath || pendingImage_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) { return false; }
    auto image = pendingImage_.get();
    if (!textureDatas_.contains(filePath)) { InstallTexture(filePath, image); }
    pendingImagePath_.clear();
    return true;
}

// 読み込み済みの画素をGPUに転送し、シェーダーから読める番号と一緒に登録する
void TextureManager::InstallTexture(const std::string& filePath, const DirectX::ScratchImage& mipImages)
{
    TextureData& textureData = textureDatas_[filePath];
    textureData.metadata = mipImages.GetMetadata();

    // GPUリソース作成
    textureData.resource =
        dxCommon_->CreateTextureResource(textureData.metadata);
    dxCommon_->UploadTextureData(textureData.resource, mipImages);

    // ===== SRV確保 =====
    textureData.srvIndex = srvManager_->Allocate();

    // Cubemapの場合はTexture2DではなくTextureCubeとしてSRVを作成
    if (textureData.metadata.IsCubemap()) {
        srvManager_->CreateSRVforTextureCube(
            textureData.srvIndex,
            textureData.resource.Get(),
            textureData.metadata.format,
            UINT(textureData.metadata.mipLevels)
        );
    } else {
        // SRV作成（Texture2D）
        srvManager_->CreateSRVforTexture2D(
            textureData.srvIndex,
            textureData.resource.Get(),
            textureData.metadata.format,
            UINT(textureData.metadata.mipLevels)
        );
    }

}

// 画像ファイルの代わりに単色の2Dテクスチャを生成して登録する
void TextureManager::CreateSolidTexture2D(
    const std::string& name,
    uint8_t red,
    uint8_t green,
    uint8_t blue,
    uint8_t alpha)
{
    if (textureDatas_.contains(name)) {
        return;
    }

    DirectX::ScratchImage image;
    HRESULT hr = image.Initialize2D(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        1,
        1,
        1,
        1);
    assert(SUCCEEDED(hr));

    uint8_t* pixel = image.GetPixels();
    pixel[0] = red;
    pixel[1] = green;
    pixel[2] = blue;
    pixel[3] = alpha;

    TextureData& textureData = textureDatas_[name];
    textureData.metadata = image.GetMetadata();
    textureData.resource =
        dxCommon_->CreateTextureResource(textureData.metadata);
    dxCommon_->UploadTextureData(textureData.resource, image);

    textureData.srvIndex = srvManager_->Allocate();
    srvManager_->CreateSRVforTexture2D(
        textureData.srvIndex,
        textureData.resource.Get(),
        textureData.metadata.format,
        UINT(textureData.metadata.mipLevels));
}

// 単色で塗った6面の画像を作り、キューブマップとして登録する
void TextureManager::CreateSolidCubeTexture(
    const std::string& name,
    uint8_t red,
    uint8_t green,
    uint8_t blue,
    uint8_t alpha)
{
    if (textureDatas_.contains(name)) {
        return;
    }

    DirectX::ScratchImage image;
    HRESULT hr = image.InitializeCube(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        1,
        1,
        1,
        1);
    assert(SUCCEEDED(hr));

    const DirectX::Image* images = image.GetImages();
    for (size_t index = 0; index < image.GetImageCount(); ++index) {
        uint8_t* pixel = images[index].pixels;
        pixel[0] = red;
        pixel[1] = green;
        pixel[2] = blue;
        pixel[3] = alpha;
    }

    TextureData& textureData = textureDatas_[name];
    textureData.metadata = image.GetMetadata();
    textureData.resource =
        dxCommon_->CreateTextureResource(textureData.metadata);
    dxCommon_->UploadTextureData(textureData.resource, image);

    textureData.srvIndex = srvManager_->Allocate();
    srvManager_->CreateSRVforTextureCube(
        textureData.srvIndex,
        textureData.resource.Get(),
        textureData.metadata.format,
        UINT(textureData.metadata.mipLevels));
}

// 空の高さに応じて色を補間し、背景や反射に使うキューブマップを生成する
void TextureManager::CreateSkyGradientCubeTexture(const std::string& name)
{
    if (textureDatas_.contains(name)) {
        return;
    }

    constexpr size_t kSize = 16;
    DirectX::ScratchImage image;
    HRESULT hr = image.InitializeCube(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        kSize,
        kSize,
        1,
        1);
    assert(SUCCEEDED(hr));

    const auto writePixel =
        [](uint8_t* pixel, float red, float green, float blue) {
            pixel[0] = static_cast<uint8_t>(std::clamp(red, 0.0f, 255.0f));
            pixel[1] = static_cast<uint8_t>(std::clamp(green, 0.0f, 255.0f));
            pixel[2] = static_cast<uint8_t>(std::clamp(blue, 0.0f, 255.0f));
            pixel[3] = 255;
        };
    const auto mix = [](float a, float b, float rate) {
        return a + (b - a) * std::clamp(rate, 0.0f, 1.0f);
    };

    const DirectX::Image* images = image.GetImages();
    for (size_t face = 0; face < image.GetImageCount(); ++face) {
        DirectX::Image faceImage = images[face];
        for (size_t y = 0; y < kSize; ++y) {
            uint8_t* row = faceImage.pixels + faceImage.rowPitch * y;
            for (size_t x = 0; x < kSize; ++x) {
                const float v =
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(kSize) *
                    2.0f - 1.0f;
                float dirY = -v;
                if (face == 2) {
                    dirY = 1.0f;
                } else if (face == 3) {
                    dirY = -1.0f;
                }

                const float skyRate = std::clamp((dirY + 0.15f) * 0.62f, 0.0f, 1.0f);
                const float horizonGlow =
                    (std::max)(0.0f, 1.0f - std::abs(dirY + 0.12f) * 3.1f);
                const float cloudBand =
                    (std::max)(0.0f, 1.0f - std::abs(dirY + 0.36f) * 4.4f);

                float red = mix(86.0f, 156.0f, skyRate);
                float green = mix(126.0f, 190.0f, skyRate);
                float blue = mix(184.0f, 232.0f, skyRate);
                red = mix(red, 225.0f, horizonGlow * 0.34f);
                green = mix(green, 235.0f, horizonGlow * 0.34f);
                blue = mix(blue, 246.0f, horizonGlow * 0.34f);
                red = mix(red, 208.0f, cloudBand * 0.20f);
                green = mix(green, 224.0f, cloudBand * 0.20f);
                blue = mix(blue, 238.0f, cloudBand * 0.20f);

                writePixel(row + x * 4, red, green, blue);
            }
        }
    }

    TextureData& textureData = textureDatas_[name];
    textureData.metadata = image.GetMetadata();
    textureData.resource =
        dxCommon_->CreateTextureResource(textureData.metadata);
    dxCommon_->UploadTextureData(textureData.resource, image);

    textureData.srvIndex = srvManager_->Allocate();
    srvManager_->CreateSRVforTextureCube(
        textureData.srvIndex,
        textureData.resource.Get(),
        textureData.metadata.format,
        UINT(textureData.metadata.mipLevels));
}

const DirectX::TexMetadata&
TextureManager::GetMetaData(const std::string& filePath) {
    assert(textureDatas_.contains(filePath));
    return textureDatas_.at(filePath).metadata;
}

uint32_t TextureManager::GetSrvIndex(const std::string& filePath) {
    assert(textureDatas_.contains(filePath));
    return textureDatas_.at(filePath).srvIndex;
}
