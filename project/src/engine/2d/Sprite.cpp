#include "engine/2d/Sprite.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/base/DirectXCommon.h"
#include "engine/3d/TextureManager.h"
#include "engine/base/SrvManager.h"
#include <cassert>

using namespace Math;

// 描画の共通設定と画像名を受け取り、2D画像を描くための頂点・材質・変換を準備する。
void Sprite::Initialize(SpriteCommon* spriteCommon, const std::string& filePath)
{
    assert(spriteCommon);
    spriteCommon_ = spriteCommon;

    // 画像の読込や描画設定に使う共通オブジェクトを保持する。
    textureFilePath_ = filePath;

    // 1) 必ず読み込む
    TextureManager::GetInstance()->LoadTexture(textureFilePath_);

    CreateVertexData();
    CreateMaterialData();
    CreateTransformData();

    AdjustTextureSize();
}


// 画面上の位置と大きさ、画像の切り出し範囲から、四角形の座標とUVを更新する。
void Sprite::Update()
{
    // ===== 頂点バッファを Map =====
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

    float left = 0.0f - anchorPoint_.x;
    float right = 1.0f - anchorPoint_.x;
    float top = 0.0f - anchorPoint_.y;
    float bottom = 1.0f - anchorPoint_.y;

    vertexData_[0].position = { left,  bottom, 0.0f, 1.0f };
    vertexData_[1].position = { left,  top,    0.0f, 1.0f };
    vertexData_[2].position = { right, bottom, 0.0f, 1.0f };
    vertexData_[3].position = { right, top,    0.0f, 1.0f };

    // UV
    float u0 = 0.0f, v0 = 0.0f;
    float u1 = 1.0f, v1 = 1.0f;
    if (isFlipX_) std::swap(u0, u1);
    if (isFlipY_) std::swap(v0, v1);

    vertexData_[0].texcoord = { u0, v1 };
    vertexData_[1].texcoord = { u0, v0 };
    vertexData_[2].texcoord = { u1, v1 };
    vertexData_[3].texcoord = { u1, v0 };

    // ===== Unmap =====
    vertexResource_->Unmap(0, nullptr);
    vertexData_ = nullptr;

    // UV 変換行列（T → S の順）
    Matrix4x4 uvScale = MakeScaleMatrix({ uvScale_.x, uvScale_.y, 1.0f });
    Matrix4x4 uvTrans = MakeTranslateMatrix({ uvTranslate_.x, uvTranslate_.y, 0.0f });
    materialData_->uvTransform = Multiply(uvScale, uvTrans);


    // 変換行列
    transform_.translate = { position_.x, position_.y, 0.0f };
    transform_.rotate = { 0.0f, 0.0f, rotation_ };
    transform_.scale = { size_.x, size_.y, 1.0f };

    Matrix4x4 world = MakeAffineMatrix(
        transform_.scale,
        transform_.rotate,
        transform_.translate);

    float width = static_cast<float>(WinApp::kClientWidth);
    float height = static_cast<float>(WinApp::kClientHeight);

    Matrix4x4 screenMatrix = {
        2.0f / width,   0,              0, 0,
        0,             -2.0f / height,  0, 0,
        0,              0,              1, 0,
       -1,              1,              0, 1
    };

    Matrix4x4 wvp = Multiply(world, screenMatrix);

    transformData_->World = Transpose(world);
    transformData_->WVP = Transpose(wvp);
}
// 四角形の頂点、色、テクスチャを描画命令へ登録する。
void Sprite::Draw()
{
    DirectXCommon* dxCommon = spriteCommon_->GetDxCommon();
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);

    commandList->SetGraphicsRootConstantBufferView(
        0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        1, transformResource_->GetGPUVirtualAddress());

    auto srvManager = spriteCommon_->GetSrvManager();
    srvManager->SetGraphicsRootDescriptorTable(
        2,
        TextureManager::GetInstance()->GetSrvIndex(textureFilePath_)
    );




    commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

// 四角形を2枚の三角形として描くための頂点とインデックスの領域を作る。
void Sprite::CreateVertexData()
{
    DirectXCommon* dxCommon = spriteCommon_->GetDxCommon();

    vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * 4);
    indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * 6);

    // 頂点データ
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

    // 左下
    vertexData_[0].position = { 0.0f, 1.0f, 0.0f, 1.0f };
    vertexData_[0].texcoord = { 0.0f, 1.0f };
    vertexData_[0].normal = { 0.0f, 0.0f, -1.0f };

    // 左上
    vertexData_[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
    vertexData_[1].texcoord = { 0.0f, 0.0f };
    vertexData_[1].normal = { 0.0f, 0.0f, -1.0f };

    // 右下
    vertexData_[2].position = { 1.0f, 1.0f, 0.0f, 1.0f };
    vertexData_[2].texcoord = { 1.0f, 1.0f };
    vertexData_[2].normal = { 0.0f, 0.0f, -1.0f };

    // 右上
    vertexData_[3].position = { 1.0f, 0.0f, 0.0f, 1.0f };
    vertexData_[3].texcoord = { 1.0f, 0.0f };
    vertexData_[3].normal = { 0.0f, 0.0f, -1.0f };


    // インデックス
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
    indexData_[0] = 0; indexData_[1] = 1; indexData_[2] = 2;
    indexData_[3] = 1; indexData_[4] = 3; indexData_[5] = 2;

    // VB/IBビュー
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
}


// 描画に使う色とUV変換を定数バッファに用意する。
void Sprite::CreateMaterialData()
{
    DirectXCommon* dxCommon = spriteCommon_->GetDxCommon();

    materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = false;
    materialData_->padding[0] = materialData_->padding[1] = materialData_->padding[2] = 0.0f;
    materialData_->uvTransform = MakeIdentity4x4();
}

// スプライトの姿勢と画面への正射影に使う定数バッファを作る。
void Sprite::CreateTransformData()
{
    DirectXCommon* dxCommon = spriteCommon_->GetDxCommon();

    transformResource_ = dxCommon->CreateBufferResource(sizeof(TransformData));
    transformResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformData_));

    transform_.scale = { 1.0f, 1.0f, 1.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 0.0f, 0.0f, 0.0f };

    Matrix4x4 world = MakeAffineMatrix(
        transform_.scale,
        transform_.rotate,
        transform_.translate);

    Matrix4x4 vp = MakeIdentity4x4();
    transformData_->World = world;
    transformData_->WVP = Multiply(world, vp);
}

// 画像のメタデータから元の幅と高さを取り、表示サイズの初期値にする。
void Sprite::AdjustTextureSize()
{
    const DirectX::TexMetadata& metadata =
        TextureManager::GetInstance()->GetMetaData(textureFilePath_);


    Math::Vector2 textureSize;
    textureSize.x = static_cast<float>(metadata.width);
    textureSize.y = static_cast<float>(metadata.height);

    size_ = textureSize;
}
