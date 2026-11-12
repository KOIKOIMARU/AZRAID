// GPU上のパーティクルの発生・更新・描画を、グループ単位で管理する

#pragma once

#include <unordered_map>
#include <list>
#include <string>
#include <memory>
#include <random>
#include <cstdint>



#include <wrl.h>
#include <d3d12.h>

#include "engine/base/Math.h"
#include "engine/3d/Particle.h"

class DirectXCommon;
class SrvManager;

/// <summary>
/// パーティクル管理クラス（Singleton）
/// </summary>
class ParticleManager
{
public:
    struct EmitSettings
    {
        float radius = 1.0f;
        Math::Vector3 direction{ 0.0f, 1.0f, 0.0f };
        float spread = 1.0f;
        Math::Vector4 colorMin{ 1.0f, 1.0f, 1.0f, 1.0f };
        Math::Vector4 colorMax{ 1.0f, 1.0f, 1.0f, 1.0f };
        Math::Vector2 scaleMin{ 0.12f, 0.12f };
        Math::Vector2 scaleMax{ 0.30f, 0.30f };
        float lifeTimeMin = 1.0f;
        float lifeTimeMax = 2.0f;
        float speedMin = 0.4f;
        float speedMax = 1.6f;
        float endScale = 1.0f;
    };

    // ===== シングルトン =====
    static ParticleManager* GetInstance();

    // コピー禁止
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

    // ===== 初期化 / 終了 =====
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Finalize();

    // ===== 更新 / 描画 =====
    void Update(const Math::Matrix4x4& viewMatrix,
        const Math::Matrix4x4& projectionMatrix, bool advanceSimulation = true);

    // 停止中は既存粒子だけを描き、GPU上の射出・移動・寿命更新を実行しない
    void Draw(bool advanceSimulation = true);

    // ===== パーティクル発生 =====
    bool CreateParticleGroup(const std::string& name,
        const std::string& textureFilePath,
        const EmitSettings& settings = {});
    bool RemoveParticleGroup(const std::string& name);

    void Emit(const std::string& name,
        const Math::Vector3& position,
        uint32_t count);
    void Emit(const std::string& name,
        const Math::Vector3& position,
        const Math::Vector3& direction,
        uint32_t count);

    float RandomFloat(float min, float max);

private:
    ParticleManager() = default;
    ~ParticleManager() = default;

    // ============================
    // インスタンシング用構造体
    // ============================
    struct ParticleCS
    {
        Math::Vector3 translate;
        Math::Vector3 scale;
        float lifeTime;
        Math::Vector3 velocity;
        float currentTime;
        Math::Vector4 color;
        Math::Vector3 initialScale;
        float endScale;
    };

    struct PerView
    {
        Math::Matrix4x4 viewProjection;
        Math::Matrix4x4 billboardMatrix;
    };

    struct EmitterSphere
    {
        Math::Vector3 translate;
        float radius;
        Math::Vector3 direction;
        float spread;
        Math::Vector4 colorMin;
        Math::Vector4 colorMax;
        Math::Vector2 scaleMin;
        Math::Vector2 scaleMax;
        float lifeTimeMin;
        float lifeTimeMax;
        float speedMin;
        float speedMax;
        float endScale;
        uint32_t count;
        uint32_t emit;
        float padding;
    };

    struct PerFrame
    {
        float time;
        float deltaTime;
        Math::Vector2 padding;
    };

    // ============================
    // パーティクルグループ
    // ============================
    struct ParticleGroup
    {
        // --- マテリアル情報 ---
        std::string textureFilePath;
        uint32_t textureSrvIndex = 0;

        // --- パーティクル本体 ---
        std::list<Particle> particles;

        // --- インスタンシング ---
        uint32_t particleSrvIndex = UINT32_MAX;
        uint32_t particleUavIndex = UINT32_MAX;
        uint32_t freeCounterUavIndex = UINT32_MAX;
        uint32_t freeListUavIndex = UINT32_MAX;
        uint32_t instanceCount = 0;
        bool needsInitialize = true;
        bool needsEmit = false;

        Microsoft::WRL::ComPtr<ID3D12Resource> particleResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> freeCounterResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource;
        Microsoft::WRL::ComPtr<ID3D12Resource> emitterResource;
        EmitterSphere* emitterData = nullptr;
        D3D12_RESOURCE_STATES particleResourceState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES freeCounterResourceState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES freeListResourceState = D3D12_RESOURCE_STATE_COMMON;
    };

private:
    // ===== 借り物 =====
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    // ===== グループ管理 =====
    std::unordered_map<std::string, ParticleGroup> particleGroups_;

    std::mt19937 randomEngine_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> initializeRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> initializePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> emitRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> emitPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> updateRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> updatePipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
    PerView* perViewData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource_;
    PerFrame* perFrameData_ = nullptr;
    float totalTime_ = 0.0f;

    static constexpr uint32_t kMaxInstanceCount_ = 1024;

    void CreateInitializePipeline();
    void CreateEmitPipeline();
    void CreateUpdatePipeline();
    void DispatchInitialize(ParticleGroup& group);
    void DispatchEmit(ParticleGroup& group);
    void DispatchUpdate(ParticleGroup& group);
    bool IsGpuPipelineReady() const;
    void ReleaseParticleGroupDescriptors(ParticleGroup& group);

};
