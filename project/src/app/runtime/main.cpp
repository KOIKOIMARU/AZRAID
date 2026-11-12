// ===== main.cpp =====
// Windows側の起動入口　起動オプションを読み取り、ゲームループと終了コードを管理する

#include <Windows.h>
#include <shellapi.h>

#include <array>
#include <cerrno>
#include <cmath>
#include <cwchar>
#include <exception>
#include <filesystem>
#include <memory>

#include "app/runtime/MyGame.h"
#include "engine/base/Logger.h"

namespace {

// 起動場所によらず、実行ファイルに対応した素材を読み込めるよう作業ディレクトリを設定する
void ConfigureRuntimeDirectory()
{
    std::array<wchar_t, 32768> executablePath{};
    const DWORD length = GetModuleFileNameW(nullptr, executablePath.data(),
        static_cast<DWORD>(executablePath.size()));
    if (length == 0 || length >= executablePath.size()) { return; }

    const auto directory = std::filesystem::path(executablePath.data()).parent_path();
    std::error_code error;
    const auto hasGameData = [&error](const std::filesystem::path& path) {
        return std::filesystem::is_directory(path / L"resources", error) &&
            std::filesystem::is_directory(path / L"shaders", error);
    };
    // 配布版はexeの隣のデータを使い、Visual Studioの作業場所は維持する
    if (hasGameData(directory)) {
        std::filesystem::current_path(directory);
    } else if (!hasGameData(std::filesystem::current_path())) {
        // generated/outputs/Debug・Release内のexeも、ダブルクリックで起動できる
        const auto project = directory.parent_path().parent_path().parent_path() / L"project";
        if (hasGameData(project)) { std::filesystem::current_path(project); }
    }
}

// 起動引数を正の数として読み取り、変換できない値を拒否する
bool TryParsePositiveNumber(const wchar_t* text, double& value)
{
    if (!text || *text == L'\0') {
        return false;
    }

    errno = 0;
    wchar_t* parseEnd = nullptr;
    const double parsedValue = std::wcstod(text, &parseEnd);
    if (errno != 0 || parseEnd == text || *parseEnd != L'\0' ||
        !std::isfinite(parsedValue) || parsedValue <= 0.0) {
        return false;
    }

    value = parsedValue;
    return true;
}

// 起動引数から自動確認、開始モード、ログ出力先などの設定を読み取る
GameLaunchOptions ParseLaunchOptions()
{
    GameLaunchOptions launch;
    auto& options = launch.smokeTest;

    int argumentCount = 0;
    wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (!arguments) {
        return launch;
    }

    for (int index = 1; index < argumentCount; ++index) {
        const std::wstring_view argument(arguments[index]);
        if (argument == L"--smoke-test") {
            options.enabled = true;
            if (index + 1 < argumentCount) {
                double seconds = 0.0;
                if (TryParsePositiveNumber(arguments[index + 1], seconds)) {
                    options.gameplaySeconds = seconds;
                    ++index;
                }
            }
        } else if (argument == L"--smoke-tutorial") {
            options.tutorial = true;
#ifdef _DEBUG
        } else if (argument == L"--debug-direct-start") {
            launch.debugDirectStart = true;
        } else if (argument == L"--smoke-tutorial-departure") {
            options.tutorial = options.playthrough = options.tutorialDeparture = true;
        } else if (argument == L"--smoke-tutorial-preview") {
            options.tutorial = true;
            options.playthrough = true;
            options.tutorialPreview = true;
        } else if (argument == L"--smoke-playthrough") {
            options.playthrough = true;
        } else if (argument == L"--smoke-phantom") {
            options.phantom = true;
        } else if (argument == L"--smoke-phantom-preview") {
            options.phantom = true;
            options.phantomPreview = true;
        } else if (argument == L"--smoke-charge") {
            options.chargeShot = true;
        } else if (argument == L"--smoke-charge-preview") {
            options.chargeShot = true;
            options.chargePreview = true;
        } else if (argument == L"--smoke-rift") {
            options.rift = true;
        } else if (argument == L"--smoke-rift-preview") {
            options.rift = true;
            options.riftPreview = true;
        } else if (argument == L"--smoke-boss") {
            options.boss = true;
        } else if (argument == L"--smoke-boss-preview") {
            options.boss = true;
            options.bossPreview = true;
        } else if (argument == L"--smoke-defeat") {
            options.defeat = true;
        } else if (argument == L"--smoke-defeat-preview") {
            options.defeat = true;
            options.defeatPreview = true;
#endif
        } else if (argument == L"--smoke-timeout" &&
                   index + 1 < argumentCount) {
            double seconds = 0.0;
            if (TryParsePositiveNumber(arguments[index + 1], seconds)) {
                options.startupTimeoutSeconds = seconds;
                ++index;
            }
        } else if (argument == L"--smoke-log" &&
                   index + 1 < argumentCount) {
            options.logPath = arguments[++index];
        }
    }

    LocalFree(arguments);
    return launch;
}

}

// Windowsの起動入口　引数を読み、ゲームを実行して終了コードを返す
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    try {
        ConfigureRuntimeDirectory();
        std::unique_ptr<Framework> game =
            std::make_unique<MyGame>(ParseLaunchOptions());
        return game->Run();
    } catch (const std::exception& exception) {
        Logger::Log(std::string("Fatal application error: ") +
                    exception.what() + "\n");
        return 10;
    } catch (...) {
        Logger::Log("Fatal application error: unknown exception\n");
        return 11;
    }
}
