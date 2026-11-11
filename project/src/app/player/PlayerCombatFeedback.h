// 自機の回避や命中を見せる短いスローと、演出の残り時間を管理する。

#pragma once

class PlayerCombatFeedback {
public:
    struct CameraShake {
        float strength = 0.0f;
        int frames = 3;
    };

    void StartJustDodge();
    CameraShake StartImpact(bool charged, bool bossHit, bool destroyed);
    void Tick();
    void ClearImpact();

    bool IsDodgeSlowActive() const { return dodgeSlowFrames_ > 0; }
    float DodgeRate() const;
    float DodgeRelease() const;
    float ImpactRate() const;
    float WorldTimeScale(float skillScale) const;

#ifdef _DEBUG
    int DodgeFlashFrames() const { return dodgeFlashFrames_; }
    int DodgeSlowFrames() const { return dodgeSlowFrames_; }
    bool IsImpactSlowActive() const { return impactSlowFrames_ > 0; }
    void ClearDodgeForDebug() { dodgeFlashFrames_ = dodgeSlowFrames_ = 0; }
    void ClearImpactSlowForDebug() { impactSlowFrames_ = 0; }
#endif

private:
    int dodgeFlashFrames_ = 0;
    int dodgeSlowFrames_ = 0;
    int impactFlashFrames_ = 0;
    int impactFlashDuration_ = 1;
    int impactSlowFrames_ = 0;
};
