#pragma once

#include "engine/base/Math.h"

#include <optional>

// ボス接近の通信、登場・形態移行の待ち時間、撃破後の閃光と爆発の時刻を管理する。
class BossPresentation {
public:
    static constexpr int kResultDelayFrames = 120;

    struct Burst {
        Math::Vector3 position;
        float strength;
        float shakePower;
        int shakeFrames;
    };

    void Reset();
    bool TryBeginWarning();
    void BeginIntro();
    void EndIntro() { introFrames_ = 0; }
    void CancelArrival();
    void BeginPhaseTransition();
    void BeginDefeat(const Math::Vector3& position);
    std::optional<Burst> AdvanceFrame();

    bool IsIntroPlaying() const { return introFrames_ > 0; }
    bool IsPhaseTransitionPlaying() const { return phaseFrames_ > 0; }
    // 登場中と形態移行中は、ボスの射撃を始められない状態にする。
    bool BlocksAttacks() const { return IsIntroPlaying() || IsPhaseTransitionPlaying(); }
    bool IsDefeatPlaying() const { return defeatFrames_ > 0; }
    float DefeatFlashRate() const;

    void RequestRadio();
    void AdvanceRadio(float deltaSeconds, bool activeBattle);
    bool HasRadio() const { return radioRemaining_ > 0.0f; }
    float RadioRemaining() const { return radioRemaining_; }
    float RadioElapsed() const;

#ifdef _DEBUG
    bool WasRadioRequestedForProbe() const { return radioRequested_; }
#endif

private:
    bool warningTriggered_ = false;
    int introFrames_ = 0;
    int phaseFrames_ = 0;
    int defeatFrames_ = 0;
    Math::Vector3 defeatPosition_{};
    bool radioRequested_ = false;
    float radioRemaining_ = 0.0f;
};
