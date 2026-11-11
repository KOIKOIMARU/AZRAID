// 2D画像1枚の位置、大きさ、色、テクスチャの切り出し範囲を持つ。

#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <string>

#include "engine/base/Math.h"

class SpriteCommon;

class Sprite {
public:
    // 表示する画像を読み込み、スプライトの描画に必要なデータを準備する。
    void Initialize(SpriteCommon* spriteCommon, const std::string& filePath);

    void Update();
    void Draw();

    // セッター
    void SetPosition(const Math::Vector2& position) { position_ = position; }
    void SetRotation(float rotation) { rotation_ = rotation; }
    void SetSize(const Math::Vector2& size) { size_ = size; }
    void SetColor(const Math::Vector4& color) { materialData_->color = color; }

    // -------- アンカーポイント --------
    const Math::Vector2& GetAnchorPoint() const { return anchorPoint_; }
    void SetAnchorPoint(const Math::Vector2& p) { anchorPoint_ = p; }

    // -------- UVスケール / 移動 --------
    void SetUVScale(const Math::Vector2& scale) { uvScale_ = scale; }
    const Math::Vector2& GetUVScale() const { return uvScale_; }

    void SetUVTranslate(const Math::Vector2& trans) { uvTranslate_ = trans; }
    const Math::Vector2& GetUVTranslate() const { return uvTranslate_; }

    // -------- フリップ --------
    void SetFlipX(bool flag) { isFlipX_ = flag; }
    void SetFlipY(bool flag) { isFlipY_ = flag; }

    bool GetFlipX() const { return isFlipX_; }
    bool GetFlipY() const { return isFlipY_; }

private:
    struct VertexData {
        Math::Vector4 position;
        Math::Vector2 texcoord;
        Math::Vector3 normal;
    };

    struct Material {
        Math::Vector4  color;
        int32_t        enableLighting;
        float          padding[3];
        Math::Matrix4x4 uvTransform;
    };

    struct TransformData {
        Math::Matrix4x4 WVP;
        Math::Matrix4x4 World;
    };

    void CreateVertexData();
    void CreateMaterialData();
    void CreateTransformData();

    // テクスチャサイズから切り出し範囲＆スプライトサイズを初期化
    void AdjustTextureSize();

private:
    SpriteCommon* spriteCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    VertexData* vertexData_ = nullptr;
    uint32_t* indexData_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW  indexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;
    TransformData* transformData_ = nullptr;

    Math::Transform transform_{};

    Math::Vector2 position_{ 0.0f, 0.0f };
    float rotation_ = 0.0f;
    Math::Vector2 size_{ 640.0f, 360.0f };


    std::string textureFilePath_;


    // AnchorPoint（0～1）
    Math::Vector2 anchorPoint_{ 0.0f, 0.0f };

    // UV 切り出し（スケール / 平行移動）
    Math::Vector2 uvScale_ = { 1.0f, 1.0f };
    Math::Vector2 uvTranslate_ = { 0.0f, 0.0f };

    // 反転フラグ
    bool isFlipX_ = false;
    bool isFlipY_ = false;
};
