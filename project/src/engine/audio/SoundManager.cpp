// ===== SoundManager.cpp =====
// 音源の読み込みとXAudio2での単発・ループ再生を管理する

#include "engine/audio/SoundManager.h"
#include "engine/base/Logger.h"
#include "engine/base/StringUtility.h"
#include <algorithm>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

using Microsoft::WRL::ComPtr;
SoundManager::~SoundManager() { Finalize(); }

// 音声の読込に使うMedia Foundationと、再生先になるXAudio2を初期化する
bool SoundManager::Initialize()
{
    Finalize();
    if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET))) {
        Logger::Log("Audio disabled: Media Foundation initialization failed.\n");
        return false;
    }
    mediaFoundationStarted_ = true;
    if (FAILED(XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR)) ||
        FAILED(xAudio2_->CreateMasteringVoice(&masterVoice_))) {
        Logger::Log("Audio disabled: output device unavailable.\n");
        Finalize();
        return false;
    }
    masterVoice_->SetVolume(0.60f);
    playCount_ = 0;
    return true;
}

// 登録済みの再生ボイスを破棄してから、ボイスが参照していた音声データを解放する
void SoundManager::DestroyEntry(Entry& entry)
{
    // DestroyVoice完了後はこの波形がオーディオスレッドから参照されない
    for (auto*& voice : entry.voices) {
        if (voice) {
            voice->DestroyVoice();
            voice = nullptr;
            --voiceCount_;
        }
    }
    entry.variations.clear();
}

// BGMの先読みが終わるのを待ち、再生ボイスと音声基盤を順に終了する
void SoundManager::Finalize()
{
    // デコーダーのCOMオブジェクトが解放されてからMedia Foundationを終了する
    if (pendingMusic_.valid()) { pendingMusic_.wait(); }
    pendingMusic_ = {};
    pendingMusicKey_.clear();
    pendingMusicFile_.clear();
    for (auto& item : sounds_) {
        DestroyEntry(item.second);
    }
    sounds_.clear();
    if (masterVoice_) {
        masterVoice_->DestroyVoice();
        masterVoice_ = nullptr;
    }
    xAudio2_.Reset();
    if (mediaFoundationStarted_) {
        MFShutdown();
        mediaFoundationStarted_ = false;
    }
}

bool SoundManager::Load(const std::string& key, const std::string& filename,
    uint32_t voiceCount, float volume, float minimumInterval,
    float pitchVariation, float gainVariation)
{
    return LoadVariations(key, { filename }, voiceCount, volume, minimumInterval,
        pitchVariation, gainVariation);
}

// BGMの読込を別スレッドで進め、準備中はfalseを返す　デコード結果がそろったフレームで再生用に登録する
bool SoundManager::PreloadLoopStep(const std::string& key, const std::string& filename)
{
    if (!xAudio2_ || !masterVoice_) { return true; }
    if (pendingMusic_.valid()) {
        if (pendingMusicKey_ != key || pendingMusicFile_ != filename ||
            pendingMusic_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) { return false; }
        auto data = pendingMusic_.get();
        pendingMusicKey_.clear();
        pendingMusicFile_.clear();
        if (data.buffer.empty()) {
            Logger::Log("Audio preload failed: " + filename + "\n");
            return true;
        }
        if (!sounds_.contains(key)) {
            std::vector<SoundData> variations;
            variations.push_back(std::move(data));
            InstallVariations(key, std::move(variations), 1, 0.0f, 0.0f, 0.0f, 0.0f);
        }
        return true;
    }
    if (sounds_.contains(key)) { return true; }
    pendingMusicKey_ = key;
    pendingMusicFile_ = filename;
    pendingMusic_ = std::async(std::launch::async, [filename]() {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(result)) { return SoundData{}; }
        // workerは独立したデコーダーとPCMだけを所有　SoundManagerや再生ボイスに触れない
        try {
            auto data = LoadFile(filename);
            CoUninitialize();
            return data;
        } catch (...) { CoUninitialize(); throw; }
    });
    return false;
}

// 同じ効果音として鳴らす複数の素材を読み込み、同時再生数の上限内で登録する
bool SoundManager::LoadVariations(const std::string& key, const std::vector<std::string>& filenames,
    uint32_t voiceCount, float volume, float minimumInterval,
    float pitchVariation, float gainVariation)
{
    if (!xAudio2_ || !masterVoice_ || filenames.empty() || filenames.size() > 4) {
        return false;
    }
    Unload(key);
    voiceCount = std::clamp(voiceCount, 1u, 4u);
    if (voiceCount_ + voiceCount > 32u) {
        return false;
    }
    std::vector<SoundData> variations;
    variations.reserve(filenames.size());
    for (const auto& filename : filenames) {
        auto data = LoadFile(filename);
        if (data.buffer.empty()) {
            Logger::Log("Audio load failed: " + filename + "\n");
            return false;
        }
        // LoadFileは全素材を48kHz/16bit/2ch PCMへ統一するので、ボイスを共有できる
        variations.push_back(std::move(data));
    }
    return InstallVariations(key, std::move(variations), voiceCount, volume, minimumInterval,
        pitchVariation, gainVariation);
}

// デコード済みの音声に対して、再生ボイスと音量・ピッチの設定を確保する
bool SoundManager::InstallVariations(const std::string& key, std::vector<SoundData> variations,
    uint32_t voiceCount, float volume, float minimumInterval, float pitchVariation, float gainVariation)
{
    if (!xAudio2_ || !masterVoice_ || variations.empty() || variations.size() > 4) { return false; }
    Unload(key);
    voiceCount = std::clamp(voiceCount, 1u, 4u);
    if (voiceCount_ + voiceCount > 32u) { return false; }
    Entry entry;
    entry.variations = std::move(variations);
    entry.minimumInterval = (std::max)(minimumInterval, 0.0f);
    entry.volume = std::clamp(volume, 0.0f, 1.0f);
    entry.pitchVariation = std::clamp(pitchVariation, 0.0f, 0.12f);
    entry.gainVariation = std::clamp(gainVariation, 0.0f, 0.20f);
    entry.lastVariation = static_cast<uint32_t>(entry.variations.size() - 1);
    for (uint32_t index = 0; index < voiceCount; ++index) {
        auto*& voice = entry.voices[index];
        if (FAILED(xAudio2_->CreateSourceVoice(&voice, &entry.variations.front().wfex))) {
            DestroyEntry(entry);
            Logger::Log("Audio voice allocation failed: " + key + "\n");
            return false;
        }
        ++voiceCount_;
        voice->SetVolume(entry.volume);
    }
    sounds_.emplace(key, std::move(entry));
    return true;
}

// 指定した音の再生ボイスとデータを解放し、登録を消す
void SoundManager::Unload(const std::string& key)
{
    const auto it = sounds_.find(key);
    if (it != sounds_.end()) {
        DestroyEntry(it->second);
        sounds_.erase(it);
    }
}

// 空いている再生枠を使って効果音を鳴らす　音量と再生ピッチも呼び出し側から指定できる
bool SoundManager::Play(const std::string& key, float gain, float pitchScale)
{
    const auto it = sounds_.find(key);
    if (!kSoundEffectsEnabled || it == sounds_.end()) {
        return false;
    }
    Entry& entry = it->second;
    if (entry.looping) { return false; }
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<float>(now - entry.lastPlay).count() < entry.minimumInterval) {
        return false;
    }
    for (auto* voice : entry.voices) {
        if (!voice) {
            continue;
        }
        XAUDIO2_VOICE_STATE state{};
        voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued != 0) {
            continue;
        }
        const auto random = [&entry]() {
            auto& state = entry.randomState;
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            return state;
        };
        const uint32_t count = static_cast<uint32_t>(entry.variations.size());
        const uint32_t variation = count == 1 ? 0 :
            (entry.lastVariation + 1 + random() % (count - 1)) % count;
        const auto signedRandom = [&random]() {
            return static_cast<float>(random() & 0xFFFFu) / 32767.5f - 1.0f;
        };
        const float pitch = std::clamp(pitchScale, 0.75f, 1.35f) *
            (1.0f + entry.pitchVariation * signedRandom());
        const float level = entry.volume * std::clamp(gain, 0.0f, 1.0f) *
            (1.0f + entry.gainVariation * signedRandom());
        if (FAILED(voice->SetFrequencyRatio(pitch)) || FAILED(voice->SetVolume(level))) { return false; }
        const auto& data = entry.variations[variation];
        XAUDIO2_BUFFER buffer{};
        buffer.pAudioData = data.buffer.data();
        buffer.AudioBytes = static_cast<UINT32>(data.buffer.size());
        buffer.Flags = XAUDIO2_END_OF_STREAM;
        if (FAILED(voice->SubmitSourceBuffer(&buffer)) || FAILED(voice->Start())) {
            voice->Stop();
            voice->FlushSourceBuffers();
            return false;
        }
        entry.lastPlay = now;
        entry.lastVariation = variation;
        ++playCount_;
        return true;
    }
    return false;
}

// BGMなどを繰り返し再生し、同じ音源のループ状態を管理する
bool SoundManager::PlayLoop(const std::string& key)
{
    const auto it = sounds_.find(key);
    if (it == sounds_.end() || !it->second.voices[0] || it->second.voices[1]) { return false; }
    auto& entry = it->second;
    if (entry.looping) { return true; }
    auto* voice = entry.voices[0];
    Stop(key);
    voice->SetFrequencyRatio(1.0f); // BGMは効果音の微変化を適用しない
    voice->SetVolume(entry.volume);
    XAUDIO2_BUFFER buffer{};
    buffer.pAudioData = entry.variations.front().buffer.data();
    buffer.AudioBytes = static_cast<UINT32>(entry.variations.front().buffer.size());
    buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    if (FAILED(voice->SubmitSourceBuffer(&buffer)) || FAILED(voice->Start())) {
        Stop(key);
        return false;
    }
    entry.looping = true;
    return true;
}

// 指定した音の全ボイスを止め、再生待ちデータとループ状態を消す
void SoundManager::Stop(const std::string& key)
{
    const auto it = sounds_.find(key);
    if (it == sounds_.end()) { return; }
    for (auto* voice : it->second.voices) {
        if (voice) { voice->Stop(); voice->FlushSourceBuffers(); }
    }
    it->second.looping = false;
}

// 音量を0～1に収め、同じ音を担当する全ボイスへ反映する
void SoundManager::SetVolume(const std::string& key, float volume)
{
    const auto it = sounds_.find(key);
    if (it == sounds_.end()) { return; }
    it->second.volume = std::clamp(volume, 0.0f, 1.0f);
    for (auto* voice : it->second.voices) {
        if (voice) { voice->SetVolume(it->second.volume); }
    }
}

// ループ用のボイスに再生データが残っている音の数を返す
uint32_t SoundManager::GetLoopCount() const
{
    uint32_t count = 0;
    for (const auto& item : sounds_) {
        if (!item.second.looping || !item.second.voices[0]) { continue; }
        XAUDIO2_VOICE_STATE state{};
        item.second.voices[0]->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (state.BuffersQueued > 0) { ++count; }
    }
    return count;
}

// 音声ファイルをMedia Foundationで読み、48kHz・16ビットのステレオPCMへ変換する
SoundData SoundManager::LoadFile(const std::string& filename)
{
    ComPtr<IMFSourceReader> reader;
    const std::wstring path = StringUtility::ConvertString(filename);
    if (FAILED(MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader))) {
        return {};
    }
    ComPtr<IMFMediaType> type;
    if (FAILED(MFCreateMediaType(&type)) ||
        FAILED(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
        FAILED(type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) ||
        FAILED(type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16)) ||
        FAILED(type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2)) ||
        FAILED(type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 48000)) ||
        FAILED(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, type.Get()))) {
        return {};
    }
    ComPtr<IMFMediaType> outputType;
    if (FAILED(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &outputType))) {
        return {};
    }
    WAVEFORMATEX* format = nullptr;
    if (FAILED(MFCreateWaveFormatExFromMFMediaType(outputType.Get(), &format, nullptr))) {
        return {};
    }
    SoundData sound;
    // 拡張情報を切り落とさないため、対応する通常PCM形式だけを受け付ける
    const bool supported = format->wFormatTag == WAVE_FORMAT_PCM && format->cbSize == 0;
    if (supported) {
        sound.wfex = *format;
    }
    CoTaskMemFree(format);
    if (!supported) {
        return {};
    }
    while (true) {
        ComPtr<IMFSample> sample;
        DWORD flags = 0;
        if (FAILED(reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0,
                nullptr, &flags, nullptr, &sample)) || (flags & MF_SOURCE_READERF_ERROR)) {
            return {};
        }
        if (sample) {
            ComPtr<IMFMediaBuffer> buffer;
            if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) {
                return {};
            }
            BYTE* data = nullptr;
            DWORD length = 0;
            if (FAILED(buffer->Lock(&data, nullptr, &length))) {
                return {};
            }
            // 1曲のPCMは最大32MB　採用するループ素材もオフラインでこの上限を確認する
            const bool fits = sound.buffer.size() + length <= 32u * 1024u * 1024u;
            if (fits && length > 0) {
                sound.buffer.insert(sound.buffer.end(), data, data + length);
            }
            buffer->Unlock();
            if (!fits) {
                return {};
            }
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break;
        }
    }
    return sound;
}
