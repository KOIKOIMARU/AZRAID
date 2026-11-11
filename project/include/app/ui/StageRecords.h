#pragma once

#include <filesystem>

// 現在の1ステージのクリア記録。シーンや描画から独立して、再起動後も引き継ぐ。
class StageRecords {
public:
    struct Record {
        int bestScore = 0;
        char bestRank = '-';
        int clearCount = 0;
    };
    struct ClearResult {
        bool newBest = false;
        bool saved = false;
    };

    static constexpr int kSScore = 150000;
    static constexpr int kAScore = 80000;
    static constexpr int kRulesVersion = 3;

    static std::filesystem::path DefaultPath();
    static char EvaluateRank(int score);

    // 不正なデータは読み込まない。次の保存時にも、元ファイルを退避して残す。
    bool Load(const std::filesystem::path& path);
    ClearResult RecordClear(int score, char rank);
    const Record& Get() const { return record_; }

private:
    bool Save(const Record& record);
    Record record_{};
    std::filesystem::path path_;
    bool invalidFile_ = false;
};
