// テクスチャやバッファをシェーダーから参照するための記述子を割り当てる。

#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>
#include <vector>

class DirectXCommon;

class SrvManager {
public:
    // 最大SRV数
    static const uint32_t kMaxSRVCount;

    // 初期化（DirectXCommonは借り物）
    void Initialize(DirectXCommon* dxCommon);

    // ===== 確保 =====
    uint32_t Allocate();
    bool CanAllocate(uint32_t count) const;
    void Free(uint32_t index);
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE handle);

    // ===== ハンドル取得 =====
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

    // ===== SRV生成 =====
    void CreateSRVforTexture2D(
        uint32_t srvIndex,
        ID3D12Resource* resource,
        DXGI_FORMAT format,
        UINT mipLevels
    );

    void CreateSRVforTextureCube(
        uint32_t srvIndex,
        ID3D12Resource* resource,
        DXGI_FORMAT format,
        UINT mipLevels
    );

    void CreateSRVforStructuredBuffer(
        uint32_t srvIndex,
        ID3D12Resource* resource,
        UINT numElements,
        UINT structureByteStride
    );

    void CreateUAVforStructuredBuffer(
        uint32_t srvIndex,
        ID3D12Resource* resource,
        UINT numElements,
        UINT structureByteStride
    );

    // ===== 描画前処理 =====
    void PreDraw();

    // ===== 描画時SRVセット =====
    void SetGraphicsRootDescriptorTable(
        UINT rootParameterIndex,
        uint32_t srvIndex
    );

    void SetComputeRootDescriptorTable(
        UINT rootParameterIndex,
        uint32_t srvIndex
    );

    // ===== DescriptorHeap取得 =====
    ID3D12DescriptorHeap* GetDescriptorHeap() const {
        return descriptorHeap_.Get();
    }

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDescriptorHeapComPtr() const { return descriptorHeap_; }


private:
    DirectXCommon* directXCommon_ = nullptr; // 借り物

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
    uint32_t descriptorSize_ = 0;

    uint32_t useIndex_ = 0;
    std::vector<uint32_t> freeIndices_;
};
