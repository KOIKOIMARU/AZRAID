// 戦闘中のポーズと操作ヘルプを管理する。再開や再挑戦は画面演出の完了後に確定する。

#pragma once

#include "engine/base/Math.h"

class Input;

class GameplayMenu {
public:
    enum class Action { None, Resume, Controls, Back, Retry, Exit, MainGame };

    void HandleShortcuts(Input* input, bool canPause);
    bool Begin(Action action);
    Action UpdateTransition();
    float TransitionProgress() const;

    void DrawPause(Input* input, const Math::Vector2& min, const Math::Vector2& size,
        int score, int hp, int maxHp);
    void DrawControls(const Math::Vector2& min, const Math::Vector2& size);

    bool IsPaused() const { return paused_; }
    bool IsControlsVisible() const { return controlsOpen_ || action_ == Action::Controls; }
    Action PendingAction() const { return action_; }
    Action SceneAction() const { return sceneAction_; }

#ifdef _DEBUG
    int SelectedPauseItem() const { return selectedItem_; }
    // 検証でポーズとヘルプの表示状態を直接設定する。解除時はポーズの経過時刻も戻す。
    void SetPausedForDebug(bool paused, bool controls = false)
    {
        paused_ = paused;
        controlsOpen_ = controls;
        if (!paused_) { pauseStarted_ = -1.0; }
    }
#endif

private:
    Action action_ = Action::None;
    Action sceneAction_ = Action::None;
    double actionStarted_ = -1.0;
    double pauseStarted_ = -1.0;
    int selectedItem_ = 0;
    bool paused_ = false;
    bool controlsOpen_ = false;
    bool inputConsumed_ = false;
};
