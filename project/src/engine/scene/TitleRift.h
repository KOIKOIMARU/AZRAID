// タイトルやステージ選択の切断面と航跡を、画面へ合成する描画処理

#pragma once

#include "engine/base/Math.h"

#include <d3d12.h>
#include <wrl/client.h>

#include <array>

class DirectXCommon;
namespace TitleMotion { struct TitleFlyby; }

// タイトル専用の断面と航跡　画像・中間描画面・追加SRVを使わず直接描く
class TitleRift {
public:
    enum class DrawMode { Background, StageBackground, Wake, StageWake, Reveal, Fade };

    void Initialize(DirectXCommon* dxCommon);

    void PrepareFlightDepth();

    void BindFlightTarget(bool begin, bool clearDepth = true);

    void Draw(float elapsed, float departure, const TitleMotion::TitleFlyby& flyby,
        const std::array<Math::Vector4, 2>& engines,
        const std::array<Math::Vector4, 12>& wakePoints, DrawMode mode,
        const Math::Vector2* stageSeam = nullptr);
    void DrawStageCurtain(float elapsed, Math::Vector2 seam, float halfWidth, float heat, float zoom = 1.0f, float rush = 0.0f,
        float kick = 0.0f, const Math::Vector4* projectedEdges = nullptr);
private:
    DirectXCommon* dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> wakePipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> departurePipeline_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> flightDsvHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> flightDepth_; // DEPTH_WRITEのまま使う、タイトル専用の深度
    UINT flightWidth_ = 0;
    UINT flightHeight_ = 0;
};
