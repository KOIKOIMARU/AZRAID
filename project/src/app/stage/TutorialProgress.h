// 練習課題の現在地と達成条件を持つ　自機の入力や命中結果を受け取り、課題ごとの進行を決める

#pragma once

#include "engine/base/Math.h"

#include <array>
#include <optional>

struct ImDrawList;

// 課題の進行と案内表示を管理する　標的・弾・リングは呼び出し側が生成する
class TutorialProgress {
public:
    enum class Lesson { Move, Shoot, Dodge, Charge, Skill, Ring, Fever, Complete };
    static constexpr int kLessonCount = static_cast<int>(Lesson::Complete);

    struct Stats {
        int defeats;
        int skillUses;
        int skillDefeats;
        int ringPasses;
    };
    struct Targets {
        int count = 0;
        int hp = 0;
    };
    struct Setup {
        Targets targets{};
        int feverGauge = -1;
        bool prepareSkill = false;
        bool spawnRing = false;
    };
    struct Frame {
        Stats stats;
        Math::Vector3 playerPosition;
        bool targetsAlive;
        bool skillActive;
        bool ringActive;
        bool feverActive;
    };
    struct Events {
        std::optional<Lesson> nextLesson;
        Targets targets{};
        bool reachedMarker = false;
        bool clearedLesson = false;
        bool respawnRing = false;
    };
    struct Guide {
        bool chargeReady;
        bool skillActive;
        bool skillReady;
        bool ringActive;
        bool feverActive;
        bool incomingShot;
    };

    void Reset();
    Setup BeginLesson(Lesson lesson, Stats stats);
    Events Advance(float frameStep, const Frame& frame);
    bool AdvanceDodgeAttack(float frameStep);
    void OnDodgeShotFired();
    void ObserveDodge(const Math::Vector3& player, const Math::Vector3& bullet);
    void RecordChargedHit();
    bool IsDodgeTelegraphActive() const;
    float DodgeTelegraphRate() const;

    Lesson CurrentLesson() const { return lesson_; }
    bool Started() const { return started_; }
    bool ShowingSuccess() const { return success_ > 0.0f; }
    bool AllowsShooting() const;
    bool AllowsSkill() const;
    bool AllowsRing() const;
    bool AllowsFever() const;
    Math::Vector3 MoveTarget(float railDistance) const;
    static bool IsDodgePromptShot(const Math::Vector3& player, const Math::Vector3& bullet);

    static const std::array<const char*, kLessonCount>& LessonNames();
    void DrawGuide(ImDrawList& list, const Math::Vector2& min, const Math::Vector2& size,
        const Math::Matrix4x4* viewProjection, float railDistance, const Guide& guide) const;

#ifdef _DEBUG
    int ProgressForProbe() const { return progress_; }
    float ElapsedForProbe() const { return elapsed_; }
    bool ChargedHitForProbe() const { return chargedHit_; }
    bool DodgeConfirmedForProbe() const { return dodgeConfirmed_; }
    Stats StartingStatsForProbe() const { return start_; }
#endif

private:
    Lesson lesson_ = Lesson::Move;
    float elapsed_ = 0.0f; // 現在の課題の経過　60fps換算、ポーズ中は止める
    float success_ = 0.0f;
    float markerHold_ = 0.0f;
    float attackTimer_ = 120.0f;
    int attackTelegraph_ = 0;
    float feverExperience_ = 0.0f;
    int progress_ = 0;
    Stats start_{};
    bool chargedHit_ = false;
    bool dodgeConfirmed_ = false;
    bool started_ = false;
};
