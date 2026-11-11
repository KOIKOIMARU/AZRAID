// ===== BossPresentation.cpp =====
// ボスの警告、登場、形態移行、撃破に合わせて、演出と管制表示のタイミングを決める。

#include "app/enemy/BossPresentation.h"

#include <algorithm>

namespace {
constexpr int kIntroFrames = 72;
constexpr int kPhaseTransitionFrames = 36;
constexpr int kDefeatFrames = 44;
constexpr float kRadioSeconds = 3.8f;
}

void BossPresentation::Reset()
{
    *this = {};
}

// 最初に警告条件を満たしたフレームでだけtrueを返し、ボス接近の演出を一度に限定する。
bool BossPresentation::TryBeginWarning()
{
    if (warningTriggered_) { return false; }
    warningTriggered_ = true;
    return true;
}

void BossPresentation::BeginIntro()
{
    introFrames_ = kIntroFrames;
    phaseFrames_ = 0;
}

// 接近・登場に使う表示と待ち時間を解除し、次の状態へ移れるようにする。
void BossPresentation::CancelArrival()
{
    EndIntro();
    radioRequested_ = false;
    radioRemaining_ = 0.0f;
}

// 形態が切り替わったときの演出時間を設定し、その間の攻撃を止める。
void BossPresentation::BeginPhaseTransition()
{
    phaseFrames_ = kPhaseTransitionFrames;
}

// ボスの撃破地点を控え、そこで連続爆発を始める。
void BossPresentation::BeginDefeat(const Math::Vector3& position)
{
    defeatPosition_ = position; // 機体が破棄された後も、最後の位置から爆発を再生する。
    defeatFrames_ = kDefeatFrames;
}

// ボス警告・登場・形態変更・撃破の演出を進める。撃破中は時刻に応じて爆発の指示を返す。
std::optional<BossPresentation::Burst> BossPresentation::AdvanceFrame()
{
    if (introFrames_ > 0) { --introFrames_; }
    if (phaseFrames_ > 0) { --phaseFrames_; }
    if (!IsDefeatPlaying()) { return std::nullopt; }

    --defeatFrames_;
    if (defeatFrames_ != 32 && defeatFrames_ != 20 && defeatFrames_ != 8) { return std::nullopt; }

    // 左右、中央の順に時間差で爆発させる。
    const int index = (32 - defeatFrames_) / 12;
    constexpr Math::Vector3 offsets[] = {
        { -2.2f, 0.5f, -0.8f }, { 2.2f, -0.3f, 0.4f }, { 0.0f, 1.0f, 0.2f }
    };
    const auto& offset = offsets[index];
    const bool finalBurst = index == 2;
    return Burst{
        { defeatPosition_.x + offset.x, defeatPosition_.y + offset.y, defeatPosition_.z + offset.z },
        finalBurst ? 2.4f : 1.55f,
        finalBurst ? 0.26f : 0.12f,
        finalBurst ? 24 : 10
    };
}

float BossPresentation::DefeatFlashRate() const
{
    return static_cast<float>(defeatFrames_) / kDefeatFrames;
}

// 同じ戦闘中に一度だけ、ボス接近の通信表示を開始する。
void BossPresentation::RequestRadio()
{
    if (radioRequested_) { return; }
    radioRequested_ = true;
    radioRemaining_ = kRadioSeconds;
}

// 戦闘中は通信の残り時間を減らし、戦闘が終わった場合は表示を閉じる。
void BossPresentation::AdvanceRadio(float deltaSeconds, bool activeBattle)
{
    if (!activeBattle) {
        radioRemaining_ = 0.0f;
        return;
    }
    radioRemaining_ = (std::max)(0.0f, radioRemaining_ - std::clamp(deltaSeconds, 0.0f, 0.1f));
}

float BossPresentation::RadioElapsed() const
{
    return kRadioSeconds - radioRemaining_;
}
