// 一定間隔でパーティクルの発生を依頼する。発生する場所と個数を持つ。

#pragma once

#include <string>
#include "engine/base/Math.h"

/// <summary>
/// パーティクル発生器（Emitter）
/// </summary>
class ParticleEmitter
{
public:
    // =========================
    // コンストラクタ
    // =========================
    ParticleEmitter(
        const std::string& groupName, // ParticleGroup 名
        const Math::Vector3& position,
        float emitInterval,           // 発生間隔（秒）
        uint32_t emitCount             // 1回の発生数
    );

    // =========================
    // 更新
    // =========================
    void Update(float deltaTime);

    // =========================
    // 手動発生
    // =========================
    void Emit();

    // =========================
    // 位置操作（必要なら）
    // =========================
    void SetPosition(const Math::Vector3& pos) { position_ = pos; }

private:
    // ===== 設定 =====
    std::string groupName_;   // ParticleGroup名
    Math::Vector3 position_; // 発生位置

    float emitInterval_ = 0.1f; // 発生間隔
    uint32_t emitCount_ = 1;    // 発生数

    // ===== 内部状態 =====
    float elapsedTime_ = 0.0f;  // 経過時間
};
