#pragma once

class PlayerDodge {
public:
    static constexpr float kDuration = 16.0f;

    // 1回の更新で自機へ足す横移動量と、描画で使うロール角を返す
    struct Step {
        float distance = 0.0f;
        float roll = 0.0f;
    };

    // 回避の時間は実時間の60fps換算　ワールドのスロー中も操作の長さを保つ
    bool UpdateInput(bool pressed, int horizontalInput, float controlStep);
    Step Advance(float controlStep);
    bool IsActive() const { return remaining_ > 0.0f; }
    int Direction() const { return direction_; }
    // 1回の回避につき、ジャスト回避の成立を一度だけ受け付ける
    bool TryConfirm();

private:
    float remaining_ = 0.0f;
    float cooldown_ = 0.0f;
    float inputBuffer_ = 0.0f;
    int bufferedDirection_ = 1;
    int direction_ = 1;
    int lastHorizontalDirection_ = 1;
    bool confirmed_ = false;
};
