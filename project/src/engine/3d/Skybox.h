// 環境画像を遠景として描き、反射に使うキューブマップも公開する

#pragma once
#include <cstdint>
#include <string>
#include <wrl.h>
#include <d3d12.h>

#include "engine/base/Math.h"

class Camera;
class DirectXCommon;
class SrvManager;

class Skybox {
public:
    void Initialize(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        const std::string& texturePath);

    void Update(const Camera* camera);
    void Draw();

private:
    struct VertexData {
        Math::Vector4 position;
    };

    struct ViewProjectionData {
        Math::Matrix4x4 viewProjection;
    };

    void CreateRootSignature();
    void CreateGraphicsPipelineState();
    void CreateVertexBuffer();
    void CreateViewProjectionResource();

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    std::string texturePath_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    uint32_t vertexCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionResource_;
    ViewProjectionData* viewProjectionData_ = nullptr;
};
