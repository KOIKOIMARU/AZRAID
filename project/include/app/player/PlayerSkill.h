#pragma once

#include "engine/base/Math.h"
#include <array>
#include <span>

class Enemy;

// スキルの対象・攻撃順・再使用までの時間。敵の操作と描画は呼び出し側で行う。
class PlayerSkill {
public:
    static constexpr int kMaxTargets = 5;
    static constexpr float kCooldownFrames = 18.0f * 60.0f;
    static constexpr float kWindupFrames = 12.0f;
    static constexpr float kStrikeInterval = 7.0f;
    struct Target {
        const Enemy* enemy = nullptr; // 所有しない。敵を破棄する前にInvalidateTargetで参照を外す。
        Math::Vector3 position{};     // 消滅後も最後に確認した位置で演出を続ける。
        int marks = 0;
    };

    // 画面内にいる候補を一度ずつ渡す。距離が同じ場合は渡された順番を保つ。
    // 固定長の上位候補だけを保持し、ボスを選んだところで対象を打ち切る。
    class Selection {
    public:
        explicit Selection(bool empowered) : empowered_(empowered) {}
        void Consider(const Enemy* enemy, Math::Vector3 position, float distanceSquared, bool boss);
    private:
        friend class PlayerSkill;
        struct Entry { Target target{}; float distance = 0.0f; bool boss = false; };
        std::array<Entry, kMaxTargets> entries_{};
        int count_ = 0;
        bool empowered_ = false;
    };

    void Reset();
    bool TryActivate(const Selection& selection);
    void TickTimers(float frameStep); // 60fps換算。ポーズ中は呼ばない。
    bool TryRecover();               // 使用可能へ戻った瞬間だけtrueを返す。
    void RecoverOnHit(bool charged, bool destroyed);
    void MakeReadyForPractice();     // 練習の開始条件を揃える。集計は残す。

    void AdvanceSequence(float frameStep);
    int ConsumeStrike();            // 今回の一撃の番号。まだ時刻に達していなければ-1。
    void MarkStrike(int strikeIndex, Math::Vector3 position);
    bool ConsumeFinisher();         // 最後の一撃を二重に処理しない。
    void EndSequenceIfElapsed();
    void CancelSequence();          // 参照と進行を外す。再使用時間と集計は残す。
    void UpdateTargetPosition(const Enemy* enemy, Math::Vector3 position);
    void InvalidateTarget(const Enemy* enemy);
    void RecordDefeat() { ++defeatCount_; }

    bool IsActive() const { return clock_ >= 0.0f; }
    bool IsReady() const { return ready_; }
    bool IsEmpowered() const { return empowered_; }
    bool IsFinished() const { return finished_; }
    float Clock() const { return clock_; }
    float CooldownFrames() const { return cooldown_; }
    float ReadyFlashFrames() const { return readyFlash_; }
    float NoTargetNoticeFrames() const { return noTargetNotice_; }
    int TargetCount() const { return targetCount_; }
    int NextStrike() const { return nextStrike_; }
    int ActivationCount() const { return activationCount_; }
    int DefeatCount() const { return defeatCount_; }
    int FinisherDamage() const { return empowered_ ? 4 : 3; }
    float FinisherTime() const { return kWindupFrames + static_cast<float>(strikeCount_) * kStrikeInterval; }
    std::span<const Target> Targets() const { return { targets_.data(), static_cast<size_t>(targetCount_) }; }
    float WorldTimeScale() const;
    bool ProtectsPlayer() const { return IsActive() && clock_ < FinisherTime() + 7.0f; }

#if defined(_DEBUG) || defined(ENABLE_DEBUG_GUI)
    void SetRechargeForDebug(float frames, bool ready = false);
    void ClearCooldownForDebug() { cooldown_ = 0.0f; }
#endif

private:
    std::array<Target, kMaxTargets> targets_{};
    bool ready_ = true;
    bool empowered_ = false;
    bool finished_ = false;
    float clock_ = -1.0f;
    float cooldown_ = 0.0f;
    float readyFlash_ = 0.0f;
    float noTargetNotice_ = 0.0f;
    int targetCount_ = 0;
    int strikeCount_ = 3;
    int nextStrike_ = 0;
    int activationCount_ = 0;
    int defeatCount_ = 0;
};
