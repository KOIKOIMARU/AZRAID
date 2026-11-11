#include "app/ui/StageRecords.h"
#include "app/combat/FeverState.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void Require(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}
void Write(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream stream(path, std::ios::binary);
    stream << content;
    Require(static_cast<bool>(stream), "Fixture write failed");
}
}

int main()
{
    try {
        FeverState fever;
        Require(fever.Extend(90) == 0, "Inactive fever was extended");
        fever.Activate();
        Require(fever.Extend(90) == 0, "Fresh fever exceeded its time limit");
        for (int frame = 0; frame < 60; ++frame) { fever.Tick(); }
        Require(fever.Extend((std::numeric_limits<int>::max)()) == 60 &&
            fever.RemainingFrames() == FeverState::kDurationFrames, "Fever extension overflowed or exceeded the cap");
        Require(fever.Extend(-1) == 0 && fever.RemainingFrames() == FeverState::kDurationFrames,
            "Negative extension shortened fever");
        const auto root = std::filesystem::current_path() / L"generated/tests/stage-records" /
            (L"fixtures-" + std::to_wstring(GetCurrentProcessId()));
        std::filesystem::create_directories(root);
        StageRecords records;
        const auto path = root / L"record.txt";
        Require(records.Load(path) && records.Get().clearCount == 0, "First launch must start empty");
        auto result = records.RecordClear(1500, 'B');
        Require(result.saved && result.newBest, "First clear must save a personal best");
        StageRecords reloaded;
        Require(reloaded.Load(path) && reloaded.Get().bestScore == 1500 &&
            reloaded.Get().bestRank == 'B' && reloaded.Get().clearCount == 1, "Record did not survive reload");
        result = reloaded.RecordClear(900, 'A');
        Require(result.saved && !result.newBest && reloaded.Get().bestScore == 1500 &&
            reloaded.Get().bestRank == 'A' && reloaded.Get().clearCount == 2, "Lower score must retain the best; rank can improve independently");
        Require(records.Load(path) && records.Get().clearCount == 2 && records.Get().bestRank == 'A', "Second clear was not persisted");
        result = records.RecordClear(1500, 'B');
        Require(result.saved && !result.newBest && records.Get().bestRank == 'A', "Ties must not show a new score record or lower the rank");
        Require(!records.RecordClear(-1, 'S').saved && !records.RecordClear(2000, 'X').saved &&
            records.Get().clearCount == 3, "Invalid results changed the record");

        Require(StageRecords::EvaluateRank(150000) == 'S' && StageRecords::EvaluateRank(149999) == 'A' &&
            StageRecords::EvaluateRank(80000) == 'A' && StageRecords::EvaluateRank(79999) == 'B' &&
            StageRecords::EvaluateRank(0) == 'B', "Score rank boundaries changed");
        const std::string invalid[] = { "", "AZRAID_RECORD 9\n100 S 1\n", "AZRAID_RECORD 3\n100 S",
            "AZRAID_RECORD 3\n-10 S 1\n", "AZRAID_RECORD 3\n100 X 1\n", "AZRAID_RECORD 3\n100 S 0\n",
            "AZRAID_RECORD 3\n99999999999999999999 S 1\n", "AZRAID_RECORD 3\n100 S 1 trailing" };
        for (size_t index = 0; index < std::size(invalid); ++index) {
            const auto bad = root / (L"invalid-" + std::to_wstring(index) + L".txt");
            Write(bad, invalid[index]);
            Require(!records.Load(bad) && records.Get().clearCount == 0, "Malformed record was accepted");
            Require(records.RecordClear(2000, 'S').saved, "Malformed record could not recover");
            std::ifstream backup(bad.wstring() + L".invalid", std::ios::binary);
            const std::string original{ std::istreambuf_iterator<char>(backup), {} };
            Require(original == invalid[index], "Recovery discarded the previous file");
            Require(reloaded.Load(bad) && reloaded.Get().bestScore == 2000, "Recovery did not persist a valid record");
        }
        const auto blocked = root / L"blocked";
        Write(blocked, "keep this file");
        records.Load(blocked / L"record.txt");
        Require(!records.RecordClear(50, 'B').saved,
            "A write failure was reported as saved");
        Require(std::filesystem::is_regular_file(blocked), "A failed save changed the original path");
        const auto maximum = root / L"maximum.txt";
        Write(maximum, "AZRAID_RECORD 3\n2147483647 S 2147483647\n");
        Require(records.Load(maximum) && records.RecordClear(1, 'B').saved &&
            records.Get().clearCount == (std::numeric_limits<int>::max)(), "Clear count overflowed");
        std::cout << "STAGE_RECORDS_PASS reload=1 independent_bests=1 ties=1 rank_boundaries=1 invalid_files=8 recovery_backup=1 failed_save=1 overflow=1 fever_extension_cap=1\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
