#include "engine/2d/SpriteCommon.h"
#include "engine/base/DirectXCommon.h"
#include "engine/3d/TextureManager.h"
#include "engine/base/SrvManager.h"
#include "engine/base/Logger.h"
#include <cassert>

using Microsoft::WRL::ComPtr;

// スプライトが共有する描画システムを受け取り、2D描画用のパイプラインを作る。
void SpriteCommon::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    CreateGraphicsPipelineState();
}

// スプライト用のシェーダーが色、変換行列、画像を読むためのバインド先を定義する。
void SpriteCommon::CreateRootSignature()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    // b0: Material, b1: Transform, t0: Texture, b3: DirectionalLight
    D3D12_ROOT_PARAMETER rootParams[4] = {};

    // b0 - MaterialCB (PS)
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[0].Descriptor.ShaderRegister = 0;

    // b1 - TransformCB (VS)
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParams[1].Descriptor.ShaderRegister = 1;

    // t0 - Texture SRV (PS)
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.BaseShaderRegister = 0;
    range.NumDescriptors = 1;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[2].DescriptorTable.pDescriptorRanges = &range;

    // b3 - DirectionalLightCB (PS)
    rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[3].Descriptor.ShaderRegister = 3;

    // Sampler s0
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;  // s0
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.NumParameters = _countof(rootParams);
    desc.pParameters = rootParams;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;

    Microsoft::WRL::ComPtr<ID3DBlob> sigBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob) {
            Logger::Log(reinterpret_cast<const char*>(errBlob->GetBufferPointer()));
        }
        assert(false);
    }

    hr = device->CreateRootSignature(
        0,
        sigBlob->GetBufferPointer(),
        sigBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}


// スプライト用の頂点とピクセルシェーダー、アルファ合成、頂点形式を設定する。
void SpriteCommon::CreateGraphicsPipelineState()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    CreateRootSignature();  // まず RootSig 作成

    // 入力レイアウト（POSITION, TEXCOORD, NORMAL）
    D3D12_INPUT_ELEMENT_DESC elems[3]{};

    elems[0].SemanticName = "POSITION";
    elems[0].SemanticIndex = 0;
    elems[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    elems[0].AlignedByteOffset = 0;

    elems[1].SemanticName = "TEXCOORD";
    elems[1].SemanticIndex = 0;
    elems[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    elems[1].AlignedByteOffset = 16;

    elems[2].SemanticName = "NORMAL";
    elems[2].SemanticIndex = 0;
    elems[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    elems[2].AlignedByteOffset = 24;

    D3D12_INPUT_LAYOUT_DESC inputLayout{};
    inputLayout.pInputElementDescs = elems;
    inputLayout.NumElements = _countof(elems);

    // ブレンド（スプライトなのでアルファ有効でもいい）
    D3D12_BLEND_DESC blend{};
    blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blend.RenderTarget[0].BlendEnable = TRUE;
    blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;


    // ラスタライザ
    D3D12_RASTERIZER_DESC rast{};
    rast.CullMode = D3D12_CULL_MODE_NONE;
    rast.FillMode = D3D12_FILL_MODE_SOLID;

    // スプライトは Depth 無しでもいい
    D3D12_DEPTH_STENCIL_DESC depth{};
    depth.DepthEnable = FALSE;
    depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depth.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    // Sprite専用のShaderを使う
    auto vs = dxCommon_->CompileShader(L"shaders/Sprite.VS.hlsl", L"vs_6_0");
    auto ps = dxCommon_->CompileShader(L"shaders/Sprite.PS.hlsl", L"ps_6_0");
    assert(vs && ps);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.InputLayout = inputLayout;
    desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    desc.BlendState = blend;
    desc.RasterizerState = rast;
    desc.DepthStencilState = depth;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT hr = device->CreateGraphicsPipelineState(
        &desc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

// 2D画像を描く前に、共有のルートシグネチャと描画設定をコマンドへ登録する。
void SpriteCommon::CommonDrawSetting()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // ❌ 削除
    // ID3D12DescriptorHeap* heaps[] = { srvManager_->GetDescriptorHeap() };
    // commandList->SetDescriptorHeaps(1, heaps);

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
