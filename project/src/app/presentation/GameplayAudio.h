#pragma once

#include <array>
#include <cstdint>
#include <memory>

class SoundManager;

// 本編で使う音の準備とミックスを管理する　ゲーム進行からは場面と時刻を受け取る
class GameplayAudio {
public:
    enum class Track { Silent = -1, Stage, Boss, Fever };

    GameplayAudio();
    ~GameplayAudio();

    void BeginLoad() { loadStep_ = 0; }
    bool LoadStep();
    float LoadProgress() const;
    const char* LoadLabel() const;
    void Clear();

    // keyは効果音の識別名　pitchは再生周波数の倍率　elapsedSecondsは前回の更新からの経過秒数
    void PlayEffect(const char* key, float pitch, float elapsedSeconds, bool skillActive);
    void UpdateMusic(Track desiredTrack, float deltaTime, bool ducked, bool tutorial);

#ifdef _DEBUG
    // 既存の自動試験がボイス数と再生状態を検証するための入口
    SoundManager* SoundForProbe() const { return sound_.get(); }
    Track TrackForProbe() const { return track_; }
    float AccentEndForProbe() const { return accentUntil_; }
    bool UsesLocalSkillAudioForProbe() const { return phantomLocalAudio_; }
    const auto& SkillPlayCountsForProbe() const { return phantomAudioPlays_; }
    void ResetMixForProbe();
#endif

private:
    void LoadSkillSounds();
    void ResetMix();

    std::unique_ptr<SoundManager> sound_;
    std::array<float, 3> musicLevels_{};
    Track track_ = Track::Silent;
    int loadStep_ = 0;
    float accentUntil_ = 0.0f;
    bool phantomLocalAudio_ = false;
#ifdef _DEBUG
    std::array<uint32_t, 2> phantomAudioPlays_{};
#endif
};
