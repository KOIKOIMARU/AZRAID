// クリア・敗北・チュートリアルの結果画面を描き、再挑戦などの選択をGameplayMenuへ渡す。

#pragma once

#include "engine/base/Math.h"

#include <array>
#include <optional>
#include <span>

class GameplayMenu;
class Input;

class StageResultMenu {
public:
    struct Record {
        int bestScore = 0;
        bool newBest = false;
        bool saved = false;
    };

    struct Stats {
        int score = 0;
        int maxChain = 0;
        int damageCount = 0;
        int defeatedCount = 0;
        int targetCount = 0;
        int waveNumber = 1;
        int waveCount = 1;
        int shots = 0;
        int hits = 0;
        int justDodges = 0;
        int escapedCount = 0;
        int ringPasses = 0;
        int ringSpawns = 0;
        float elapsedSeconds = 0.0f;
        std::optional<Record> record;
    };

    void ResetDisplay();
    void ResetSelection() { selectedItem_ = 0; }
    float UpdateClear();
    bool IsClearVisible() const { return kind_ == Kind::Clear; }

    void DrawClear(Input* input, const Math::Vector2& min, const Math::Vector2& size,
        const Stats& stats, float elapsed, GameplayMenu& menu);
    void DrawDefeat(Input* input, const Math::Vector2& min, const Math::Vector2& size,
        const Stats& stats, float deltaSeconds, GameplayMenu& menu);
    void DrawTutorial(Input* input, const Math::Vector2& min, const Math::Vector2& size,
        std::span<const char* const> lessons, GameplayMenu& menu);

#ifdef _DEBUG
    bool IsDefeatVisible() const { return kind_ == Kind::Defeat; }
    bool IsDetailsOpen() const { return details_; }
    int SelectedItem() const { return selectedItem_; }
#endif

private:
    enum class Kind { Hidden, Clear, Defeat };
    float BeginDisplay(Kind kind);

    Kind kind_ = Kind::Hidden;
    double started_ = -1.0;
    int selectedItem_ = 0;
    bool details_ = false;
    std::array<float, 2> defeatActionEmphasis_{};
};
