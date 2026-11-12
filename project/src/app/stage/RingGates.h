#pragma once

#include "engine/3d/Object3d.h"

#include <array>
#include <cstddef>
#include <memory>

class Model;
class Object3dCommon;

// コース上の加速ゲートの寿命・通過判定・表示と、通過後の短い加速を管理する
// 通過数を返すだけにし、スコアやフィーバーのルールは参照しない
class RingGates {
public:
    static constexpr size_t kCapacity = 8;
    static constexpr float kRadiusX = 2.15f;
    static constexpr float kRadiusY = 1.75f;
    static constexpr float kBoostFrames = 66.0f;

    struct Gate {
        Math::Vector3 position{}; // ワールド座標　生成後はレールや自機へ追従させない
        float age = 0.0f;        // ageとlifetimeは60fps換算のフレーム数
        float lifetime = 0.0f;
        bool active = false;
        float releaseAge = -1.0f; // 通過後だけフレームを前方・外側へ開く
    };

    // 描画物体はここで一度だけ確保する　commonとモデルの所有権は受け取らない
    void Initialize(Object3dCommon* common, Model* frame, Model* light, Model* rear, Model* flow);
    void Clear();      // 描画物体を破棄し、借りた参照も解除する
    void Reset();      // 再挑戦用　確保済みの描画物体を保ち、リングと集計を初期化する
    void Deactivate(); // 終了画面用　集計は保ち、表示中のリングだけ消す
    bool Spawn(const Math::Vector3& position, float railDistance, float railSpeed);

    // 前後の自機位置から通過地点のXYを補間する　戻り値はこの更新で通過した個数
    // 停止中は呼び出さない　判定済み・期限切れのリングは、その場で再利用枠へ戻る
    int Update(const Math::Vector3& previous, const Math::Vector3& current, float frameStep);
    void Draw(const Math::Vector3& playerPosition, bool fever);
    void StartBoost(); // 複数通過しても速度を積み上げず、同じ長さへ更新する
    float BoostStrength() const;
    float BoostAge() const { return boostAge_; }

    int SpawnCount() const { return spawnCount_; }
    int PassCount() const { return passCount_; }
    // 表示や検証は読み取りだけ　生成・通過・破棄は上記の操作に集約する
    const std::array<Gate, kCapacity>& Gates() const { return gates_; }

private:
    struct Visual {
        std::array<std::unique_ptr<Object3d>, 4> frames;
        std::array<std::unique_ptr<Object3d>, 4> lights;
        std::unique_ptr<Object3d> rear;
        std::unique_ptr<Object3d> flow;
    };
    Object3dCommon* common_ = nullptr;
    std::array<Gate, kCapacity> gates_{};
    std::array<Visual, kCapacity> visuals_{};
    int spawnCount_ = 0;
    int passCount_ = 0;
    float boostAge_ = -1.0f;
};
