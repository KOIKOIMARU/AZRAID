#include "engine/base/DirectXCommon.h"
#include "engine/base/Logger.h"
#include "engine/base/SrvManager.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <format>
#include <dxcapi.h>            // DXC（dxcUtils, dxcCompiler, IncludeHandler）
#include <vector>              // arguments 用
#include <string>              // wstring
#include <chrono>
#include <stdexcept>
#include <thread>   // sleep_for 用


#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include <d3dx12.h>
#include <filesystem>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")
using namespace Microsoft::WRL;
using Logger::Log;

namespace {

constexpr Math::Vector4 kRenderTextureClearColor{ 0.04f, 0.06f, 0.09f, 1.0f };

float ToMilliseconds(
    const std::chrono::steady_clock::time_point& begin,
    const std::chrono::steady_clock::time_point& end)
{
    return std::chrono::duration<float, std::milli>(end - begin).count();
}

std::string WideToUtf8(const wchar_t* text)
{
    if (!text || *text == L'\0') {
        return {};
    }

    const int requiredSize = WideCharToMultiByte(
        CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (requiredSize <= 1) {
        return {};
    }

    std::string result(static_cast<size_t>(requiredSize), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, text, -1, result.data(), requiredSize,
        nullptr, nullptr);
    result.resize(static_cast<size_t>(requiredSize - 1));
    return result;
}

std::string GetDredObjectName(const char* ansiName, const wchar_t* wideName)
{
    if (ansiName && *ansiName != '\0') {
        return ansiName;
    }
    const std::string convertedName = WideToUtf8(wideName);
    return convertedName.empty() ? "<unnamed>" : convertedName;
}

const char* GetBreadcrumbOperationName(D3D12_AUTO_BREADCRUMB_OP operation)
{
    switch (operation) {
    case D3D12_AUTO_BREADCRUMB_OP_SETMARKER: return "SetMarker";
    case D3D12_AUTO_BREADCRUMB_OP_BEGINEVENT: return "BeginEvent";
    case D3D12_AUTO_BREADCRUMB_OP_ENDEVENT: return "EndEvent";
    case D3D12_AUTO_BREADCRUMB_OP_DRAWINSTANCED: return "DrawInstanced";
    case D3D12_AUTO_BREADCRUMB_OP_DRAWINDEXEDINSTANCED:
        return "DrawIndexedInstanced";
    case D3D12_AUTO_BREADCRUMB_OP_EXECUTEINDIRECT: return "ExecuteIndirect";
    case D3D12_AUTO_BREADCRUMB_OP_DISPATCH: return "Dispatch";
    case D3D12_AUTO_BREADCRUMB_OP_COPYBUFFERREGION:
        return "CopyBufferRegion";
    case D3D12_AUTO_BREADCRUMB_OP_COPYTEXTUREREGION:
        return "CopyTextureRegion";
    case D3D12_AUTO_BREADCRUMB_OP_COPYRESOURCE: return "CopyResource";
    case D3D12_AUTO_BREADCRUMB_OP_RESOLVESUBRESOURCE:
        return "ResolveSubresource";
    case D3D12_AUTO_BREADCRUMB_OP_CLEARRENDERTARGETVIEW:
        return "ClearRenderTargetView";
    case D3D12_AUTO_BREADCRUMB_OP_CLEARUNORDEREDACCESSVIEW:
        return "ClearUnorderedAccessView";
    case D3D12_AUTO_BREADCRUMB_OP_CLEARDEPTHSTENCILVIEW:
        return "ClearDepthStencilView";
    case D3D12_AUTO_BREADCRUMB_OP_RESOURCEBARRIER: return "ResourceBarrier";
    case D3D12_AUTO_BREADCRUMB_OP_EXECUTEBUNDLE: return "ExecuteBundle";
    case D3D12_AUTO_BREADCRUMB_OP_PRESENT: return "Present";
    case D3D12_AUTO_BREADCRUMB_OP_RESOLVEQUERYDATA:
        return "ResolveQueryData";
    case D3D12_AUTO_BREADCRUMB_OP_BEGINSUBMISSION: return "BeginSubmission";
    case D3D12_AUTO_BREADCRUMB_OP_ENDSUBMISSION: return "EndSubmission";
    case D3D12_AUTO_BREADCRUMB_OP_WRITEBUFFERIMMEDIATE:
        return "WriteBufferImmediate";
    case D3D12_AUTO_BREADCRUMB_OP_BUILDRAYTRACINGACCELERATIONSTRUCTURE:
        return "BuildRaytracingAccelerationStructure";
    case D3D12_AUTO_BREADCRUMB_OP_DISPATCHRAYS: return "DispatchRays";
    case D3D12_AUTO_BREADCRUMB_OP_DISPATCHMESH: return "DispatchMesh";
    case D3D12_AUTO_BREADCRUMB_OP_BARRIER: return "Barrier";
    case D3D12_AUTO_BREADCRUMB_OP_BEGIN_COMMAND_LIST:
        return "BeginCommandList";
    default: return "Unknown";
    }
}

const char* GetMessageSeverityName(D3D12_MESSAGE_SEVERITY severity)
{
    switch (severity) {
    case D3D12_MESSAGE_SEVERITY_CORRUPTION: return "CORRUPTION";
    case D3D12_MESSAGE_SEVERITY_ERROR: return "ERROR";
    case D3D12_MESSAGE_SEVERITY_WARNING: return "WARNING";
    case D3D12_MESSAGE_SEVERITY_INFO: return "INFO";
    case D3D12_MESSAGE_SEVERITY_MESSAGE: return "MESSAGE";
    default: return "UNKNOWN";
    }
}

}

DirectXCommon::~DirectXCommon()
{
    FlushDebugMessages();
    if (device_) {
        const HRESULT removedReason = device_->GetDeviceRemovedReason();
        if (FAILED(removedReason)) {
            ReportDeviceRemovedDiagnostics("DirectXCommon shutdown", removedReason);
        }
    }
}

void DirectXCommon::Initialize(WinApp* winApp)
{
    // WinAppを覚えておく
    assert(winApp);
    this->winApp_ = winApp;

    InitializeDiagnosticLog();
    InitializeDevice();      // デバイス・DXGI
    InitializeCommand();     // コマンドキュー/アロケータ/リスト
    InitializeSwapChain();   // ★ スワップチェーン
    InitializeDepthBuffer();   // ★ 深度バッフ
    InitializeDescriptorHeaps();
    InitializeRenderTargetView();
    InitializeDepthStencilView();
    InitializeFence();
    InitializeViewport();       // ★ここで初期化
    InitializeScissorRect();
    InitializeDXC();

    // ★ FPS 固定の初期化
    InitializeFixFPS();
}


void DirectXCommon::ResizePresentation()
{
    RECT client{};
    if (!winApp_ || !GetClientRect(winApp_->GetHwnd(), &client)) { return; }
    const UINT width = static_cast<UINT>((std::max)(client.right, 0L));
    const UINT height = static_cast<UINT>((std::max)(client.bottom, 0L));
    if (width == 0 || height == 0 || (width == presentationWidth_ && height == presentationHeight_)) { return; }

    // 前フレーム・転送処理の参照が残る間は解放しない。ResizeBuffersはフレーム外だけで行う。
    CheckDeviceOperation(commandQueue_->Signal(fence_.Get(), ++fenceValue_), "Resize::Signal");
    if (fence_->GetCompletedValue() < fenceValue_) {
        CheckDeviceOperation(fence_->SetEventOnCompletion(fenceValue_, fenceEvent_), "Resize::Fence");
        const DWORD result = WaitForSingleObject(fenceEvent_, INFINITE);
        CheckDeviceOperation(result == WAIT_OBJECT_0 ? S_OK : HRESULT_FROM_WIN32(ERROR_GEN_FAILURE), "Resize::Wait");
    }
    CheckDeviceOperation(commandAllocator_->Reset(), "Resize::AllocatorReset");
    CheckDeviceOperation(commandList_->Reset(commandAllocator_.Get(), nullptr), "Resize::ListReset");
    CheckDeviceOperation(commandList_->Close(), "Resize::ListClose");
    for (auto& resource : swapChainResources_) { resource.Reset(); }
    CheckDeviceOperation(swapChain_->ResizeBuffers(kBackBufferCount, width, height,
        DXGI_FORMAT_R8G8B8A8_UNORM, 0), "SwapChain::ResizeBuffers");
    InitializeRenderTargetView();
    currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
    presentationWidth_ = width;
    presentationHeight_ = height;
    // 内部解像度は維持し、表示先はクライアント領域全体を使う。
    // カメラも表示先の縦横比を使うため、機体を横に引き伸ばさず黒帯だけをなくせる。
    presentationViewport_ = { 0, 0, static_cast<float>(width), static_cast<float>(height), 0, 1 };
    presentationScissor_ = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    FlushDebugMessages();
}

void DirectXCommon::PreDraw() {
    assert(renderTextureResource_);

    // 0. コマンドリストを描画用に準備
    HRESULT hr = S_OK;
    hr = commandAllocator_->Reset();
    CheckDeviceOperation(hr, "CommandAllocator::Reset");
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    CheckDeviceOperation(hr, "GraphicsCommandList::Reset");

    // バックバッファの番号取得
    currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
    UINT bbIndex = currentBackBufferIndex_;

    if (renderTextureState_ != D3D12_RESOURCE_STATE_RENDER_TARGET) {
        D3D12_RESOURCE_BARRIER renderTextureBarrier{};
        renderTextureBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        renderTextureBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        renderTextureBarrier.Transition.pResource = renderTextureResource_.Get();
        renderTextureBarrier.Transition.StateBefore = renderTextureState_;
        renderTextureBarrier.Transition.StateAfter =
            D3D12_RESOURCE_STATE_RENDER_TARGET;
        renderTextureBarrier.Transition.Subresource =
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList_->ResourceBarrier(1, &renderTextureBarrier);
        renderTextureState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    // 2. TransitionBarrier PRESENT → RENDER_TARGET
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources_[bbIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    // 3. RTV & DSV の設定
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetCPUDescriptorHandle(
        rtvHeap_,
        rtvDescriptorSize_,
        kRenderTextureRTVIndex
    );

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
        dsvHeap_->GetCPUDescriptorHandleForHeapStart();

    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // 4. クリア
    float clearColor[] = {
        kRenderTextureClearColor.x,
        kRenderTextureClearColor.y,
        kRenderTextureClearColor.z,
        kRenderTextureClearColor.w
    };
    commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    commandList_->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr
    );

    // 5. ビューポート & シザー
    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCommon::DrawRenderTextureToSwapChain(int postEffectMode)
{
    assert(renderTextureResource_);
    assert(postEffectTextureResource_);
    assert(fullscreenRootSignature_);
    assert(copyImagePipelineState_);
    assert(grayscalePipelineState_);
    assert(vignettePipelineState_);
    assert(boxFilterPipelineState_);
    assert(boxFilter5x5PipelineState_);
    assert(gaussianFilterPipelineState_);
    assert(vignetteSmoothingPipelineState_);
    assert(luminanceOutlinePipelineState_);
    assert(depthOutlinePipelineState_);
    assert(radialBlurPipelineState_);
    assert(dissolvePipelineState_);
    assert(randomPipelineState_);
    assert(gameTonePipelineState_);
    assert(bloomPipelineState_);
    assert(bloomExtractPipelineState_);
    assert(bloomBlurPipelineState_);
    assert(bloomDownsamplePipelineState_);
    assert(bloomUpsamplePipelineState_);
    assert(bloomCompositePipelineState_);
    assert(srvDescriptorHeap_);

    const bool useDissolve = postEffectMode == 9;
    const bool useRandom = postEffectMode == 10;
    const bool useGameTone =
        12 <= postEffectMode && postEffectMode <= 15;
    const bool useDepthTexture = postEffectMode == 7 || useGameTone;
    if (useDissolve) {
        dissolveThreshold_ += deltaTime_ * 0.25f;
        if (dissolveThreshold_ > 1.0f) {
            dissolveThreshold_ = 0.0f;
        }
        dissolveParameterData_->threshold = dissolveThreshold_;
    }
    if (useRandom) {
        randomTime_ += deltaTime_;
        randomParameterData_->time = randomTime_;
    }
    if (useGameTone) {
        gameToneParameterData_->vignetteStrength = 0.36f;
        gameToneParameterData_->saturation = 1.10f;
        gameToneParameterData_->contrast = 1.10f;
        gameToneParameterData_->damageTint = 0.0f;
        gameToneParameterData_->fogStart = 72.0f;
        gameToneParameterData_->fogEnd = 330.0f;
        gameToneParameterData_->fogStrength = 0.46f;
        gameToneParameterData_->horizonFogStrength = 0.035f;
        gameToneParameterData_->exposure = 1.02f;
        gameToneParameterData_->blackPoint = 0.006f;
        gameToneParameterData_->highlightCompression = 0.50f;
        gameToneParameterData_->colorTemperature = 0.050f;
        if (postEffectMode == 13) {
            gameToneParameterData_->vignetteStrength = 0.92f;
            gameToneParameterData_->saturation = 0.86f;
            gameToneParameterData_->contrast = 1.22f;
            gameToneParameterData_->damageTint = 0.24f;
            gameToneParameterData_->fogStrength = 0.54f;
            gameToneParameterData_->horizonFogStrength = 0.08f;
            gameToneParameterData_->exposure = 1.02f;
            gameToneParameterData_->blackPoint = 0.028f;
            gameToneParameterData_->highlightCompression = 0.42f;
            gameToneParameterData_->colorTemperature = -0.04f;
        } else if (postEffectMode == 14) {
            gameToneParameterData_->vignetteStrength = 0.36f;
            gameToneParameterData_->saturation = 1.13f;
            gameToneParameterData_->contrast = 1.15f;
            gameToneParameterData_->damageTint = 0.0f;
            gameToneParameterData_->fogStrength = 0.40f;
            gameToneParameterData_->horizonFogStrength = 0.045f;
            gameToneParameterData_->exposure = 1.09f;
            gameToneParameterData_->blackPoint = 0.012f;
            gameToneParameterData_->highlightCompression = 0.32f;
            gameToneParameterData_->colorTemperature = 0.09f;
        } else if (postEffectMode == 15) {
            gameToneParameterData_->vignetteStrength = 1.12f;
            gameToneParameterData_->saturation = 0.28f;
            gameToneParameterData_->contrast = 1.22f;
            gameToneParameterData_->damageTint = 0.18f;
            gameToneParameterData_->fogStrength = 0.60f;
            gameToneParameterData_->horizonFogStrength = 0.10f;
            gameToneParameterData_->exposure = 0.92f;
            gameToneParameterData_->blackPoint = 0.050f;
            gameToneParameterData_->highlightCompression = 0.56f;
            gameToneParameterData_->colorTemperature = -0.10f;
        }
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = renderTextureResource_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);
    renderTextureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    if (postEffectMode == 16) {
        DrawBloomPasses(
            renderTextureSrvIndex_,
            WinApp::kClientWidth,
            WinApp::kClientHeight,
            0.58f,
            0.76f);
        DrawBloomCompositeToBackBuffer(renderTextureSrvIndex_, 0.86f);
        return;
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap_.Get() };
    if (postEffectTextureState_ != D3D12_RESOURCE_STATE_RENDER_TARGET) {
        D3D12_RESOURCE_BARRIER postEffectBarrier{};
        postEffectBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        postEffectBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        postEffectBarrier.Transition.pResource =
            postEffectTextureResource_.Get();
        postEffectBarrier.Transition.StateBefore = postEffectTextureState_;
        postEffectBarrier.Transition.StateAfter =
            D3D12_RESOURCE_STATE_RENDER_TARGET;
        postEffectBarrier.Transition.Subresource =
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList_->ResourceBarrier(1, &postEffectBarrier);
        postEffectTextureState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    D3D12_RESOURCE_BARRIER depthBarrier{};
    if (useDepthTexture) {
        depthBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        depthBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        depthBarrier.Transition.pResource = depthStencilResource_.Get();
        depthBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        depthBarrier.Transition.StateAfter =
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        depthBarrier.Transition.Subresource =
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList_->ResourceBarrier(1, &depthBarrier);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE postEffectRtvHandle = GetCPUDescriptorHandle(
        rtvHeap_,
        rtvDescriptorSize_,
        kPostEffectTextureRTVIndex
    );
    commandList_->OMSetRenderTargets(1, &postEffectRtvHandle, FALSE, nullptr);
    float clearColor[] = {
        kRenderTextureClearColor.x,
        kRenderTextureClearColor.y,
        kRenderTextureClearColor.z,
        kRenderTextureClearColor.w
    };
    commandList_->ClearRenderTargetView(
        postEffectRtvHandle,
        clearColor,
        0,
        nullptr
    );

    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);

    commandList_->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList_->SetGraphicsRootSignature(fullscreenRootSignature_.Get());
    ID3D12PipelineState* pipelineState = copyImagePipelineState_.Get();
    if (postEffectMode == 1) {
        pipelineState = grayscalePipelineState_.Get();
    } else if (postEffectMode == 2) {
        pipelineState = vignettePipelineState_.Get();
    } else if (postEffectMode == 3) {
        pipelineState = boxFilterPipelineState_.Get();
    } else if (postEffectMode == 4) {
        pipelineState = boxFilter5x5PipelineState_.Get();
    } else if (postEffectMode == 5) {
        pipelineState = gaussianFilterPipelineState_.Get();
    } else if (postEffectMode == 6) {
        pipelineState = luminanceOutlinePipelineState_.Get();
    } else if (postEffectMode == 7) {
        pipelineState = depthOutlinePipelineState_.Get();
    } else if (postEffectMode == 8) {
        pipelineState = radialBlurPipelineState_.Get();
    } else if (postEffectMode == 9) {
        pipelineState = dissolvePipelineState_.Get();
    } else if (postEffectMode == 10) {
        pipelineState = randomPipelineState_.Get();
    } else if (postEffectMode == 11) {
        pipelineState = vignetteSmoothingPipelineState_.Get();
    } else if (useGameTone) {
        pipelineState = gameTonePipelineState_.Get();
    }
    commandList_->SetPipelineState(pipelineState);
    commandList_->SetGraphicsRootDescriptorTable(
        0,
        GetGPUDescriptorHandle(
            srvDescriptorHeap_,
            device_->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            ),
            renderTextureSrvIndex_
        )
    );
    commandList_->SetGraphicsRootDescriptorTable(
        1,
        GetGPUDescriptorHandle(
            srvDescriptorHeap_,
            device_->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            ),
            useDissolve ? dissolveMaskTextureSrvIndex_ : depthTextureSrvIndex_
        )
    );
    commandList_->SetGraphicsRootConstantBufferView(
        2,
        useGameTone ?
        gameToneParameterResource_->GetGPUVirtualAddress() :
        useDepthTexture ?
        depthOutlineParameterResource_->GetGPUVirtualAddress() :
        useRandom ?
        randomParameterResource_->GetGPUVirtualAddress() :
        useDissolve ?
        dissolveParameterResource_->GetGPUVirtualAddress() :
        radialBlurParameterResource_->GetGPUVirtualAddress()
    );
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->DrawInstanced(3, 1, 0, 0);

    D3D12_RESOURCE_BARRIER postEffectBarrier{};
    postEffectBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    postEffectBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    postEffectBarrier.Transition.pResource = postEffectTextureResource_.Get();
    postEffectBarrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_RENDER_TARGET;
    postEffectBarrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    postEffectBarrier.Transition.Subresource =
        D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &postEffectBarrier);
    postEffectTextureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    if (useDepthTexture) {
        depthBarrier.Transition.StateBefore =
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        depthBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        commandList_->ResourceBarrier(1, &depthBarrier);
    }

    if (useGameTone) {
        const float bloomThreshold = postEffectMode == 15 ? 0.94f : 0.90f;
        const float bloomIntensity = postEffectMode == 15 ? 0.14f : 0.26f;
        DrawBloomPasses(
            postEffectTextureSrvIndex_,
            WinApp::kClientWidth,
            WinApp::kClientHeight,
            bloomThreshold,
            0.72f);
        DrawBloomCompositeToBackBuffer(
            postEffectTextureSrvIndex_,
            bloomIntensity);
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetCPUDescriptorHandle(
        rtvHeap_,
        rtvDescriptorSize_,
        currentBackBufferIndex_
    );
    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    const float black[] = { 0, 0, 0, 1 };
    commandList_->ClearRenderTargetView(rtvHandle, black, 0, nullptr);
    commandList_->RSSetViewports(1, &presentationViewport_);
    commandList_->RSSetScissorRects(1, &presentationScissor_);

    commandList_->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList_->SetGraphicsRootSignature(fullscreenRootSignature_.Get());
    commandList_->SetPipelineState(copyImagePipelineState_.Get());
    commandList_->SetGraphicsRootDescriptorTable(
        0,
        GetGPUDescriptorHandle(
            srvDescriptorHeap_,
            device_->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            ),
            postEffectTextureSrvIndex_
        )
    );
    commandList_->SetGraphicsRootDescriptorTable(
        1,
        GetGPUDescriptorHandle(
            srvDescriptorHeap_,
            device_->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
            ),
            useDissolve ? dissolveMaskTextureSrvIndex_ : depthTextureSrvIndex_
        )
    );
    commandList_->SetGraphicsRootConstantBufferView(
        2,
        useGameTone ?
        gameToneParameterResource_->GetGPUVirtualAddress() :
        useDepthTexture ?
        depthOutlineParameterResource_->GetGPUVirtualAddress() :
        useRandom ?
        randomParameterResource_->GetGPUVirtualAddress() :
        useDissolve ?
        dissolveParameterResource_->GetGPUVirtualAddress() :
        radialBlurParameterResource_->GetGPUVirtualAddress()
    );
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->DrawInstanced(3, 1, 0, 0);

}

D3D12_GPU_DESCRIPTOR_HANDLE
DirectXCommon::GetRenderTextureGpuDescriptorHandle() const
{
    assert(srvDescriptorHeap_);

    return GetGPUDescriptorHandle(
        srvDescriptorHeap_,
        device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
        postEffectTextureSrvIndex_);
}

Math::Vector2 DirectXCommon::GetRenderTextureSize() const
{
    return {
        static_cast<float>(WinApp::kClientWidth),
        static_cast<float>(WinApp::kClientHeight)
    };
}

void DirectXCommon::SetPostEffectProjectionMatrix(
    const Math::Matrix4x4& projectionMatrix)
{
    const Math::Matrix4x4 projectionInverse =
        Math::Transpose(Math::Inverse(projectionMatrix));

    if (depthOutlineParameterData_) {
        depthOutlineParameterData_->projectionInverse = projectionInverse;
    }
    if (gameToneParameterData_) {
        gameToneParameterData_->projectionInverse = projectionInverse;
    }
}


void DirectXCommon::SetFlightBlur(float strength, Math::Vector2 center,
    Math::Vector2 player, Math::Vector2 reticle, float aspect)
{
    if (!gameToneParameterData_) { return; }
    gameToneParameterData_->flightBlurStrength = std::clamp(strength, 0.0f, 0.85f);
    gameToneParameterData_->flightBlurCenter = center;
    gameToneParameterData_->flightBlurPlayer = player;
    gameToneParameterData_->flightBlurReticle = reticle;
    gameToneParameterData_->flightBlurAspect = std::clamp(aspect, 0.5f, 4.0f);
}

void DirectXCommon::PostDraw()
{
    const auto postDrawBegin = std::chrono::steady_clock::now();
    HRESULT hr = S_OK;

    // バックバッファの番号取得
    UINT bbIndex = currentBackBufferIndex_; // ← 絶対に GetCurrentBackBufferIndex() を呼ばない


    // RenderTarget → Present へのリソースバリア
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources_[bbIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    // コマンドリストをクローズ
    hr = commandList_->Close();
    CheckDeviceOperation(hr, "GraphicsCommandList::Close");

    // GPU コマンドの実行
    ID3D12CommandList* cmdLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    // 画面のフリップ（Present）
    const auto presentBegin = std::chrono::steady_clock::now();
    hr = swapChain_->Present(1, 0);
    CheckDeviceOperation(hr, "SwapChain::Present");
    const auto presentEnd = std::chrono::steady_clock::now();
    frameTiming_.presentMs = ToMilliseconds(presentBegin, presentEnd);

    // Fence の値更新 & Signal
    fenceValue_++;
    hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
    CheckDeviceOperation(hr, "CommandQueue::Signal");

    // コマンド完了待ち
    const auto fenceBegin = std::chrono::steady_clock::now();
    const UINT64 completedValue = fence_->GetCompletedValue();
    if (completedValue == UINT64_MAX) {
        HRESULT removedReason = device_->GetDeviceRemovedReason();
        if (SUCCEEDED(removedReason)) {
            removedReason = DXGI_ERROR_DEVICE_REMOVED;
        }
        CheckDeviceOperation(removedReason, "Fence::GetCompletedValue");
    }
    if (completedValue < fenceValue_) {
        hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        CheckDeviceOperation(hr, "Fence::SetEventOnCompletion");
        const DWORD waitResult = WaitForSingleObject(fenceEvent_, INFINITE);
        if (waitResult != WAIT_OBJECT_0) {
            const DWORD error = waitResult == WAIT_FAILED
                ? GetLastError()
                : ERROR_GEN_FAILURE;
            CheckDeviceOperation(
                HRESULT_FROM_WIN32(error),
                "WaitForSingleObject(fence)");
        }
    }
    const auto fenceEnd = std::chrono::steady_clock::now();
    frameTiming_.fenceWaitMs = ToMilliseconds(fenceBegin, fenceEnd);
    FlushDebugMessages();

    // ★ ここで FPS 固定
    UpdateFixFPS();
    const auto postDrawEnd = std::chrono::steady_clock::now();
    frameTiming_.postDrawMs = ToMilliseconds(postDrawBegin, postDrawEnd);


}

void DirectXCommon::InitializeDevice() {

#ifdef _DEBUG
    ConfigureDred();

    ComPtr<ID3D12Debug1> debugController = nullptr;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        AppendDiagnosticLog("D3D12_DEBUG_LAYER enabled=1");
#ifdef ENABLE_D3D12_GPU_VALIDATION
        debugController->SetEnableGPUBasedValidation(true);
        AppendDiagnosticLog("D3D12_GPU_BASED_VALIDATION enabled=1");
#else
        AppendDiagnosticLog("D3D12_GPU_BASED_VALIDATION enabled=0");
#endif
    } else {
        AppendDiagnosticLog("D3D12_DEBUG_LAYER enabled=0");
    }
#endif

    // DXGIファクトリーの生成（★ メンバに直接作る）
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory_));
    assert(SUCCEEDED(hr));

    // 使用するアダプタ用の変数
    ComPtr<IDXGIAdapter4> useAdapter = nullptr;
    // 良い順にアダプタを探す
    for (UINT i = 0;
        dxgiFactory_->EnumAdapterByGpuPreference(
            i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND;
        ++i) {

        DXGI_ADAPTER_DESC3 adapterDesc{};
        hr = useAdapter->GetDesc3(&adapterDesc);
        assert(SUCCEEDED(hr));

        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            break;
        }
        useAdapter = nullptr;
    }
    assert(useAdapter != nullptr);

    // ★ 一時変数じゃなくてそのまま device_ に作る
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0
    };
    const char* featureLevelStrings[] = { "12.2","12.1","12.0" };

    for (size_t i = 0; i < _countof(featureLevels); ++i) {
        hr = D3D12CreateDevice(
            useAdapter.Get(),
            featureLevels[i],
            IID_PPV_ARGS(&device_));
        if (SUCCEEDED(hr)) {
            Log(std::format("Feature Level: {}\n", featureLevelStrings[i]));
            break;
        }
    }
    assert(device_ != nullptr);
    Log("Complete create D3D12Device!!!\n");

#ifdef _DEBUG
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue_)))) {
        const bool breakOnSevereMessage = IsDebuggerPresent() != FALSE;
        infoQueue_->SetBreakOnSeverity(
            D3D12_MESSAGE_SEVERITY_CORRUPTION, breakOnSevereMessage);
        infoQueue_->SetBreakOnSeverity(
            D3D12_MESSAGE_SEVERITY_ERROR, breakOnSevereMessage);
        infoQueue_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);

        D3D12_MESSAGE_ID denyIds[] = {
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
        };
        D3D12_MESSAGE_SEVERITY severities[] = {
            D3D12_MESSAGE_SEVERITY_INFO,
            D3D12_MESSAGE_SEVERITY_MESSAGE
        };
        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        infoQueue_->PushStorageFilter(&filter);
        AppendDiagnosticLog(std::format(
            "D3D12_INFO_QUEUE enabled=1 break_on_error={}",
            breakOnSevereMessage ? 1 : 0));
    } else {
        AppendDiagnosticLog("D3D12_INFO_QUEUE enabled=0");
    }
#endif
}

void DirectXCommon::InitializeCommand()
{
    assert(device_);  // 念のため

    HRESULT hr{};

    // コマンドキュー生成
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
    commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    hr = device_->CreateCommandQueue(
        &commandQueueDesc,
        IID_PPV_ARGS(&commandQueue_));
    assert(SUCCEEDED(hr));
    commandQueue_->SetName(L"CG2 Main Direct Queue");

    // コマンドアロケータ生成
    hr = device_->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&commandAllocator_));
    assert(SUCCEEDED(hr));
    commandAllocator_->SetName(L"CG2 Main Command Allocator");

    // コマンドリスト生成
    hr = device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocator_.Get(),
        nullptr,
        IID_PPV_ARGS(&commandList_));
    assert(SUCCEEDED(hr));
    commandList_->SetName(L"CG2 Main Graphics Command List");

    // いったん Close しておく（毎フレーム Reset して使う想定）
    hr = commandList_->Close();
    assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeSwapChain()
{
    assert(dxgiFactory_);
    assert(commandQueue_);
    assert(winApp_);

    HRESULT hr{};

    // スワップチェーンの設定
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = WinApp::kClientWidth;   // 画面の幅
    swapChainDesc.Height = WinApp::kClientHeight;  // 画面の高さ
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;                       // マルチサンプルなし
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;                       // ダブルバッファ
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;

    // 一旦 IDXGISwapChain1 として作成
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1 = nullptr;
    hr = dxgiFactory_->CreateSwapChainForHwnd(
        commandQueue_.Get(),          // コマンドキュー
        winApp_->GetHwnd(),           // ウィンドウハンドル
        &swapChainDesc,               // 設定
        nullptr,                      // フルスクリーンの設定（今回は使わない）
        nullptr,                      // 出力先モニタ
        &swapChain1);                 // 返ってくるスワップチェーン
    assert(SUCCEEDED(hr));

    // IDXGISwapChain4 にキャストしてメンバに保持
    hr = swapChain1.As(&swapChain_);
    assert(SUCCEEDED(hr));
    // Alt+EnterはWinAppの枠なし全画面で処理し、DXGIの排他的切り替えと競合させない。
    CheckDeviceOperation(dxgiFactory_->MakeWindowAssociation(winApp_->GetHwnd(), DXGI_MWA_NO_ALT_ENTER),
        "DXGI::MakeWindowAssociation");
}

void DirectXCommon::InitializeDepthBuffer()
{
    assert(device_);  // 念のため

    // 生成するResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = WinApp::kClientWidth;   // 画面サイズに合わせる
    resourceDesc.Height = WinApp::kClientHeight;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;         // 深度24bit + ステンシル8bit
    resourceDesc.SampleDesc.Count = 1;                                      // MSAAなし
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    // ヒーププロパティ
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    // 初期クリア値
    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthClearValue.DepthStencil.Depth = 1.0f;
    depthClearValue.DepthStencil.Stencil = 0;

    // リソース生成（★ 結果だけメンバに持つ）
    HRESULT hr = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClearValue,
        IID_PPV_ARGS(&depthStencilResource_));
    assert(SUCCEEDED(hr));
}

ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(
    D3D12_DESCRIPTOR_HEAP_TYPE heapType,
    UINT numDescriptors,
    bool shaderVisible)
{
    assert(device_);

    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = heapType;
    desc.NumDescriptors = numDescriptors;
    desc.Flags = shaderVisible
        ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NodeMask = 0;

    ComPtr<ID3D12DescriptorHeap> heap;
    HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
    assert(SUCCEEDED(hr));

    return heap;
}

void DirectXCommon::InitializeDescriptorHeaps()
{
    rtvHeap_ = CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        kRTVDescriptorCount,
        false
    );
    dsvHeap_ = CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        1,
        false
    );

    // ★★★ これが無かった ★★★
    rtvDescriptorSize_ =
        device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV
        );

    dsvDescriptorSize_ =
        device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_DSV
        );
}



void DirectXCommon::InitializeRenderTargetView()
{
    assert(device_);
    assert(swapChain_);
    assert(rtvHeap_);

    for (UINT i = 0; i < kBackBufferCount; ++i) {
        // スワップチェーンからバックバッファのリソースを取得
        HRESULT hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
        assert(SUCCEEDED(hr));

        // SRGB の RTV 設定
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;          // ★ SRGB でビューを作る
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        rtvDesc.Texture2D.PlaneSlice = 0;

        // 対応するRTVハンドルを計算
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
            GetCPUDescriptorHandle(rtvHeap_, rtvDescriptorSize_, i);

        // RTVを生成
        device_->CreateRenderTargetView(
            swapChainResources_[i].Get(),
            &rtvDesc,                 // ★ nullptr ではなく &rtvDesc
            rtvHandle);
    }
}



D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(
    const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
    uint32_t descriptorSize, uint32_t index)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(descriptorSize) * index;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(
    const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
    uint32_t descriptorSize, uint32_t index)
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle =
        descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(descriptorSize) * index;
    return handle;
}

void DirectXCommon::InitializeDepthStencilView()
{
    assert(device_);
    assert(depthStencilResource_);
    assert(dsvHeap_);

    // DSV の設定
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    // DSV を DSVヒープの先頭に 1 個だけ作成
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
        dsvHeap_->GetCPUDescriptorHandleForHeapStart();

    device_->CreateDepthStencilView(
        depthStencilResource_.Get(),
        &dsvDesc,
        dsvHandle);
}

void DirectXCommon::InitializeFence()
{
    assert(device_);

    // フェンス生成
    HRESULT hr = device_->CreateFence(
        fenceValue_,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence_));
    assert(SUCCEEDED(hr));

    // イベント生成（待機に使う）
    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(fenceEvent_ != nullptr);
}

void DirectXCommon::InitializeViewport()
{
    viewport_.Width = static_cast<float>(WinApp::kClientWidth);
    viewport_.Height = static_cast<float>(WinApp::kClientHeight);
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
}

void DirectXCommon::InitializeScissorRect()
{
    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = WinApp::kClientWidth;
    scissorRect_.bottom = WinApp::kClientHeight;
}

void DirectXCommon::InitializeDXC()
{
    HRESULT hr{};

    // DXCユーティリティの生成
    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    assert(SUCCEEDED(hr));

    // DXCコンパイラの生成
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    assert(SUCCEEDED(hr));

    // デフォルトインクルードハンドラの生成
    hr = dxcUtils_->CreateDefaultIncludeHandler(&dxcIncludeHandler_);
    assert(SUCCEEDED(hr));
}


Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(
    const std::wstring& filePath,
    const wchar_t* profile)
{
    HRESULT hr = S_OK;

    // ============================
    // 1) ファイルパス → 絶対パス
    // ============================
    std::filesystem::path absPath = std::filesystem::absolute(filePath);

    if (!std::filesystem::exists(absPath)) {
        std::string msg =
            "❌ Shader file not found:\n" +
            absPath.string() +
            "\nCurrentDir: " +
            std::filesystem::current_path().string() +
            "\n";
        OutputDebugStringA(msg.c_str());
        assert(false);
        return nullptr;
    }

    const std::wstring cacheKey =
        absPath.lexically_normal().wstring() + L"|" + profile;
    const auto cachedShader = shaderCache_.find(cacheKey);
    if (cachedShader != shaderCache_.end()) {
        return cachedShader->second;
    }

    // ============================
    // 2) ファイル読み込み
    // ============================
    Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
    hr = dxcUtils_->LoadFile(absPath.c_str(), nullptr, &shaderSource);
    if (FAILED(hr)) {
        OutputDebugStringA("❌ LoadFile failed\n");
        assert(false);
        return nullptr;
    }

    DxcBuffer shaderBuffer{};
    shaderBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderBuffer.Size = shaderSource->GetBufferSize();
    shaderBuffer.Encoding = DXC_CP_UTF8;

    // ============================
    // 3) コンパイル引数の準備
    // ============================
    LPCWSTR args[] = {
        absPath.c_str(),      // ファイル名
        L"-E", L"main",       // エントリーポイント
        L"-T", profile,       // ターゲット
        L"-Zi",               // デバッグ情報
        L"-Qembed_debug"      // デバッグ情報埋め込み
    };

    Microsoft::WRL::ComPtr<IDxcResult> result = nullptr;

    hr = dxcCompiler_->Compile(
        &shaderBuffer,
        args,
        _countof(args),
        dxcIncludeHandler_.Get(),   // ← ここが超重要（修正済）
        IID_PPV_ARGS(&result)
    );

    if (FAILED(hr)) {
        OutputDebugStringA("❌ Shader compile request failed.\n");
        assert(false);
        return nullptr;
    }

    // ============================
    // 4) エラーメッセージ取り出し
    // ============================
    Microsoft::WRL::ComPtr<IDxcBlobUtf8> errorBlob = nullptr;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorBlob), nullptr);

    if (errorBlob && errorBlob->GetStringLength() != 0) {
        OutputDebugStringA("❌ Shader Compile Error:\n");
        OutputDebugStringA(errorBlob->GetStringPointer());
        assert(false);
        return nullptr;
    }

    // ============================
    // 5) 成功したバイナリの取得
    // ============================
    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;
    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);

    if (!shaderBlob) {
        OutputDebugStringA("❌ DXC returned no shader blob.\n");
        assert(false);
        return nullptr;
    }

    shaderCache_.emplace(cacheKey, shaderBlob);
    return shaderBlob;
}

void DirectXCommon::InitializeDiagnosticLog()
{
    const DWORD requiredLength = GetEnvironmentVariableW(
        L"CG2_D3D12_DIAGNOSTIC_LOG", nullptr, 0);
    if (requiredLength == 0) {
#ifdef _DEBUG
        std::array<wchar_t, 32768> executablePathBuffer{};
        const DWORD executablePathLength = GetModuleFileNameW(
            nullptr,
            executablePathBuffer.data(),
            static_cast<DWORD>(executablePathBuffer.size()));
        if (executablePathLength == 0 ||
            executablePathLength >= executablePathBuffer.size()) {
            return;
        }

        diagnosticLogPath_ = (
            std::filesystem::path(executablePathBuffer.data()).parent_path() /
            L"d3d12-diagnostics.log").wstring();
#else
        return;
#endif
    } else {
        std::vector<wchar_t> pathBuffer(requiredLength);
        const DWORD copiedLength = GetEnvironmentVariableW(
            L"CG2_D3D12_DIAGNOSTIC_LOG",
            pathBuffer.data(),
            static_cast<DWORD>(pathBuffer.size()));
        if (copiedLength == 0 || copiedLength >= pathBuffer.size()) {
            return;
        }

        diagnosticLogPath_ = pathBuffer.data();
    }
    const std::filesystem::path logPath(diagnosticLogPath_);
    if (!logPath.parent_path().empty()) {
        std::error_code error;
        std::filesystem::create_directories(logPath.parent_path(), error);
    }

    std::ofstream clearLog(logPath, std::ios::out | std::ios::trunc);
    clearLog.close();

#ifdef _DEBUG
    AppendDiagnosticLog("D3D12_DIAGNOSTICS_START build=Debug");
#else
    AppendDiagnosticLog("D3D12_DIAGNOSTICS_START build=Release");
#endif
}

void DirectXCommon::ConfigureDred()
{
#ifdef _DEBUG
    ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dredSettings;
    const HRESULT settingsResult = D3D12GetDebugInterface(
        IID_PPV_ARGS(&dredSettings));
    if (FAILED(settingsResult)) {
        AppendDiagnosticLog(std::format(
            "DRED_SETTINGS_UNAVAILABLE hr=0x{:08X}",
            static_cast<uint32_t>(settingsResult)));
        return;
    }

    dredSettings->SetAutoBreadcrumbsEnablement(
        D3D12_DRED_ENABLEMENT_FORCED_ON);
    dredSettings->SetPageFaultEnablement(
        D3D12_DRED_ENABLEMENT_FORCED_ON);
    dredSettings->SetWatsonDumpEnablement(
        D3D12_DRED_ENABLEMENT_SYSTEM_CONTROLLED);

    ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dredSettings1;
    const bool breadcrumbContextsEnabled =
        SUCCEEDED(dredSettings.As(&dredSettings1));
    if (breadcrumbContextsEnabled) {
        dredSettings1->SetBreadcrumbContextEnablement(
            D3D12_DRED_ENABLEMENT_FORCED_ON);
    }

    AppendDiagnosticLog(std::format(
        "DRED_SETTINGS_ENABLED breadcrumbs=1 page_faults=1 contexts={}",
        breadcrumbContextsEnabled ? 1 : 0));
#endif
}

void DirectXCommon::AppendDiagnosticLog(std::string_view message) const
{
    std::string output(message);
    if (output.empty() || output.back() != '\n') {
        output.push_back('\n');
    }
    Log(output);

    if (diagnosticLogPath_.empty()) {
        return;
    }

    std::ofstream log(
        std::filesystem::path(diagnosticLogPath_),
        std::ios::out | std::ios::app);
    if (log) {
        log << output;
    }
}

void DirectXCommon::FlushDebugMessages()
{
#ifdef _DEBUG
    if (!infoQueue_) {
        return;
    }

    const UINT64 messageCount =
        infoQueue_->GetNumStoredMessagesAllowedByRetrievalFilter();
    for (UINT64 index = 0; index < messageCount; ++index) {
        SIZE_T messageLength = 0;
        if (FAILED(infoQueue_->GetMessage(index, nullptr, &messageLength)) ||
            messageLength == 0) {
            continue;
        }

        std::vector<std::byte> messageStorage(messageLength);
        auto* message = reinterpret_cast<D3D12_MESSAGE*>(messageStorage.data());
        if (FAILED(infoQueue_->GetMessage(index, message, &messageLength))) {
            continue;
        }

        AppendDiagnosticLog(std::format(
            "D3D12_MESSAGE severity={} id={} description={}",
            GetMessageSeverityName(message->Severity),
            static_cast<unsigned int>(message->ID),
            message->pDescription ? message->pDescription : "<none>"));
    }

    if (messageCount > 0) {
        infoQueue_->ClearStoredMessages();
    }
#endif
}

void DirectXCommon::CheckDeviceOperation(
    HRESULT result,
    std::string_view operation)
{
    if (SUCCEEDED(result)) {
        return;
    }

    FlushDebugMessages();
    ReportDeviceRemovedDiagnostics(operation, result);
    throw std::runtime_error(std::format(
        "DirectX 12 operation failed: {} (0x{:08X})",
        operation,
        static_cast<uint32_t>(result)));
}

void DirectXCommon::ReportDeviceRemovedDiagnostics(
    std::string_view operation,
    HRESULT failureResult)
{
    const HRESULT removedReason = device_
        ? device_->GetDeviceRemovedReason()
        : E_POINTER;
    AppendDiagnosticLog(std::format(
        "D3D12_OPERATION_FAILED operation={} failure_hr=0x{:08X} "
        "device_reason_hr=0x{:08X}",
        operation,
        static_cast<uint32_t>(failureResult),
        static_cast<uint32_t>(removedReason)));

    if (!device_ || !FAILED(removedReason)) {
        AppendDiagnosticLog("DEVICE_REMOVED detected=0");
        return;
    }

    if (deviceRemovedDiagnosticsReported_) {
        return;
    }
    deviceRemovedDiagnosticsReported_ = true;
    AppendDiagnosticLog("DEVICE_REMOVED detected=1");

    ComPtr<ID3D12DeviceRemovedExtendedData1> dred;
    const HRESULT dredResult = device_.As(&dred);
    if (FAILED(dredResult)) {
        AppendDiagnosticLog(std::format(
            "DRED_OUTPUT_UNAVAILABLE hr=0x{:08X}",
            static_cast<uint32_t>(dredResult)));
        return;
    }

    D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT1 breadcrumbs{};
    if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput1(&breadcrumbs))) {
        const D3D12_AUTO_BREADCRUMB_NODE1* node =
            breadcrumbs.pHeadAutoBreadcrumbNode;
        for (uint32_t nodeIndex = 0; node && nodeIndex < 64;
             ++nodeIndex, node = node->pNext) {
            const uint32_t completed = (std::min)(
                node->pLastBreadcrumbValue
                    ? *node->pLastBreadcrumbValue
                    : 0,
                node->BreadcrumbCount);
            AppendDiagnosticLog(std::format(
                "DRED_BREADCRUMB_NODE index={} queue={} list={} completed={}/{}",
                nodeIndex,
                GetDredObjectName(
                    node->pCommandQueueDebugNameA,
                    node->pCommandQueueDebugNameW),
                GetDredObjectName(
                    node->pCommandListDebugNameA,
                    node->pCommandListDebugNameW),
                completed,
                node->BreadcrumbCount));

            if (!node->pCommandHistory || node->BreadcrumbCount == 0) {
                continue;
            }

            const uint32_t historyBegin = completed > 4 ? completed - 4 : 0;
            const uint32_t historyEnd = (std::min)(
                node->BreadcrumbCount,
                completed + 5);
            for (uint32_t historyIndex = historyBegin;
                 historyIndex < historyEnd;
                 ++historyIndex) {
                const auto operationValue = node->pCommandHistory[historyIndex];
                AppendDiagnosticLog(std::format(
                    "DRED_BREADCRUMB_OP index={} state={} op={}({})",
                    historyIndex,
                    historyIndex < completed ? "completed" : "pending",
                    GetBreadcrumbOperationName(operationValue),
                    static_cast<unsigned int>(operationValue)));
            }

            for (uint32_t contextIndex = 0;
                 contextIndex < node->BreadcrumbContextsCount;
                 ++contextIndex) {
                const auto& context = node->pBreadcrumbContexts[contextIndex];
                if (context.BreadcrumbIndex < historyBegin ||
                    context.BreadcrumbIndex >= historyEnd) {
                    continue;
                }
                AppendDiagnosticLog(std::format(
                    "DRED_BREADCRUMB_CONTEXT index={} text={}",
                    context.BreadcrumbIndex,
                    GetDredObjectName(nullptr, context.pContextString)));
            }
        }
    }

    D3D12_DRED_PAGE_FAULT_OUTPUT1 pageFault{};
    if (SUCCEEDED(dred->GetPageFaultAllocationOutput1(&pageFault))) {
        AppendDiagnosticLog(std::format(
            "DRED_PAGE_FAULT virtual_address=0x{:016X}",
            static_cast<uint64_t>(pageFault.PageFaultVA)));

        const auto logAllocations = [this](
            std::string_view category,
            const D3D12_DRED_ALLOCATION_NODE1* allocation) {
            for (uint32_t index = 0; allocation && index < 64;
                 ++index, allocation = allocation->pNext) {
                AppendDiagnosticLog(std::format(
                    "DRED_ALLOCATION category={} index={} type={} name={}",
                    category,
                    index,
                    static_cast<unsigned int>(allocation->AllocationType),
                    GetDredObjectName(
                        allocation->ObjectNameA,
                        allocation->ObjectNameW)));
            }
        };
        logAllocations("existing", pageFault.pHeadExistingAllocationNode);
        logAllocations("recently_freed", pageFault.pHeadRecentFreedAllocationNode);
    }
}


Microsoft::WRL::ComPtr<ID3D12Resource>
DirectXCommon::CreateBufferResource(size_t sizeInBytes)
{
    assert(device_);
    ++frameTiming_.bufferCreates;

    // ヒープ設定（UploadHeap）
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    // リソース設定（バッファ用）
    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = sizeInBytes;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // リソース作成
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = device_->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
DirectXCommon::CreateBufferResource(
    size_t sizeInBytes,
    D3D12_HEAP_TYPE heapType,
    D3D12_RESOURCE_STATES initialState,
    D3D12_RESOURCE_FLAGS resourceFlags)
{
    assert(device_);
    ++frameTiming_.bufferCreates;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = heapType;

    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = sizeInBytes;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resDesc.Flags = resourceFlags;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = device_->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
DirectXCommon::CreateTextureResource(const DirectX::TexMetadata& metadata)
{
    assert(device_);

    // metadata を基に Resource の設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = UINT(metadata.width);
    resourceDesc.Height = UINT(metadata.height);
    resourceDesc.MipLevels = UINT(metadata.mipLevels);
    resourceDesc.DepthOrArraySize = UINT(metadata.arraySize);
    resourceDesc.Format = metadata.format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

    // 利用するヒープの設定
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;


    // Resource の生成
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = device_->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource>
DirectXCommon::CreateRenderTextureResource(
    ID3D12Device* device,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    const Math::Vector4& clearColor)
{
    assert(device);

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = format;
    clearValue.Color[0] = clearColor.x;
    clearValue.Color[1] = clearColor.y;
    clearValue.Color[2] = clearColor.z;
    clearValue.Color[3] = clearColor.w;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        &clearValue,
        IID_PPV_ARGS(&resource)
    );
    assert(SUCCEEDED(hr));

    return resource;
}

void DirectXCommon::TransitionResource(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES& currentState,
    D3D12_RESOURCE_STATES nextState)
{
    assert(resource);
    if (currentState == nextState) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = currentState;
    barrier.Transition.StateAfter = nextState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);
    currentState = nextState;
}

void DirectXCommon::DrawFullscreenPass(
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
    uint32_t width,
    uint32_t height,
    ID3D12PipelineState* pipelineState,
    uint32_t sourceSrvIndex,
    uint32_t secondarySrvIndex,
    D3D12_GPU_VIRTUAL_ADDRESS parameterAddress, bool presentation)
{
    assert(pipelineState);
    assert(srvDescriptorHeap_);

    commandList_->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    D3D12_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect{};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = static_cast<LONG>(width);
    scissorRect.bottom = static_cast<LONG>(height);

    if (presentation) {
        viewport = presentationViewport_;
        scissorRect = presentationScissor_;
        const float black[] = { 0, 0, 0, 1 };
        commandList_->ClearRenderTargetView(rtvHandle, black, 0, nullptr);
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap_.Get() };
    commandList_->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList_->RSSetViewports(1, &viewport);
    commandList_->RSSetScissorRects(1, &scissorRect);
    commandList_->SetGraphicsRootSignature(fullscreenRootSignature_.Get());
    commandList_->SetPipelineState(pipelineState);

    const uint32_t descriptorSize =
        device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    commandList_->SetGraphicsRootDescriptorTable(
        0,
        GetGPUDescriptorHandle(
            srvDescriptorHeap_,
            descriptorSize,
            sourceSrvIndex));
    commandList_->SetGraphicsRootDescriptorTable(
        1,
        GetGPUDescriptorHandle(
            srvDescriptorHeap_,
            descriptorSize,
            secondarySrvIndex));
    commandList_->SetGraphicsRootConstantBufferView(2, parameterAddress);
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->DrawInstanced(3, 1, 0, 0);
}

void DirectXCommon::DrawBloomPasses(
    uint32_t sourceSrvIndex,
    uint32_t sourceWidth,
    uint32_t sourceHeight,
    float threshold,
    float scatter)
{
    assert(bloomParameterData_);
    assert(bloomParameterResource_);
    for (const BloomRenderTexture& texture : bloomTextures_) {
        assert(texture.resource);
    }

    BloomRenderTexture& halfA = bloomTextures_[0];
    BloomRenderTexture& halfB = bloomTextures_[1];
    BloomRenderTexture& quarterA = bloomTextures_[2];
    BloomRenderTexture& quarterB = bloomTextures_[3];
    const D3D12_GPU_VIRTUAL_ADDRESS bloomParameterAddress =
        bloomParameterResource_->GetGPUVirtualAddress();
    auto getRtvHandle = [&](UINT rtvIndex) {
        return GetCPUDescriptorHandle(rtvHeap_, rtvDescriptorSize_, rtvIndex);
    };
    auto setupParameter =
        [&](uint32_t sourceWidth,
            uint32_t sourceHeight,
            Math::Vector2 direction,
            float threshold,
            float intensity,
            float scatter) {
            bloomParameterData_->texelSize = {
                1.0f / static_cast<float>((std::max)(sourceWidth, 1u)),
                1.0f / static_cast<float>((std::max)(sourceHeight, 1u))
            };
            bloomParameterData_->direction = direction;
            bloomParameterData_->threshold = threshold;
            bloomParameterData_->intensity = intensity;
            bloomParameterData_->scatter = scatter;
            bloomParameterData_->padding = 0.0f;
        };

    TransitionResource(halfA.resource.Get(), halfA.state, D3D12_RESOURCE_STATE_RENDER_TARGET);
    setupParameter(sourceWidth, sourceHeight, { 0.0f, 0.0f }, threshold, 1.0f, 0.0f);
    DrawFullscreenPass(getRtvHandle(halfA.rtvIndex), halfA.width, halfA.height, bloomExtractPipelineState_.Get(), sourceSrvIndex, sourceSrvIndex, bloomParameterAddress);
    TransitionResource(halfA.resource.Get(), halfA.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    TransitionResource(halfB.resource.Get(), halfB.state, D3D12_RESOURCE_STATE_RENDER_TARGET);
    setupParameter(halfA.width, halfA.height, { 1.0f, 0.0f }, 0.0f, 1.0f, 0.0f);
    DrawFullscreenPass(getRtvHandle(halfB.rtvIndex), halfB.width, halfB.height, bloomBlurPipelineState_.Get(), halfA.srvIndex, halfA.srvIndex, bloomParameterAddress);
    TransitionResource(halfB.resource.Get(), halfB.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    TransitionResource(halfA.resource.Get(), halfA.state, D3D12_RESOURCE_STATE_RENDER_TARGET);
    setupParameter(halfB.width, halfB.height, { 0.0f, 1.0f }, 0.0f, 1.0f, 0.0f);
    DrawFullscreenPass(getRtvHandle(halfA.rtvIndex), halfA.width, halfA.height, bloomBlurPipelineState_.Get(), halfB.srvIndex, halfB.srvIndex, bloomParameterAddress);
    TransitionResource(halfA.resource.Get(), halfA.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    TransitionResource(quarterA.resource.Get(), quarterA.state, D3D12_RESOURCE_STATE_RENDER_TARGET);
    setupParameter(halfA.width, halfA.height, { 0.0f, 0.0f }, 0.0f, 1.0f, 0.0f);
    DrawFullscreenPass(getRtvHandle(quarterA.rtvIndex), quarterA.width, quarterA.height, bloomDownsamplePipelineState_.Get(), halfA.srvIndex, halfA.srvIndex, bloomParameterAddress);
    TransitionResource(quarterA.resource.Get(), quarterA.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    TransitionResource(quarterB.resource.Get(), quarterB.state, D3D12_RESOURCE_STATE_RENDER_TARGET);
    setupParameter(quarterA.width, quarterA.height, { 1.0f, 0.0f }, 0.0f, 1.0f, 0.0f);
    DrawFullscreenPass(getRtvHandle(quarterB.rtvIndex), quarterB.width, quarterB.height, bloomBlurPipelineState_.Get(), quarterA.srvIndex, quarterA.srvIndex, bloomParameterAddress);
    TransitionResource(quarterB.resource.Get(), quarterB.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    TransitionResource(quarterA.resource.Get(), quarterA.state, D3D12_RESOURCE_STATE_RENDER_TARGET);
    setupParameter(quarterB.width, quarterB.height, { 0.0f, 1.0f }, 0.0f, 1.0f, 0.0f);
    DrawFullscreenPass(getRtvHandle(quarterA.rtvIndex), quarterA.width, quarterA.height, bloomBlurPipelineState_.Get(), quarterB.srvIndex, quarterB.srvIndex, bloomParameterAddress);
    TransitionResource(quarterA.resource.Get(), quarterA.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    TransitionResource(halfB.resource.Get(), halfB.state, D3D12_RESOURCE_STATE_RENDER_TARGET);
    setupParameter(halfA.width, halfA.height, { 0.0f, 0.0f }, 0.0f, 1.0f, scatter);
    DrawFullscreenPass(getRtvHandle(halfB.rtvIndex), halfB.width, halfB.height, bloomUpsamplePipelineState_.Get(), halfA.srvIndex, quarterA.srvIndex, bloomParameterAddress);
    TransitionResource(halfB.resource.Get(), halfB.state, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void DirectXCommon::DrawBloomCompositeToBackBuffer(
    uint32_t sourceSrvIndex,
    float intensity)
{
    assert(bloomParameterData_);
    assert(bloomParameterResource_);

    BloomRenderTexture& halfB = bloomTextures_[1];
    assert(halfB.resource);
    TransitionResource(
        halfB.resource.Get(),
        halfB.state,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    bloomParameterData_->texelSize = {
        1.0f / static_cast<float>((std::max)(halfB.width, 1u)),
        1.0f / static_cast<float>((std::max)(halfB.height, 1u))
    };
    bloomParameterData_->direction = { 0.0f, 0.0f };
    bloomParameterData_->threshold = 0.0f;
    bloomParameterData_->intensity = intensity;
    bloomParameterData_->scatter = 0.0f;
    bloomParameterData_->padding = 0.0f;

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetCPUDescriptorHandle(
        rtvHeap_,
        rtvDescriptorSize_,
        currentBackBufferIndex_);
    DrawFullscreenPass(
        rtvHandle,
        WinApp::kClientWidth,
        WinApp::kClientHeight,
        bloomCompositePipelineState_.Get(),
        sourceSrvIndex,
        halfB.srvIndex,
        bloomParameterResource_->GetGPUVirtualAddress(), true);
}

void DirectXCommon::InitializeRenderTexture(SrvManager* srvManager)
{
    assert(srvManager);
    assert(device_);
    assert(rtvHeap_);

    constexpr DXGI_FORMAT kRenderTextureFormat =
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    renderTextureResource_ = CreateRenderTextureResource(
        device_.Get(),
        WinApp::kClientWidth,
        WinApp::kClientHeight,
        kRenderTextureFormat,
        kRenderTextureClearColor
    );
    postEffectTextureResource_ = CreateRenderTextureResource(
        device_.Get(),
        WinApp::kClientWidth,
        WinApp::kClientHeight,
        kRenderTextureFormat,
        kRenderTextureClearColor
    );
    constexpr DXGI_FORMAT kBloomTextureFormat =
        DXGI_FORMAT_R16G16B16A16_FLOAT;
    const uint32_t bloomHalfWidth = (std::max)(WinApp::kClientWidth / 2u, 1u);
    const uint32_t bloomHalfHeight = (std::max)(WinApp::kClientHeight / 2u, 1u);
    const uint32_t bloomQuarterWidth =
        (std::max)(WinApp::kClientWidth / 4u, 1u);
    const uint32_t bloomQuarterHeight =
        (std::max)(WinApp::kClientHeight / 4u, 1u);
    bloomTextures_[0].rtvIndex = kBloomHalfARTVIndex;
    bloomTextures_[0].width = bloomHalfWidth;
    bloomTextures_[0].height = bloomHalfHeight;
    bloomTextures_[1].rtvIndex = kBloomHalfBRTVIndex;
    bloomTextures_[1].width = bloomHalfWidth;
    bloomTextures_[1].height = bloomHalfHeight;
    bloomTextures_[2].rtvIndex = kBloomQuarterARTVIndex;
    bloomTextures_[2].width = bloomQuarterWidth;
    bloomTextures_[2].height = bloomQuarterHeight;
    bloomTextures_[3].rtvIndex = kBloomQuarterBRTVIndex;
    bloomTextures_[3].width = bloomQuarterWidth;
    bloomTextures_[3].height = bloomQuarterHeight;
    for (BloomRenderTexture& texture : bloomTextures_) {
        texture.resource = CreateRenderTextureResource(
            device_.Get(),
            texture.width,
            texture.height,
            kBloomTextureFormat,
            { 0.0f, 0.0f, 0.0f, 1.0f });
        texture.state = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = kRenderTextureFormat;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetCPUDescriptorHandle(
        rtvHeap_,
        rtvDescriptorSize_,
        kRenderTextureRTVIndex
    );
    device_->CreateRenderTargetView(
        renderTextureResource_.Get(),
        &rtvDesc,
        rtvHandle
    );

    D3D12_CPU_DESCRIPTOR_HANDLE postEffectRtvHandle = GetCPUDescriptorHandle(
        rtvHeap_,
        rtvDescriptorSize_,
        kPostEffectTextureRTVIndex
    );
    device_->CreateRenderTargetView(
        postEffectTextureResource_.Get(),
        &rtvDesc,
        postEffectRtvHandle
    );

    D3D12_RENDER_TARGET_VIEW_DESC bloomRtvDesc{};
    bloomRtvDesc.Format = kBloomTextureFormat;
    bloomRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    bloomRtvDesc.Texture2D.MipSlice = 0;
    bloomRtvDesc.Texture2D.PlaneSlice = 0;
    for (const BloomRenderTexture& texture : bloomTextures_) {
        device_->CreateRenderTargetView(
            texture.resource.Get(),
            &bloomRtvDesc,
            GetCPUDescriptorHandle(
                rtvHeap_,
                rtvDescriptorSize_,
                texture.rtvIndex));
    }

    renderTextureSrvIndex_ = srvManager->Allocate();
    srvManager->CreateSRVforTexture2D(
        renderTextureSrvIndex_,
        renderTextureResource_.Get(),
        kRenderTextureFormat,
        1
    );
    postEffectTextureSrvIndex_ = srvManager->Allocate();
    srvManager->CreateSRVforTexture2D(
        postEffectTextureSrvIndex_,
        postEffectTextureResource_.Get(),
        kRenderTextureFormat,
        1
    );
    for (BloomRenderTexture& texture : bloomTextures_) {
        texture.srvIndex = srvManager->Allocate();
        srvManager->CreateSRVforTexture2D(
            texture.srvIndex,
            texture.resource.Get(),
            kBloomTextureFormat,
            1);
    }

    depthTextureSrvIndex_ = srvManager->Allocate();
    D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc{};
    depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthSrvDesc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    depthSrvDesc.Texture2D.MipLevels = 1;
    device_->CreateShaderResourceView(
        depthStencilResource_.Get(),
        &depthSrvDesc,
        srvManager->GetCPUDescriptorHandle(depthTextureSrvIndex_)
    );

    DirectX::ScratchImage dissolveMaskImage =
        LoadTexture("resources/noise0.png");
    const DirectX::TexMetadata& dissolveMaskMetadata =
        dissolveMaskImage.GetMetadata();
    dissolveMaskTextureResource_ = CreateTextureResource(dissolveMaskMetadata);
    UploadTextureData(dissolveMaskTextureResource_, dissolveMaskImage);
    dissolveMaskTextureSrvIndex_ = srvManager->Allocate();
    srvManager->CreateSRVforTexture2D(
        dissolveMaskTextureSrvIndex_,
        dissolveMaskTextureResource_.Get(),
        dissolveMaskMetadata.format,
        static_cast<UINT>(dissolveMaskMetadata.mipLevels)
    );

    srvDescriptorHeap_ = srvManager->GetDescriptorHeapComPtr();

    CreateFullscreenRootSignature();
    copyImagePipelineState_ =
        CreateFullscreenPipelineState(L"shaders/CopyImage.PS.hlsl");
    grayscalePipelineState_ =
        CreateFullscreenPipelineState(L"shaders/Grayscale.PS.hlsl");
    vignettePipelineState_ =
        CreateFullscreenPipelineState(L"shaders/Vignette.PS.hlsl");
    boxFilterPipelineState_ =
        CreateFullscreenPipelineState(L"shaders/BoxFilter.PS.hlsl");
    boxFilter5x5PipelineState_ =
        CreateFullscreenPipelineState(L"shaders/BoxFilter5x5.PS.hlsl");
    gaussianFilterPipelineState_ =
        CreateFullscreenPipelineState(L"shaders/GaussianFilter.PS.hlsl");
    vignetteSmoothingPipelineState_ =
        CreateFullscreenPipelineState(L"shaders/VignetteSmoothing.PS.hlsl");
    luminanceOutlinePipelineState_ =
        CreateFullscreenPipelineState(
            L"shaders/LuminanceBasedOutline.PS.hlsl");
    depthOutlinePipelineState_ =
        CreateFullscreenPipelineState(L"shaders/DepthBasedOutline.PS.hlsl");
    radialBlurPipelineState_ =
        CreateFullscreenPipelineState(L"shaders/RadialBlur.PS.hlsl");
    dissolvePipelineState_ =
        CreateFullscreenPipelineState(L"shaders/Dissolve.PS.hlsl");
    randomPipelineState_ =
        CreateFullscreenPipelineState(L"shaders/Random.PS.hlsl");
    gameTonePipelineState_ =
        CreateFullscreenPipelineState(L"shaders/GameTone.PS.hlsl");
    bloomPipelineState_ =
        CreateFullscreenPipelineState(L"shaders/Bloom.PS.hlsl");
    constexpr DXGI_FORMAT kBloomPipelineFormat =
        DXGI_FORMAT_R16G16B16A16_FLOAT;
    bloomExtractPipelineState_ =
        CreateFullscreenPipelineState(
            L"shaders/BloomExtract.PS.hlsl",
            kBloomPipelineFormat);
    bloomBlurPipelineState_ =
        CreateFullscreenPipelineState(
            L"shaders/BloomBlur.PS.hlsl",
            kBloomPipelineFormat);
    bloomDownsamplePipelineState_ =
        CreateFullscreenPipelineState(
            L"shaders/BloomDownsample.PS.hlsl",
            kBloomPipelineFormat);
    bloomUpsamplePipelineState_ =
        CreateFullscreenPipelineState(
            L"shaders/BloomUpsample.PS.hlsl",
            kBloomPipelineFormat);
    bloomCompositePipelineState_ =
        CreateFullscreenPipelineState(L"shaders/BloomComposite.PS.hlsl");

    radialBlurParameterResource_ =
        CreateBufferResource(sizeof(RadialBlurParameter));
    radialBlurParameterResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&radialBlurParameterData_)
    );
    radialBlurParameterData_->center = { 0.5f, 0.5f };
    radialBlurParameterData_->blurWidth = 0.01f;
    radialBlurParameterData_->padding = 0.0f;

    dissolveParameterResource_ =
        CreateBufferResource(sizeof(DissolveParameter));
    dissolveParameterResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&dissolveParameterData_)
    );
    dissolveParameterData_->threshold = 0.0f;
    dissolveParameterData_->edgeWidth = 0.03f;
    dissolveParameterData_->padding = { 0.0f, 0.0f };

    randomParameterResource_ =
        CreateBufferResource(sizeof(RandomParameter));
    randomParameterResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&randomParameterData_)
    );
    randomParameterData_->time = randomTime_;
    randomParameterData_->padding[0] = 0.0f;
    randomParameterData_->padding[1] = 0.0f;
    randomParameterData_->padding[2] = 0.0f;

    gameToneParameterResource_ =
        CreateBufferResource(sizeof(GameToneParameter));
    gameToneParameterResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&gameToneParameterData_)
    );
    gameToneParameterData_->projectionInverse =
        Math::Transpose(Math::MakeIdentity4x4());
    gameToneParameterData_->vignetteStrength = 0.46f;
    gameToneParameterData_->saturation = 1.07f;
    gameToneParameterData_->contrast = 1.15f;
    gameToneParameterData_->damageTint = 0.0f;
    gameToneParameterData_->fogStart = 72.0f;
    gameToneParameterData_->fogEnd = 330.0f;
    gameToneParameterData_->fogStrength = 0.46f;
    gameToneParameterData_->horizonFogStrength = 0.035f;
    gameToneParameterData_->exposure = 1.00f;
    gameToneParameterData_->blackPoint = 0.010f;
    gameToneParameterData_->highlightCompression = 0.46f;
    gameToneParameterData_->colorTemperature = 0.035f;
    SetFlightBlur();

    depthOutlineParameterResource_ =
        CreateBufferResource(sizeof(DepthOutlineParameter));
    depthOutlineParameterResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&depthOutlineParameterData_)
    );
    depthOutlineParameterData_->projectionInverse =
        Math::Transpose(Math::MakeIdentity4x4());

    bloomParameterResource_ =
        CreateBufferResource(sizeof(BloomParameter));
    bloomParameterResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&bloomParameterData_)
    );
    bloomParameterData_->texelSize = { 1.0f / WinApp::kClientWidth, 1.0f / WinApp::kClientHeight };
    bloomParameterData_->direction = { 0.0f, 0.0f };
    bloomParameterData_->threshold = 0.62f;
    bloomParameterData_->intensity = 0.86f;
    bloomParameterData_->scatter = 0.72f;
    bloomParameterData_->padding = 0.0f;
}

void DirectXCommon::CreateFullscreenRootSignature()
{
    D3D12_DESCRIPTOR_RANGE descriptorRanges[2]{};
    descriptorRanges[0].BaseShaderRegister = 0;
    descriptorRanges[0].NumDescriptors = 1;
    descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[0].OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    descriptorRanges[1].BaseShaderRegister = 1;
    descriptorRanges[1].NumDescriptors = 1;
    descriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRanges[1].OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[3]{};
    for (uint32_t index = 0; index < _countof(descriptorRanges); ++index) {
        rootParameters[index].ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rootParameters[index].DescriptorTable.pDescriptorRanges =
            &descriptorRanges[index];
        rootParameters[index].DescriptorTable.NumDescriptorRanges = 1;
    }
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].Descriptor.ShaderRegister = 0;

    D3D12_STATIC_SAMPLER_DESC staticSamplers[2]{};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    staticSamplers[1] = staticSamplers[0];
    staticSamplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    staticSamplers[1].ShaderRegister = 1;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &descriptionRootSignature,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob
    );
    if (FAILED(hr)) {
        if (errorBlob) {
            Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }

    hr = device_->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&fullscreenRootSignature_)
    );
    assert(SUCCEEDED(hr));
}

Microsoft::WRL::ComPtr<ID3D12PipelineState>
DirectXCommon::CreateFullscreenPipelineState(
    const std::wstring& pixelShaderPath,
    DXGI_FORMAT rtvFormat)
{
    auto vertexShaderBlob =
        CompileShader(L"shaders/Fullscreen.VS.hlsl", L"vs_6_0");
    auto pixelShaderBlob =
        CompileShader(pixelShaderPath, L"ps_6_0");

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = false;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{};
    pipelineDesc.pRootSignature = fullscreenRootSignature_.Get();
    pipelineDesc.InputLayout.pInputElementDescs = nullptr;
    pipelineDesc.InputLayout.NumElements = 0;
    pipelineDesc.VS = {
        vertexShaderBlob->GetBufferPointer(),
        vertexShaderBlob->GetBufferSize()
    };
    pipelineDesc.PS = {
        pixelShaderBlob->GetBufferPointer(),
        pixelShaderBlob->GetBufferSize()
    };
    pipelineDesc.BlendState = blendDesc;
    pipelineDesc.RasterizerState = rasterizerDesc;
    pipelineDesc.DepthStencilState = depthStencilDesc;
    pipelineDesc.NumRenderTargets = 1;
    pipelineDesc.RTVFormats[0] = rtvFormat;
    pipelineDesc.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipelineDesc.SampleDesc.Count = 1;
    pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    HRESULT hr = device_->CreateGraphicsPipelineState(
        &pipelineDesc,
        IID_PPV_ARGS(&pipelineState)
    );
    assert(SUCCEEDED(hr));

    return pipelineState;
}

void DirectXCommon::UploadTextureData(
    const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
    const DirectX::ScratchImage& mipImages)
{
    assert(device_);
    assert(commandQueue_);
    assert(fence_);

    HRESULT hr{};

    // ① 単発転送の場合だけ、専用コマンドを作成
    ComPtr<ID3D12CommandAllocator> uploadAllocator;
    ComPtr<ID3D12GraphicsCommandList> uploadList;
    ID3D12GraphicsCommandList* destinationList = textureUploadBatchList_.Get();
    if (!isTextureUploadBatchActive_) {
        hr = device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&uploadAllocator));
        assert(SUCCEEDED(hr));
        uploadAllocator->SetName(L"CG2 Texture Upload Allocator");

        hr = device_->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            uploadAllocator.Get(),
            nullptr,
            IID_PPV_ARGS(&uploadList));
        assert(SUCCEEDED(hr));
        uploadList->SetName(L"CG2 Texture Upload Command List");
        destinationList = uploadList.Get();
    }
    assert(destinationList);

    // ② サブリソース情報を作成
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    DirectX::PrepareUpload(
        device_.Get(),
        mipImages.GetImages(),
        mipImages.GetImageCount(),
        mipImages.GetMetadata(),
        subresources);

    // 中間アップロードバッファ
    UINT64 intermediateSize =
        GetRequiredIntermediateSize(
            texture.Get(), 0,
            static_cast<UINT>(subresources.size()));

    auto intermediate =
        CreateBufferResource(static_cast<size_t>(intermediateSize));

    // ③ UpdateSubresources で COPY_DEST にコピー
    UpdateSubresources(
        destinationList,
        texture.Get(),
        intermediate.Get(),
        0, 0,
        static_cast<UINT>(subresources.size()),
        subresources.data());

    // コピー完了後、テクスチャを SHADER_RESOURCE 用に遷移
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = texture.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    destinationList->ResourceBarrier(1, &barrier);

    if (isTextureUploadBatchActive_) {
        textureUploadBatchIntermediates_.push_back(intermediate);
        return;
    }

    // ④ コマンドを閉じて実行
    hr = uploadList->Close();
    CheckDeviceOperation(hr, "TextureUpload CommandList::Close");

    ID3D12CommandList* lists[] = { uploadList.Get() };
    commandQueue_->ExecuteCommandLists(_countof(lists), lists);

    // ⑤ フェンスで GPU 完了を待つ
    fenceValue_++;
    hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
    CheckDeviceOperation(hr, "TextureUpload CommandQueue::Signal");

    const UINT64 completedValue = fence_->GetCompletedValue();
    if (completedValue == UINT64_MAX) {
        HRESULT removedReason = device_->GetDeviceRemovedReason();
        if (SUCCEEDED(removedReason)) {
            removedReason = DXGI_ERROR_DEVICE_REMOVED;
        }
        CheckDeviceOperation(
            removedReason,
            "TextureUpload Fence::GetCompletedValue");
    }
    if (completedValue < fenceValue_) {
        hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        CheckDeviceOperation(
            hr,
            "TextureUpload Fence::SetEventOnCompletion");
        const DWORD waitResult = WaitForSingleObject(fenceEvent_, INFINITE);
        if (waitResult != WAIT_OBJECT_0) {
            const DWORD error = waitResult == WAIT_FAILED
                ? GetLastError()
                : ERROR_GEN_FAILURE;
            CheckDeviceOperation(
                HRESULT_FROM_WIN32(error),
                "TextureUpload WaitForSingleObject(fence)");
        }
    }
    FlushDebugMessages();

    // ここまで来れば intermediate / uploadList / uploadAllocator は
    // GPU 側の処理が終わっているので破棄されてOK（スコープアウト）
}

void DirectXCommon::BeginTextureUploadBatch()
{
    assert(device_);
    assert(!isTextureUploadBatchActive_);

    textureUploadBatchIntermediates_.clear();
    HRESULT hr = device_->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&textureUploadBatchAllocator_));
    assert(SUCCEEDED(hr));
    textureUploadBatchAllocator_->SetName(
        L"CG2 Texture Upload Batch Allocator");

    hr = device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        textureUploadBatchAllocator_.Get(),
        nullptr,
        IID_PPV_ARGS(&textureUploadBatchList_));
    assert(SUCCEEDED(hr));
    textureUploadBatchList_->SetName(
        L"CG2 Texture Upload Batch Command List");
    isTextureUploadBatchActive_ = true;
}

void DirectXCommon::EndTextureUploadBatch()
{
    assert(isTextureUploadBatchActive_);
    assert(textureUploadBatchList_);

    HRESULT hr = textureUploadBatchList_->Close();
    CheckDeviceOperation(hr, "TextureUploadBatch CommandList::Close");

    if (!textureUploadBatchIntermediates_.empty()) {
        ID3D12CommandList* lists[] = { textureUploadBatchList_.Get() };
        commandQueue_->ExecuteCommandLists(_countof(lists), lists);

        ++fenceValue_;
        hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
        CheckDeviceOperation(hr, "TextureUploadBatch CommandQueue::Signal");
        const UINT64 completedValue = fence_->GetCompletedValue();
        if (completedValue == UINT64_MAX) {
            HRESULT removedReason = device_->GetDeviceRemovedReason();
            if (SUCCEEDED(removedReason)) {
                removedReason = DXGI_ERROR_DEVICE_REMOVED;
            }
            CheckDeviceOperation(
                removedReason,
                "TextureUploadBatch Fence::GetCompletedValue");
        }
        if (completedValue < fenceValue_) {
            hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
            CheckDeviceOperation(
                hr,
                "TextureUploadBatch Fence::SetEventOnCompletion");
            const DWORD waitResult = WaitForSingleObject(
                fenceEvent_, INFINITE);
            if (waitResult != WAIT_OBJECT_0) {
                const DWORD error = waitResult == WAIT_FAILED
                    ? GetLastError()
                    : ERROR_GEN_FAILURE;
                CheckDeviceOperation(
                    HRESULT_FROM_WIN32(error),
                    "TextureUploadBatch WaitForSingleObject(fence)");
            }
        }
        FlushDebugMessages();
    }

    textureUploadBatchIntermediates_.clear();
    textureUploadBatchList_.Reset();
    textureUploadBatchAllocator_.Reset();
    isTextureUploadBatchActive_ = false;
}

// すでにある UploadTextureData の下あたりに追加
DirectX::ScratchImage DirectXCommon::LoadTexture(
    const std::string& filePath,
    bool forceSrgb)
{
    // std::string → std::wstring に変換
    std::wstring filePathW(filePath.begin(), filePath.end());

    DirectX::ScratchImage image{};
    DirectX::TexMetadata metadata{};

    HRESULT hr = S_OK;
    const std::wstring extension =
        std::filesystem::path(filePathW).extension().wstring();

    if (extension == L".dds" || extension == L".DDS") {
        hr = DirectX::LoadFromDDSFile(
            filePathW.c_str(),
            DirectX::DDS_FLAGS_NONE,
            &metadata,
            image);
        assert(SUCCEEDED(hr));
        return image;
    }

    // PNG / JPG など WIC 対応画像を読み込む
    hr = DirectX::LoadFromWICFile(
        filePathW.c_str(),
        forceSrgb ? DirectX::WIC_FLAGS_FORCE_SRGB : DirectX::WIC_FLAGS_NONE,
        &metadata,
        image);
    assert(SUCCEEDED(hr));

    // ミップマップ生成
    DirectX::ScratchImage mipImages{};
    hr = DirectX::GenerateMipMaps(
        image.GetImages(),
        image.GetImageCount(),
        metadata,
        forceSrgb ? DirectX::TEX_FILTER_SRGB : DirectX::TEX_FILTER_DEFAULT,
        0,
        mipImages);
    assert(SUCCEEDED(hr));

    return mipImages;  // これをそのまま返して使う
}

void DirectXCommon::InitializeFixFPS() {
    // 現在時間を記録
    reference_ = std::chrono::steady_clock::now();
}

void DirectXCommon::UpdateFixFPS() {
    frameTiming_.fpsWaitMs = 0.0f;

    const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));
    constexpr bool kUseSoftwareFrameLimiter = false;

    auto now = std::chrono::steady_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

    // ★ ここが追加（秒に変換）
    deltaTime_ = elapsed.count() / 1000000.0f;

    if (kUseSoftwareFrameLimiter && elapsed < kMinTime) {
        const auto waitBegin = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - reference_ < kMinTime) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
        const auto waitEnd = std::chrono::steady_clock::now();
        frameTiming_.fpsWaitMs = ToMilliseconds(waitBegin, waitEnd);

        // ★ FPS固定時は deltaTime を 1/60 に揃える
        deltaTime_ = 1.0f / 60.0f;
    }

    reference_ = std::chrono::steady_clock::now();
}
