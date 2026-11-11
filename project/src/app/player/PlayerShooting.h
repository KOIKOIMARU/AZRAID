#pragma once

// 自機の連射間隔・入力の先行受付・チャージを管理する。
// キーの読み取り、弾の確保、音や画面演出は呼び出し側が担当する。
class PlayerShooting {
public:
    static constexpr int kMaxChargeFrames = 100;
    static constexpr int kInputBufferFrames = 8;
    static constexpr int kFeverIntervalFrames = 9;

    struct Tuning {
        int chargeFrames = 66; // 射撃を休めて1.1秒でチャージ完了。
        int normalIntervalFrames = 14;
        int chargedIntervalFrames = 20;
    };
    struct InputState {
        bool held = false;
        bool triggered = false;
    };

    void Reset(); // 調整値は保ち、再挑戦・練習開始時の状態へ戻す。
    // 時間は従来どおり60fps換算。射撃可能なプレイ中に1フレーム1回呼ぶ。
    // 発射要求があるとtrue。弾を確保できなくても、連射間隔と先行入力は消化する。
    bool Update(InputState input, bool feverActive);
    // 弾を実際に生成できたときだけ呼ぶ。確保失敗ではチャージを消費しない。
    void OnShotFired(bool feverActive);
    void FillCharge();
    void AddCharge(int frames);
    void CapCooldown(int maximumFrames); // リング通過時など、次の発射までの待ち時間を短くする。

    void SetTuning(Tuning tuning);
    Tuning GetTuning() const { return tuning_; }
    bool IsChargeReady() const { return chargeFrames_ >= tuning_.chargeFrames; }
    float ChargeRate() const;
    int ChargeFrames() const { return chargeFrames_; }
    int CooldownFrames() const { return cooldownFrames_; }
    int BufferedFrames() const { return bufferedFrames_; }

#if defined(_DEBUG) || defined(ENABLE_DEBUG_GUI)
    void SetChargeForDebug(int frames);
    void SetCooldownForDebug(int frames);
#endif

private:
    Tuning tuning_{};
    int chargeFrames_ = 0;
    int cooldownFrames_ = 0;
    int bufferedFrames_ = 0;
};
