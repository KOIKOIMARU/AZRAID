#pragma once

#include "engine/base/Math.h"

struct ImDrawList;

// 戦闘中の計器と短い画面フィードバック。ゲームのルールやアクターは所有しない。
class GameplayHud {
public:
    enum class BossCue { None, Counter, Fan, Sweep, Charge, Crossfire, Rush };
    struct FeverView {
        bool active = false, ready = false;
        float rate = 0.0f, seconds = 0.0f, activationFlash = 0.0f;
    };
    struct ChainView {
        int count = 0, multiplier = 1;
        float remaining = 0.0f, breakFlash = 0.0f, punch = 0.0f;
    };
    struct BossView {
        bool visible = false;
        BossCue cue = BossCue::None;
        float hp = 0.0f, action = 0.0f;
    };
    struct SkillView {
        bool active = false, ready = false, empowered = false, locked = false, noTarget = false;
        float cooldownSeconds = 0.0f, recovery = 0.0f, readyFlash = 0.0f;
    };
    // 一回の描画で読む値の写し。0〜1の比率は呼び出し側で各ルールから計算する。
    // 敵・カメラ・射撃クラスへの参照を持たず、描画からゲーム状態を書き換えない。
    struct Frame {
        Math::Vector2 min{}, size{ 1280, 720 }, reticle{};
        int hp = 100, maxHp = 100, score = 0, wave = 0, waveCount = 0;
        float clock = 0.0f, charge = 0.0f;
        float justDodge = 0.0f, bossDefeat = 0.0f, dodgeDirection = 1.0f;
        bool tutorial = false, chargeReady = false, bossSpawned = false;
        FeverView fever{};
        ChainView chain{};
        BossView boss{};
        SkillView skill{};
        bool reticleAligned = false;
        int ringNoticeFrames = 0;
        bool ringExtendedFever = false;
        float bossRadioSeconds = 0.0f; // 表示する場合だけ正値。通信の重複防止は進行側で管理。
        float bossRadioElapsed = 0.0f;
    };

    void Reset();
    void RecordHit(Math::Vector2 screen, bool charged, bool boss, bool destroyed);
    void RecordDamage(Math::Vector2 screen, Math::Vector2 incomingVelocity);
    void TickFeedback(); // 既存の更新順に合わせ、命中・被弾を処理した後に呼ぶ。ポーズ中は呼ばない。
    void TickCharge();   // 通常射撃の更新時だけ減らす。スキル中は発射の閃光を持ち越す。
    void FlashCharge(int frames);
    void FlashChargeShot();
    void ClearChargeFlash() { chargeFlash_ = 0; }

    void DrawRingNotice(const Frame& frame, ImDrawList* draw) const;
    void DrawCore(const Frame& frame, ImDrawList* draw) const;
    void DrawBoss(const Frame& frame, ImDrawList* draw) const;
    void DrawCombatFeedback(const Frame& frame, ImDrawList* draw) const;
    void DrawStatus(const Frame& frame, ImDrawList* draw) const;

#ifdef _DEBUG
    struct Snapshot {
        int hitFrames = 0, hitDuration = 1, damageFrames = 0, damageDuration = 1, chargeFlash = 0;
        Math::Vector2 hitScreen{}, damageScreen{}, damageDirection{};
    };
    Snapshot Inspect() const;
#endif

private:
    void DrawStageCue(const Frame& frame, ImDrawList* draw) const;
    void DrawReticle(const Frame& frame, ImDrawList* draw) const;
    void DrawHitConfirm(ImDrawList* draw) const;
    void DrawDamage(const Frame& frame, ImDrawList* draw) const;
    void DrawChain(const Frame& frame, ImDrawList* draw) const;
    void DrawFever(const Frame& frame, ImDrawList* draw) const;
    void DrawSkill(const Frame& frame, ImDrawList* draw) const;
    void DrawRadio(const Frame& frame, ImDrawList* draw) const;

    int chargeFlash_ = 0;
    int hitFrames_ = 0, hitDuration_ = 1;
    int damageFrames_ = 0, damageDuration_ = 1;
    float hitStrength_ = 1.0f;
    bool hitCharged_ = false, hitDestroyed_ = false;
    Math::Vector2 hitScreen_{}, damageScreen_{}, damageDirection_{ 0, -1 };
};
