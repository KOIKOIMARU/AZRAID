#include "app/ui/StageResults.h"

#include "app/stage/TutorialProgress.h"

// 今回のリザルトの表示、結果音、戦績の確定状態を初期化する。
void StageResults::Reset()
{
    menu_ = {};
    delayFrames_ = -1;
    soundPlayed_ = false;
    recordCommitted_ = false;
    record_ = {};
}

// リザルトまでの待ち時間を減らす。クリア記録は待ち時間が終わった時点で一度だけ保存する。
void StageResults::Advance(bool saveClearRecord, int score)
{
    if (delayFrames_ > 0) { --delayFrames_; }
    if (!saveClearRecord || IsWaiting() || recordCommitted_) { return; }

    // 描画の回数で保存回数が変わらないよう、更新側で確定する。
    recordCommitted_ = true;
    if (records_) {
        record_ = records_->RecordClear(score, StageRecords::EvaluateRank(score));
    }
}

// 敗北直後はリザルトを待機状態にし、墜落演出が表示開始を指示できるようにする。
void StageResults::BeginDefeat()
{
    // 墜落演出の完了通知が来るまで開かない。
    delayFrames_ = 1;
    Hide();
    ResetSelection();
    soundPlayed_ = false;
}

// 今回のリザルトで最初に呼ばれた場合だけtrueを返し、結果音を一度に限定する。
bool StageResults::TryMarkSoundPlayed()
{
    if (soundPlayed_) { return false; }
    soundPlayed_ = true;
    return true;
}

// 敗北かクリアかで描くリザルトを選ぶ。クリア時には保存済みのベスト記録も表示へ渡す。
void StageResults::Draw(const View& view, StageResultMenu::Stats stats, bool defeat)
{
    if (defeat) {
        menu_.DrawDefeat(view.input, view.min, view.size, stats, view.deltaSeconds, view.menu);
        return;
    }

    if (records_ && recordCommitted_) {
        stats.record = StageResultMenu::Record{ records_->Get().bestScore, record_.newBest, record_.saved };
    }
    const float elapsed = menu_.UpdateClear();
    presentation_.Update(elapsed, view.aspectRatio);
    menu_.DrawClear(view.input, view.min, view.size, stats, elapsed, view.menu);
}

void StageResults::DrawTutorial(const View& view)
{
    menu_.DrawTutorial(view.input, view.min, view.size, TutorialProgress::LessonNames(), view.menu);
}
