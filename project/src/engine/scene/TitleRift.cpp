// ===== TitleRift.cpp =====
// タイトルから出撃までの裂け目をシェーダーで描き、機体と背景の重なりを処理する。

#include "engine/scene/TitleRift.h"

#include "engine/base/DirectXCommon.h"
#include "engine/scene/TitleMotion.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

using namespace TitleMotion;

// 裂け目の描画で使うシェーダーとルートシグネチャ、描画パイプラインを用意する。
void TitleRift::Initialize(DirectXCommon* dxCommon)
{
    dxCommon_ = dxCommon;
    D3D12_ROOT_PARAMETER parameter{};
    parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameter.Constants.Num32BitValues = 64;
    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters = 1;
    rootDesc.pParameters = &parameter;
    Microsoft::WRL::ComPtr<ID3DBlob> signature, error;
    if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)) ||
        FAILED(dxCommon_->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(),
            signature->GetBufferSize(), IID_PPV_ARGS(&root_)))) {
        throw std::runtime_error("Title rift root signature");
    }
    const auto vs = dxCommon_->CompileShader(L"shaders/Fullscreen.VS.hlsl", L"vs_6_0");
    const auto ps = dxCommon_->CompileShader(L"shaders/TitleRift.PS.hlsl", L"ps_6_0");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = root_.Get();
    desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    desc.RasterizerState.DepthClipEnable = TRUE;
    desc.DepthStencilState.DepthEnable = FALSE;
    desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipeline_)))) {
        throw std::runtime_error("Title rift pipeline");
    }
    auto& blend = desc.BlendState.RenderTarget[0];
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&wakePipeline_)))) {
        throw std::runtime_error("Title flight wake pipeline");
    }
    // 本編の色補正後の描画先には深度を接続しない。
    desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    if (FAILED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&departurePipeline_)))) {
        throw std::runtime_error("Title departure overlay pipeline");
    }
    flightDsvHeap_ = dxCommon_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1,false);
}

// 画面の大きさに合わせて機体の重なり判定用の深度バッファを確保する。解像度が同じなら再利用する。
void TitleRift::PrepareFlightDepth()
{
    DXGI_SWAP_CHAIN_DESC1 swapDesc{};
    if (FAILED(dxCommon_->GetSwapChain()->GetDesc1(&swapDesc))) {
        throw std::runtime_error("Title flight target size");
    }
    if (flightDepth_ && flightWidth_ == swapDesc.Width && flightHeight_ == swapDesc.Height) { return; }
    // Updateは前フレームのPostDrawによるGPU完了待ち後。リサイズ時だけ作り直す。
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap.CreationNodeMask = heap.VisibleNodeMask = 1;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = swapDesc.Width;
    desc.Height = swapDesc.Height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    D3D12_CLEAR_VALUE clear{};
    clear.Format = desc.Format;
    clear.DepthStencil.Depth = 1.0f;
    Microsoft::WRL::ComPtr<ID3D12Resource> depth;
    if (FAILED(dxCommon_->GetDevice()->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear,IID_PPV_ARGS(&depth)))) {
        throw std::runtime_error("Title flight depth");
    }
    D3D12_DEPTH_STENCIL_VIEW_DESC view{};
    view.Format = desc.Format;
    view.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dxCommon_->GetDevice()->CreateDepthStencilView(depth.Get(),&view,
        flightDsvHeap_->GetCPUDescriptorHandleForHeapStart());
    flightDepth_ = std::move(depth);
    flightWidth_ = swapDesc.Width;
    flightHeight_ = swapDesc.Height;
}

// 機体を画面バッファへ描く際は、機体専用の深度バッファを組み合わせる。
void TitleRift::BindFlightTarget(bool begin, bool clearDepth)
{
    auto rtv = dxCommon_->GetRTVHeap()->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(dxCommon_->GetSwapChain()->GetCurrentBackBufferIndex()) *
        dxCommon_->GetRTVDescriptorSize();
    auto* command = dxCommon_->GetCommandList();
    const auto dsv = flightDsvHeap_->GetCPUDescriptorHandleForHeapStart();
    command->OMSetRenderTargets(1,&rtv,FALSE,begin ? &dsv : nullptr);
    if (begin) {
        // 本編・背景の深度を持ち込まず、機体自身の前後関係だけを解決する。
        if (clearDepth) { command->ClearDepthStencilView(dsv,D3D12_CLEAR_FLAG_DEPTH,1.0f,0,0,nullptr); }
        const auto& viewport = dxCommon_->GetPresentationViewport();
        const D3D12_RECT scissor{0,0,static_cast<LONG>(flightWidth_),static_cast<LONG>(flightHeight_)};
        command->RSSetViewports(1,&viewport);
        command->RSSetScissorRects(1,&scissor);
    }
}

// 背景・航跡・ステージ開示などの描画モードに合わせて、裂け目の情報をシェーダーへ渡す。
void TitleRift::Draw(float elapsed, float departure, const TitleFlyby& flyby,
    const std::array<Math::Vector4, 2>& engines,
    const std::array<Math::Vector4, 12>& wakePoints, DrawMode mode,
    const Math::Vector2* stageSeam)
{
    const bool wake = mode == DrawMode::Wake || mode == DrawMode::StageWake;
    const bool reveal = mode == DrawMode::Reveal || mode == DrawMode::Fade;
    // この値は TitleRift.PS.hlsl の描画モードに対応する。
    float shaderMode = 0.0f;
    switch (mode) {
    case DrawMode::Background: break;
    case DrawMode::StageBackground: shaderMode = 4.0f; break;
    case DrawMode::Wake: shaderMode = IsTitleOrbitalWake(flyby.phase, flyby.pattern) ? 2.0f : 1.0f; break;
    case DrawMode::StageWake: shaderMode = 1.25f; break;
    case DrawMode::Reveal: shaderMode = -1.0f; break;
    case DrawMode::Fade: shaderMode = 3.0f; break;
    }
    const auto& viewport = reveal ? dxCommon_->GetPresentationViewport() : dxCommon_->GetViewport();
    std::array<float, 64> parameters{ viewport.Width, viewport.Height, elapsed, departure,
        flyby.lightX, flyby.energy, flyby.phase + static_cast<float>(flyby.pattern) * kFlybyLoop +
            (flyby.cutPattern == 2 ? kFlightSequence : 0.0f),
        shaderMode,
        engines[0].x, engines[0].y, engines[0].z, engines[0].w,
        engines[1].x, engines[1].y, engines[1].z, engines[1].w };
    if (departure > 0.0f && !wake) {
        // 背景モードでは未使用のノズル領域を切断姿勢に使う。航跡モードは元の値を保持。
        // root定数64個・GPU所有権を変えず、文字・自機・背景を同じCPUの時計で描く。
        const auto strike = GetDepartureStrike(departure);
        const auto offset = DepartureOffset(departure,0);
        parameters[5] = DepartureResistance(departure);
        parameters[8] = strike.bladeX; parameters[9] = strike.charge;
        parameters[10] = (std::max)(0.0f,departure - kDepartureImpactTime);
        parameters[11] = DepartureImpact(departure);
        parameters[12] = offset.x; parameters[13] = offset.y;
        parameters[14] = strike.blade; parameters[15] = DepartureFade(departure);
    }
    for (size_t index = 0; index < wakePoints.size(); ++index) {
        const auto& point = wakePoints[index];
        const size_t offset = 16 + index * 4;
        parameters[offset] = point.x; parameters[offset + 1] = point.y;
        parameters[offset + 2] = point.z; parameters[offset + 3] = point.w;
    }
    if (stageSeam && !wake) {
        parameters[16]=stageSeam->x; parameters[17]=stageSeam->y;
        parameters[18]=160.0f; parameters[20]=1.0f;
    }
    auto* command = dxCommon_->GetCommandList();
    command->SetGraphicsRootSignature(root_.Get());
    command->SetPipelineState(reveal ? departurePipeline_.Get() : wake ? wakePipeline_.Get() : pipeline_.Get());
    command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    command->SetGraphicsRoot32BitConstants(0, static_cast<UINT>(parameters.size()), parameters.data(), 0);
    command->DrawInstanced(3, 1, 0, 0);
}

// ステージの裂け目の中心、幅、発光をシェーダーへ渡し、画面手前の幕を描く。
void TitleRift::DrawStageCurtain(float elapsed, Math::Vector2 seam, float halfWidth, float heat, float zoom, float rush,
    float kick, const Math::Vector4* projectedEdges)
{
    const auto& viewport=dxCommon_->GetPresentationViewport();
    std::array<float,64> parameters{viewport.Width,viewport.Height,elapsed,0,
        0,0,0,5, seam.x,seam.y,halfWidth,heat,
        kStageDepartureFocus.x,kStageDepartureFocus.y,zoom,rush, kick,0,0,0};
    if (projectedEdges) {
        parameters[20]=1.0f;
        parameters[21]=projectedEdges->x; parameters[22]=projectedEdges->y;
        parameters[23]=projectedEdges->z; parameters[24]=projectedEdges->w;
    }
    auto* command=dxCommon_->GetCommandList();
    command->SetGraphicsRootSignature(root_.Get());
    command->SetPipelineState(departurePipeline_.Get());
    command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    command->SetGraphicsRoot32BitConstants(0,static_cast<UINT>(parameters.size()),parameters.data(),0);
    command->DrawInstanced(3,1,0,0);
}
