#include "app/ui/StageRecords.h"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <string>

namespace {
// ランクを比較用の数値に変換し、最高ランクを判定できるようにする。
int RankValue(char rank)
{
    switch (rank) {
    case 'S': return 3;
    case 'A': return 2;
    case 'B': return 1;
    default: return 0;
    }
}
}

// ユーザーごとの保存先から戦績ファイルのパスを作る。保存先を取得できないときは代替パスを使う。
std::filesystem::path StageRecords::DefaultPath()
{
    std::array<wchar_t, 32768> local{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local.data(), static_cast<DWORD>(local.size()));
    if (length > 0 && length < local.size()) {
        return std::filesystem::path(local.data()) / L"AZRAID" / L"record-v3.txt";
    }
    // 環境変数がない場合も、実行場所に保存先を用意する。
    return std::filesystem::current_path() / L"save" / L"record-v3.txt";
}

// 得点をS・Aの基準と比較し、今回のクリアランクを返す。
char StageRecords::EvaluateRank(int score)
{
    if (score >= kSScore) { return 'S'; }
    if (score >= kAScore) { return 'A'; }
    return 'B';
}

// 保存データを読み、値の範囲や形式が正しいと確認してから現在の記録へ取り込む。
bool StageRecords::Load(const std::filesystem::path& path)
{
    path_ = path;
    record_ = {};
    invalidFile_ = false;
    std::error_code error;
    if (!std::filesystem::exists(path_, error)) { return !error; }

    std::ifstream stream(path_);
    std::string magic;
    int version = 0;
    Record loaded;
    const bool parsed = static_cast<bool>(stream >> magic >> version >> loaded.bestScore >> loaded.bestRank >> loaded.clearCount);
    if (parsed) { stream >> std::ws; }
    if (!parsed || !stream.eof() || magic != "AZRAID_RECORD" || version != kRulesVersion ||
        loaded.bestScore < 0 || loaded.clearCount < 1 || RankValue(loaded.bestRank) == 0) {
        invalidFile_ = true;
        return false;
    }
    record_ = loaded;
    return true;
}

// 今回の得点とランクを最高記録と比較して保存する。更新の有無と保存結果を呼び出し元に返す。
StageRecords::ClearResult StageRecords::RecordClear(int score, char rank)
{
    if (score < 0 || RankValue(rank) == 0) { return {}; }
    Record next = record_;
    const bool newBest = record_.clearCount == 0 || score > record_.bestScore;
    next.bestScore = (std::max)(next.bestScore, score);
    if (RankValue(rank) > RankValue(next.bestRank)) { next.bestRank = rank; }
    if (next.clearCount < (std::numeric_limits<int>::max)()) { ++next.clearCount; }
    const bool saved = Save(next);
    // 保存先へ書き込めない場合も、その起動中の記録は保持する。
    record_ = next;
    return { newBest, saved };
}

// 一時ファイルへ書き出してから置き換え、保存途中に失敗した場合に備える。
bool StageRecords::Save(const Record& record)
{
    if (path_.empty()) { return false; }
    std::error_code error;
    if (!path_.parent_path().empty()) {
        std::filesystem::create_directories(path_.parent_path(), error);
        if (error) { return false; }
    }
    if (invalidFile_) {
        auto backup = path_;
        backup += L".invalid";
        // 以前の退避データがある場合は上書きしない。
        if (!std::filesystem::copy_file(path_, backup, std::filesystem::copy_options::none, error)) { return false; }
        invalidFile_ = false;
    }
    auto temporary = path_;
    temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    {
        std::ofstream stream(temporary, std::ios::out | std::ios::trunc);
        stream << "AZRAID_RECORD " << kRulesVersion << '\n' << record.bestScore << ' ' << record.bestRank << ' ' << record.clearCount << '\n';
        stream.flush();
        if (!stream) {
            stream.close();
            std::filesystem::remove(temporary, error);
            return false;
        }
    }
    // 完成したファイルを置き換える。途中で書込みが失敗しても、元の記録は壊さない。
    if (!MoveFileExW(temporary.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}
