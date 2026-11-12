// ===== Skybox.cpp =====
// カメラの向きに合わせて、キューブマップをステージの空として描く

#include "engine/3d/Skybox.h"

#include <array>
#include <cassert>
#include <cstring>

#include <d3dx12.h>

#include "engine/3d/Camera.h"
#include "engine/3d/TextureManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/Logger.h"
#include "engine/base/SrvManager.h"

using Microsoft::WRL::ComPtr;

// 背景用のキューブマップを読み、立方体と描画設定を準備する
void Skybox::Initialize(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    const std::string& texturePath)
{
    assert(dxCommon);
    assert(srvManager);

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    texturePath_ = texturePath;

    // Cubemap用DDSを読み込んでSRVを作成
    TextureManager::GetInstance()->LoadTexture(texturePath_);

    CreateRootSignature();
    CreateGraphicsPipelineState();
    CreateVertexBuffer();
    CreateViewProjectionResource();
}

// 現在のカメラに合わせて空の描画行列を作り、移動で空の見え方がずれないようにする
void Skybox::Update(const Camera* camera)
{
    assert(camera);
    assert(viewProjectionData_);

    // Skyboxはカメラの移動には追従させず、回転だけ反映する
    Math::Matrix4x4 view = camera->GetViewMatrix();
    view.m[3][0] = 0.0f;
    view.m[3][1] = 0.0f;
    view.m[3][2] = 0.0f;

    const Math::Matrix4x4 viewProjection =
        Math::Multiply(view, camera->GetProjectionMatrix());

    viewProjectionData_->viewProjection =
        Math::Transpose(viewProjection);
}

// キューブマップとビュー投影を渡し、遠景の空を描く
void Skybox::Draw()
{
    auto* commandList = dxCommon_->GetCommandList();

    // Skybox専用のRootSignature/PSOを設定
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    commandList->SetGraphicsRootConstantBufferView(
        0,
        viewProjectionResource_->GetGPUVirtualAddress()
    );

    srvManager_->SetGraphicsRootDescriptorTable(
        1,
        TextureManager::GetInstance()->GetSrvIndex(texturePath_)
    );

    commandList->DrawInstanced(vertexCount_, 1, 0, 0);
}

// 空を描くシェーダーが行列とキューブマップを読むための参照先を定義する
void Skybox::CreateRootSignature()
{
    auto* device = dxCommon_->GetDevice();

    // b0: ViewProjectionCB (VertexShader)
    D3D12_ROOT_PARAMETER rootParameters[2] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    // t0: Cubemap texture (PixelShader)
    D3D12_DESCRIPTOR_RANGE descriptorRange{};
    descriptorRange.BaseShaderRegister = 0;
    descriptorRange.NumDescriptors = 1;
    descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[1].ParameterType =
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRange;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;

    // Cubemapは端で繰り返さず、境界をClampする
    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    desc.pParameters = rootParameters;
    desc.NumParameters = _countof(rootParameters);
    desc.pStaticSamplers = &staticSampler;
    desc.NumStaticSamplers = 1;

    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob
    );
    if (FAILED(hr)) {
        Logger::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }

    hr = device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_)
    );
    assert(SUCCEEDED(hr));
}

// 空を遠景として描くシェーダー、頂点形式、深度の設定を作る
void Skybox::CreateGraphicsPipelineState()
{
    auto* device = dxCommon_->GetDevice();

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        {
            "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
        },
    };

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    // BlendState
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;

    // 箱の内側から見るのでカリングは行わない
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // 背景なので深度値は比較するが、DepthBufferには書き込まない
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = true;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    auto vertexShaderBlob =
        dxCommon_->CompileShader(L"shaders/Skybox.VS.hlsl", L"vs_6_0");
    auto pixelShaderBlob =
        dxCommon_->CompileShader(L"shaders/Skybox.PS.hlsl", L"ps_6_0");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature_.Get();
    desc.InputLayout = inputLayoutDesc;
    desc.VS = {
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize()
    };
    desc.PS = {
        pixelShaderBlob->GetBufferPointer(),
        pixelShaderBlob->GetBufferSize()
    };
    desc.BlendState = blendDesc;
    desc.RasterizerState = rasterizerDesc;
    desc.DepthStencilState = depthStencilDesc;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.SampleDesc.Count = 1;
    desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT hr = device->CreateGraphicsPipelineState(
        &desc,
        IID_PPV_ARGS(&pipelineState_)
    );
    assert(SUCCEEDED(hr));
}

// キューブマップを方向で読むための立方体の頂点を用意する
void Skybox::CreateVertexBuffer()
{
    // キューブの頂点座標を、そのままCubemapのサンプリング方向として使う
    constexpr float size = 1.0f;
    const std::array<VertexData, 36> vertices = {
        VertexData{{-size,  size, -size, 1.0f}},
        VertexData{{ size,  size, -size, 1.0f}},
        VertexData{{ size, -size, -size, 1.0f}},
        VertexData{{-size,  size, -size, 1.0f}},
        VertexData{{ size, -size, -size, 1.0f}},
        VertexData{{-size, -size, -size, 1.0f}},

        VertexData{{ size,  size,  size, 1.0f}},
        VertexData{{-size,  size,  size, 1.0f}},
        VertexData{{-size, -size,  size, 1.0f}},
        VertexData{{ size,  size,  size, 1.0f}},
        VertexData{{-size, -size,  size, 1.0f}},
        VertexData{{ size, -size,  size, 1.0f}},

        VertexData{{-size,  size,  size, 1.0f}},
        VertexData{{-size,  size, -size, 1.0f}},
        VertexData{{-size, -size, -size, 1.0f}},
        VertexData{{-size,  size,  size, 1.0f}},
        VertexData{{-size, -size, -size, 1.0f}},
        VertexData{{-size, -size,  size, 1.0f}},

        VertexData{{ size,  size, -size, 1.0f}},
        VertexData{{ size,  size,  size, 1.0f}},
        VertexData{{ size, -size,  size, 1.0f}},
        VertexData{{ size,  size, -size, 1.0f}},
        VertexData{{ size, -size,  size, 1.0f}},
        VertexData{{ size, -size, -size, 1.0f}},

        VertexData{{-size,  size,  size, 1.0f}},
        VertexData{{ size,  size,  size, 1.0f}},
        VertexData{{ size,  size, -size, 1.0f}},
        VertexData{{-size,  size,  size, 1.0f}},
        VertexData{{ size,  size, -size, 1.0f}},
        VertexData{{-size,  size, -size, 1.0f}},

        VertexData{{-size, -size, -size, 1.0f}},
        VertexData{{ size, -size, -size, 1.0f}},
        VertexData{{ size, -size,  size, 1.0f}},
        VertexData{{-size, -size, -size, 1.0f}},
        VertexData{{ size, -size,  size, 1.0f}},
        VertexData{{-size, -size,  size, 1.0f}},
    };

    vertexCount_ = static_cast<uint32_t>(vertices.size());
    vertexResource_ =
        dxCommon_->CreateBufferResource(sizeof(VertexData) * vertices.size());

    VertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    memcpy(vertexData, vertices.data(), sizeof(VertexData) * vertices.size());
    vertexResource_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes =
        UINT(sizeof(VertexData) * vertices.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

// 空のカメラ行列をGPUへ渡す定数バッファを作る
void Skybox::CreateViewProjectionResource()
{
    viewProjectionResource_ =
        dxCommon_->CreateBufferResource(sizeof(ViewProjectionData));
    viewProjectionResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&viewProjectionData_)
    );
    viewProjectionData_->viewProjection = Math::MakeIdentity4x4();
}
