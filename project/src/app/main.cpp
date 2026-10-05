#include <Windows.h>
#include <shellapi.h>

#include <cerrno>
#include <cmath>
#include <cwchar>
#include <exception>
#include <memory>

#include "app/MyGame.h"
#include "engine/base/Logger.h"

namespace {

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

SmokeTestOptions ParseSmokeTestOptions()
{
    SmokeTestOptions options;

    int argumentCount = 0;
    wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (!arguments) {
        return options;
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
    return options;
}

}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    try {
        std::unique_ptr<Framework> game =
            std::make_unique<MyGame>(ParseSmokeTestOptions());
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
