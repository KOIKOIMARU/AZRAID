// テクスチャの読み込み・GPUへの転送・名前による再利用を担当する

#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <future>
#include <wrl.h>
#include <d3d12.h>
#include "DirectXTex.h"

class DirectXCommon;
class SrvManager;

class TextureManager {
public:
    static TextureManager* GetInstance();

    // 画像をシェーダーから参照するための記述子を割り当てる管理クラスを受け取る
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    // 同じ画像を二重に作らないよう登録状況を確認し、必要な場合は準備を完了させる
    void LoadTexture(const std::string& filePath, bool forceSrgb = true);
    bool PreloadTextureStep(const std::string& filePath, bool forceSrgb = true);
    bool IsTextureLoaded(const std::string& filePath) const { return textureDatas_.contains(filePath); }
    void CreateSolidTexture2D(
        const std::string& name,
        uint8_t red,
        uint8_t green,
        uint8_t blue,
        uint8_t alpha = 255);
    void CreateSolidCubeTexture(
        const std::string& name,
        uint8_t red,
        uint8_t green,
        uint8_t blue,
        uint8_t alpha = 255);
    void CreateSkyGradientCubeTexture(const std::string& name);

    // ===== getter =====
    const DirectX::TexMetadata& GetMetaData(const std::string& filePath);
    uint32_t GetSrvIndex(const std::string& filePath);

    static void Destroy();

private:
    TextureManager() = default;
    ~TextureManager() = default;
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

private:
    struct TextureData {
        DirectX::TexMetadata metadata;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint32_t srvIndex = 0;
    };


    std::unordered_map<std::string, TextureData> textureDatas_;

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    void InstallTexture(const std::string& filePath, const DirectX::ScratchImage& mipImages);
    std::future<DirectX::ScratchImage> pendingImage_;
    std::string pendingImagePath_;

    static TextureManager* instance_;
};
