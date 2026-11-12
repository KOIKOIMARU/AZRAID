#pragma once

// フィーバーのゲージ・制限時間・発動回数　音や画面演出は呼び出し側が扱う
class FeverState {
public:
    static constexpr int kGaugeMax = 100;
    static constexpr int kDurationFrames = 360; // 基本6秒　常時維持ではなく、攻める区間を作る
    static constexpr int kActivationFlashFrames = 90;
    static constexpr int kScoreMultiplier = 3;

    void Reset();
    // 時間は60fps換算のフレーム数で、1回のTickで1減らす
    // 更新開始時に発動中だったかを返し、終了フレームもチャージを満タンにする
    bool Tick();
    void Activate();
    void Stop(bool resetSpeedEffect = false); // 停止後は加速演出をなだらかに戻し、死亡時は即座に消す
    void SetGauge(int amount);
    void AddGauge(int amount);
    void RemoveGauge(int amount);
    // 発動中の残り時間を通常の最大時間まで伸ばし、実際に増えたフレーム数を返す
    int Extend(int frames);

    bool IsActive() const { return remainingFrames_ > 0; }
    bool IsReady() const { return !IsActive() && gauge_ >= kGaugeMax; }
    int Gauge() const { return gauge_; }
    int RemainingFrames() const { return remainingFrames_; }
    int ActivationFlashFrames() const { return activationFlashFrames_; }
    int ActivationCount() const { return activationCount_; }
    float SpeedEffectRate() const { return speedEffectRate_; }
    int ScoreMultiplier() const { return IsActive() ? kScoreMultiplier : 1; }

#if defined(_DEBUG) || defined(ENABLE_DEBUG_GUI)
    void SetRemainingFramesForDebug(int frames) { remainingFrames_ = frames; }
    void SetSpeedEffectForDebug(float rate) { speedEffectRate_ = rate; }
    void SetActivationCountForDebug(int count) { activationCount_ = count; }
#endif

private:
    int gauge_ = 0;
    int remainingFrames_ = 0;
    int activationFlashFrames_ = 0;
    int activationCount_ = 0;
    float speedEffectRate_ = 0.0f;
};
