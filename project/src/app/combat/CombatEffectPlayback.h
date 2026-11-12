#pragma once

#include "engine/3d/Object3d.h"

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

class Camera;
class Model;
class Object3dCommon;

// 起動時に用意した描画物体を貸し出し、表示が終わった枠から回収する
class CombatEffectPlayback {
public:
    static constexpr size_t kVisualCapacity = 320;
    static constexpr size_t kEffectCapacity = 320;

    enum class EffectType { EnemyImpact, EnemyDestroy, RewardCollect, PlayerDamage };
    struct Effect {
        Math::Vector3 worldPosition{};
        float age = 0.0f; // age・durationは60fps換算のフレーム数
        int duration = 24;
        float strength = 1.0f;
        EffectType type = EffectType::EnemyDestroy;
    private:
        friend class CombatEffectPlayback;
        struct Visual {
            std::unique_ptr<Object3d> object;
            Math::Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
            float baseSize = 1.0f;
            float growth = 1.0f;
            float spin = 0.0f;
            float popDelay = 0.0f;
            float lifeEnd = 1.0f; // 閃光・衝撃波・煙を別々の時間で収束させる
            float roll = 0.0f;
            float drag = 0.0f;
            float aspectX = 1.0f;
            float aspectY = 1.0f;
            Math::Vector3 velocity{};
            bool additive = true;
        };
        static constexpr size_t kMaxVisuals = 24;
        std::array<Visual, kMaxVisuals> visuals{};
        size_t visualCount = 0;
    };

    // baseSizeはワールド単位、spin/rollはラジアン　popDelay/lifeEndは全寿命に対する0〜1の割合
    // velocityとdragは60fps換算の1フレームを基準にする
    struct VisualSpec {
        Math::Vector4 color{ 1, 1, 1, 1 };
        float baseSize = 1.0f;
        float growth = 1.0f;
        float spin = 0.0f;
        float popDelay = 0.0f;
        float aspectX = 1.0f;
        float aspectY = 1.0f;
        Math::Vector3 velocity{};
        bool additive = true;
        float lifeEnd = 1.0f;
        float roll = 0.0f;
        float drag = 0.0f;
    };

    void Initialize(Object3dCommon* common, Camera* camera);
    void PrepareOne();
    void Clear();
    void Reset();
    void Update(float deltaSeconds);
    void Draw();

    // 枠不足時は演出だけを省く　作成途中で借りた物体も必ず回収する
    void AddVisual(Effect& effect, Model* model, const Math::Vector3& worldPosition, const VisualSpec& spec);
    void Commit(Effect&& effect);

    size_t ActiveCount() const { return effects_.size(); }
    size_t ActiveVisualCount() const;
    size_t AvailableCount() const { return available_.size(); }
    size_t MissCount() const { return missCount_; }
#ifdef _DEBUG
    float LatestStrength() const { return effects_.empty() ? 0.0f : effects_.back().strength; }
    size_t LatestVisualCount() const { return effects_.empty() ? 0 : effects_.back().visualCount; }
#endif

private:
    std::unique_ptr<Object3d> AcquireVisual();
    void RecycleVisuals(Effect& effect);

    Object3dCommon* common_ = nullptr;
    Camera* camera_ = nullptr;
    std::vector<Effect> effects_;
    std::vector<std::unique_ptr<Object3d>> available_;
    size_t missCount_ = 0;
};
