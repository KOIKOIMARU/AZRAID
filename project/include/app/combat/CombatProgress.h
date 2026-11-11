#pragma once

// スコアと連続撃破のルール。敵・描画・入力のクラスには依存しない。
class CombatProgress {
public:
    static constexpr int kChainDurationFrames = 210;
    static constexpr int kChainBreakFlashFrames = 42;
    static constexpr int kChainMaxCount = 99;
    static constexpr int kChainScoreTierSize = 4;
    static constexpr int kChainMaxMultiplier = 4;

    void Reset();
    void ResetChain();
    // フィーバーなどの追加倍率を受け取り、連続撃破の倍率と合わせて得点へ反映する。
    void AddScore(int baseScore, int bonusMultiplier);
    // 敵がいない移動区間では猶予を減らさず、終了中は連続撃破の進行も止める。
    void Update(bool hasTarget, bool gameplayRunning);
    void RegisterDefeat();
    void BreakChain();
    void ExtendChain(int minimumFrames);
    void RefreshChain();

    int Score() const { return score_; }
    int ChainCount() const { return chainCount_; }
    int ChainRemainingFrames() const { return chainRemainingFrames_; }
    int ChainBreakFlashFrames() const { return chainBreakFlashFrames_; }
    int MaxChainCount() const { return maxChainCount_; }
    int ChainMultiplier() const;

#if defined(_DEBUG) || defined(ENABLE_DEBUG_GUI)
    void SetScoreForDebug(int score) { score_ = score; }
    void SetChainForDebug(int count, int remainingFrames = kChainDurationFrames) {
        chainCount_ = count;
        chainRemainingFrames_ = remainingFrames;
    }
#endif

private:
    int score_ = 0;
    int chainCount_ = 0;
    int chainRemainingFrames_ = 0;
    int chainBreakFlashFrames_ = 0;
    int maxChainCount_ = 0;
};
