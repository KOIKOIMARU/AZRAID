#pragma once

#include "app/presentation/ClearResultPresentation.h"
#include "app/ui/StageRecords.h"
#include "app/ui/StageResultMenu.h"

// 演出が終わってからリザルトを開き、本編のクリア記録を一度だけ確定する
class StageResults {
public:
    struct View {
        Input* input;
        GameplayMenu& menu;
        Math::Vector2 min;
        Math::Vector2 size;
        float deltaSeconds;
        float aspectRatio;
    };

    void SetRecords(StageRecords* records) { records_ = records; }
    void Reset();
    // リザルトの待ち時間を進め、表示可能になった時点でクリア記録を1回だけ確定する
    void Advance(bool saveClearRecord, int score);

    void BeginDefeat();
    void SetDelay(int frames) { delayFrames_ = frames; }
    bool IsWaiting() const { return delayFrames_ > 0; }
    // 今回のリザルトで最初に呼ばれた場合だけtrueを返し、結果音を一度に限定する
    bool TryMarkSoundPlayed();

    void Hide() { menu_.ResetDisplay(); }
    void ResetSelection() { menu_.ResetSelection(); }
    bool HasClearPresentation() const { return menu_.IsClearVisible() && presentation_.IsReady(); }
    ClearResultPresentation& Presentation() { return presentation_; }
    const ClearResultPresentation& Presentation() const { return presentation_; }
    void Draw(const View& view, StageResultMenu::Stats stats, bool defeat);
    void DrawTutorial(const View& view);

#ifdef _DEBUG
    const StageResultMenu& MenuForProbe() const { return menu_; }
    int DelayForProbe() const { return delayFrames_; }
    bool SoundPlayedForProbe() const { return soundPlayed_; }
    void ResetSoundForProbe() { soundPlayed_ = false; }
    const StageRecords* RecordsForProbe() const { return records_; }
    bool RecordCommittedForProbe() const { return recordCommitted_; }
    bool RecordSavedForProbe() const { return record_.saved; }
#endif

private:
    StageRecords* records_ = nullptr;
    StageRecords::ClearResult record_{}; // 今回のクリア記録の結果　保存の成否も含む
    StageResultMenu menu_;
    ClearResultPresentation presentation_;
    int delayFrames_ = -1; // 結果を表示するまでの残りフレーム数
    bool soundPlayed_ = false;
    bool recordCommitted_ = false; // 記録の確定処理を通したかを示し、保存の成否はrecord_.savedで持つ
};
