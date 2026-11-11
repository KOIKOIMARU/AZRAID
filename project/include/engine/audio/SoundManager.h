// XAudio2の再生機能を管理する。名前で音声を読み込み、効果音とループ音を再生する。

#pragma once
#include <xaudio2.h>
#include <wrl/client.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

struct SoundData {
    WAVEFORMATEX wfex{};
    std::vector<BYTE> buffer; // PCM波形。再生ボイスを破棄するまで保持する。
};

// 効果音と短いBGMループ用。ロード時だけボイスとPCMを確保し、戦闘中にはデコードしない。
class SoundManager {
public:
    // 既存の効果音は方向性を見直すまで停止。確認した音は個別に有効化できる。
    static constexpr bool kSoundEffectsEnabled = false;
    SoundManager() = default;
    ~SoundManager();
    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;
    bool Initialize(); // 音声デバイスが使えなくてもゲームは継続できる。
    void Finalize();
    bool Load(const std::string& key, const std::string& filename,
        uint32_t voiceCount = 3, float volume = 0.5f, float minimumInterval = 0.04f,
        float pitchVariation = 0.0f, float gainVariation = 0.0f);
    // 同時に1曲だけCPUでデコード。falseは準備中、trueは完了（音声無効・読込失敗も含む）。
    // XAudio2のボイス作成・登録は呼出し側だけで行い、曲や再生設定は変えない。
    bool PreloadLoopStep(const std::string& key, const std::string& filename);
    // 同じ用途の別テイクを最大4個保持する。再生枠は増やさず、同じテイクの連続を避ける。
    bool LoadVariations(const std::string& key, const std::vector<std::string>& filenames,
        uint32_t voiceCount, float volume, float minimumInterval,
        float pitchVariation = 0.0f, float gainVariation = 0.0f);
    void Unload(const std::string& key);
    // pitchは演出の音程倍率。連撃の段階に使い、BGMには適用しない。
    bool Play(const std::string& key, float gain = 1.0f, float pitch = 1.0f); // 満杯時は追加生成しない。
    void EnableWhileEffectsMuted(const std::string& key); // 読込済みの一音だけを明示的に採用する。
    bool PlayLoop(const std::string& key); // 1ボイスでロードした曲を無限ループ。再呼び出しで重ねない。
    void Stop(const std::string& key);
    void SetVolume(const std::string& key, float volume);
    uint32_t GetLoopCount() const;
    uint32_t GetVoiceCount() const { return voiceCount_; }
    uint64_t GetPlayCount() const { return playCount_; }
private:
    struct Entry {
        std::vector<SoundData> variations; // 初期化時に読み込むPCM。再生中は追加・デコードしない。
        std::array<IXAudio2SourceVoice*, 4> voices{};
        float minimumInterval = 0.04f; // 同じ音の重なり過ぎを防ぐ秒数。
        float volume = 0.5f;
        float pitchVariation = 0.0f; // 再生速度の微変化幅。0なら元音程（BGM・通知音）。
        float gainVariation = 0.0f; // 強さの微変化幅。0なら一定。
        uint32_t lastVariation = 0;
        uint32_t randomState = 0xA341316Cu; // ゲームの敵配置等とは独立した、音専用の乱数状態。
        bool looping = false;
        bool enabledWhileMuted = false;
        std::chrono::steady_clock::time_point lastPlay{};
    };
    static SoundData LoadFile(const std::string& filename);
    bool InstallVariations(const std::string& key, std::vector<SoundData> variations,
        uint32_t voiceCount, float volume, float minimumInterval, float pitchVariation, float gainVariation);
    void DestroyEntry(Entry& entry);
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    IXAudio2MasteringVoice* masterVoice_ = nullptr;
    std::unordered_map<std::string, Entry> sounds_;
    bool mediaFoundationStarted_ = false;
    uint32_t voiceCount_ = 0; // 効果音・BGM合計の確保数。最大32。
    uint64_t playCount_ = 0;
    std::future<SoundData> pendingMusic_;
    std::string pendingMusicKey_, pendingMusicFile_;
};
