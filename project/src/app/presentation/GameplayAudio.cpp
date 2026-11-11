// ===== GameplayAudio.cpp =====
// 戦闘で使う音源を準備し、プレイの状態に応じてBGMや効果音の再生を切り替える。

#include "app/presentation/GameplayAudio.h"

#include "engine/audio/SoundManager.h"
#include "engine/base/Logger.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <string_view>

namespace {
constexpr const char* kLoadLabels[] = {"Audio device", "Shot audio", "Charge audio", "Skill audio", "Hit audio",
    "Destroy audio", "Damage audio", "Dodge audio", "Fever audio", "Clear audio", "Failure audio",
    "Ready audio", "Phantom audio", "Stage music", "Boss music", "Fever music"};
}

GameplayAudio::GameplayAudio() = default;
GameplayAudio::~GameplayAudio() = default;

float GameplayAudio::LoadProgress() const
{
    return static_cast<float>(loadStep_) / static_cast<float>(std::size(kLoadLabels));
}

const char* GameplayAudio::LoadLabel() const
{
    return loadStep_ < std::ssize(kLoadLabels) ? kLoadLabels[loadStep_] : "Audio";
}

void GameplayAudio::Clear()
{
    sound_.reset(); // ボイスを破棄してから波形を解放する。
}

void GameplayAudio::ResetMix()
{
    musicLevels_.fill(0.0f);
    track_ = Track::Silent;
}

#ifdef _DEBUG
void GameplayAudio::ResetMixForProbe()
{
    ResetMix();
}
#endif

// 音声を定めた順で少しずつ読み込む。準備完了を呼び出し元へ返し、出撃前のロード進捗に使う。
bool GameplayAudio::LoadStep()
{
    if (loadStep_ == 0) {
        ResetMix();
        accentUntil_ = 0.0f;
        sound_ = std::make_unique<SoundManager>();
        if (!sound_->Initialize()) { return true; }
        ++loadStep_;
        return false;
    }
    const auto varied = [this](const char* key, uint32_t voices, float volume, float interval, float pitch) {
        const std::string base = std::string("resources/audio/combat/") + key;
        sound_->LoadVariations(key, { base + ".wav", base + "_02.wav", base + "_03.wav" },
            voices, volume, interval, pitch, 0.05f);
    };
    switch (loadStep_) {
    case 1: varied("shot", 4, 0.38f, 0.065f, 0.012f); break;
    case 2: sound_->Load("charge", "resources/audio/combat/charge.wav", 2, 0.51f, 0.16f, 0.012f, 0.025f); break;
    case 3: sound_->Load("skill_start", "resources/audio/combat/skill_start.wav", 1, 0.52f, 0.4f); break;
    case 4: varied("hit", 3, 0.35f, 0.075f, 0.025f); break;
    case 5: varied("destroy", 3, 0.49f, 0.12f, 0.025f); break;
    case 6: sound_->Load("damage", "resources/audio/combat/damage.wav", 2, 0.60f, 0.15f, 0.012f); break;
    case 7: sound_->Load("dodge", "resources/audio/combat/just_dodge.wav", 2, 0.48f, 0.12f); break;
    case 8: sound_->Load("fever", "resources/audio/combat/fever.wav", 1, 0.59f, 0.5f); break;
    case 9: sound_->Load("clear", "resources/audio/combat/clear.wav", 1, 0.54f, 1.0f); break;
    case 10: sound_->Load("fail", "resources/audio/combat/fail.wav", 1, 0.48f, 1.0f); break;
    case 11:
        sound_->Load("skill_ready", "resources/audio/combat/skill_ready.wav", 1, 0.29f, 0.5f);
        sound_->Load("gate_boost", "resources/audio/combat/gate_boost.wav", 1, 0.62f, 0.30f);
        break;
    case 12: LoadSkillSounds(); break;
    case 13: if (!sound_->PreloadLoopStep("music_stage", "resources/audio/music/stage.wav")) { return false; } break;
    case 14: if (!sound_->PreloadLoopStep("music_boss", "resources/audio/music/boss.wav")) { return false; } break;
    case 15: if (!sound_->PreloadLoopStep("music_fever", "resources/audio/music/fever.wav")) { return false; } break;
    default: return true;
    }
    return ++loadStep_ >= std::ssize(kLoadLabels);
}

// スキルの構え、連撃、最後の一撃に使う音を準備する。
void GameplayAudio::LoadSkillSounds()
{
    phantomLocalAudio_ = false;
#ifdef _DEBUG
    phantomAudioPlays_.fill(0);
#endif
    if (!sound_) { return; }
    // Sonniss素材の加工WAVは公開リポジトリへ入れない。通常配布は既存CC0バンクを使う。
    // 入場時だけ探索・デコードし、プレイ中のファイルアクセスやボイス追加はしない。
    const std::string local = "../generated/audio-phantom-runtime/";
    char* setting = nullptr;
    size_t settingLength = 0;
    const bool originalOnly = _dupenv_s(&setting, &settingLength, "AZRAID_PHANTOM_AUDIO") == 0 &&
        setting && std::string_view(setting) == "original";
    std::free(setting);
    bool complete = !originalOnly;
    for (const char* name : { "slash.wav", "slash_02.wav", "slash_03.wav", "slash_finish.wav" }) {
        std::error_code error;
        complete = complete && std::filesystem::is_regular_file(local + name, error);
    }
    if (complete) {
        const bool slashLoaded = sound_->LoadVariations("slash",
            { local + "slash.wav", local + "slash_02.wav", local + "slash_03.wav" },
            3, 0.66f, 0.060f, 0.006f, 0.025f);
        const bool finishLoaded = sound_->Load("slash_finish", local + "slash_finish.wav",
            1, 0.78f, 0.4f);
        phantomLocalAudio_ = slashLoaded && finishLoaded;
        if (!phantomLocalAudio_) {
            // 一部だけ成功した場合も両方を戻す。欠落や破損で連撃が無音にならない。
            sound_->Unload("slash");
            sound_->Unload("slash_finish");
        }
    }
    if (!phantomLocalAudio_) {
        sound_->LoadVariations("slash", { "resources/audio/combat/slash.wav",
            "resources/audio/combat/slash_02.wav", "resources/audio/combat/slash_03.wav" },
            3, 0.57f, 0.060f, 0.012f, 0.05f);
        sound_->Load("slash_finish", "resources/audio/combat/slash_finish.wav", 1, 0.68f, 0.4f);
    }
    Logger::Log(phantomLocalAudio_ ? "Phantom audio: local cinematic bank loaded.\n" :
        "Phantom audio: original CC0 bank loaded.\n");
}

// 効果音を名前で再生する。スキルなどの目立たせたい音が鳴る間は、通常の射撃音・命中音・撃破音を下げる。
void GameplayAudio::PlayEffect(const char* key, float pitch, float elapsedSeconds, bool skillActive)
{
    if (sound_) {
        const std::string_view cue(key);
        const bool accent = elapsedSeconds < accentUntil_;
        float gain = 1.0f;
        // 主役となる一音に場所を空ける。被弾通知と技本体は抑えない。
        if (cue == "shot") { gain = skillActive ? 0.38f : accent ? 0.58f : 1.0f; }
        else if (cue == "hit") { gain = skillActive ? 0.45f : accent ? 0.68f : 1.0f; }
        else if (cue == "destroy") { gain = skillActive ? 0.24f : accent ? 0.56f : 1.0f; }
        if (sound_->Play(key, gain, pitch)) {
#ifdef _DEBUG
            if (cue == "slash") { ++phantomAudioPlays_[0]; }
            else if (cue == "slash_finish") { ++phantomAudioPlays_[1]; }
#endif
            const float hold = cue == "slash_finish" ? 0.55f : cue == "fever" ? 0.48f :
                cue == "gate_boost" ? 0.25f : cue == "skill_start" ? 0.25f : cue == "damage" ? 0.22f : cue == "charge" ? 0.18f : 0.0f;
            if (hold > 0.0f) {
                accentUntil_ = (std::max)(accentUntil_, elapsedSeconds + hold);
            }
        }
    }
}

// 戦闘状況で再生するBGMを選び、音量を補間して曲の切り替えをなじませる。
void GameplayAudio::UpdateMusic(Track desiredTrack, float deltaTime, bool ducked, bool tutorial)
{
    if (!sound_) { return; }
    constexpr const char* keys[] = { "music_stage", "music_boss", "music_fever" };
    constexpr float volumes[] = { 0.28f, 0.32f, 0.30f };
    const int desired = static_cast<int>(desiredTrack);
    if (desiredTrack != track_) {
        if (desired >= 0 && !sound_->PlayLoop(keys[desired])) {
            Logger::Log("Music loop unavailable; gameplay continues.\n");
        }
        track_ = desiredTrack;
    }
    const float dt = std::clamp(deltaTime, 0.0f, 0.1f);
    const float duck = ducked ? 0.42f : 1.0f;
    for (int index = 0; index < 3; ++index) {
        const float target = index == desired ? volumes[index] * duck * (tutorial ? 0.70f : 1.0f) : 0.0f;
        auto& level = musicLevels_[index];
        const float speed = volumes[index] / (target > level ? 0.70f : 0.35f);
        level += std::clamp(target - level, -speed * dt, speed * dt);
        sound_->SetVolume(keys[index], level);
        if (index != desired && level <= 0.0f) { sound_->Stop(keys[index]); }
    }
}
