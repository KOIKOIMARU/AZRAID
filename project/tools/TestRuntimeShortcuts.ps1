[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$output = [IO.Path]::GetFullPath((Join-Path $project '..\generated\shortcut-tests'))
New-Item -ItemType Directory -Path $output -Force | Out-Null

function Get-CppFunction([string]$text, [string]$signature) {
    # 関数の閉じ括弧は行頭にある。隣の関数の名前や並び順には依存しない。
    $pattern = '(?ms)^' + [regex]::Escape($signature) + '[ \t]*\r?\n\{.*?^\}[ \t]*(?=\r?$)'
    $definitionsFound = [regex]::Matches($text, $pattern)
    if ($definitionsFound.Count -ne 1) { throw "Expected one C++ definition for $signature." }
    return $definitionsFound[0].Value
}

# キー判定、メニュー遷移、ショートカットの振り分けには本体の実装を使う。
$runtime = Get-Content -LiteralPath (Join-Path $project 'src\app\runtime\GameRuntime.cpp') -Raw -Encoding utf8
$menu = Get-Content -LiteralPath (Join-Path $project 'src\app\ui\GameplayMenu.cpp') -Raw -Encoding utf8
$menuUi = Get-Content -LiteralPath (Join-Path $project 'src\app\ui\MenuUi.h') -Raw -Encoding utf8
$inputFunctions = @(
    Get-CppFunction $menuUi 'inline bool Pressed(Input* input, BYTE key)'
    Get-CppFunction $menuUi 'inline bool ConfirmPressed(Input* input)'
) -join "`n"
$gameFunctions = @(
    Get-CppFunction $menu 'void GameplayMenu::HandleShortcuts(Input* input, bool canPause)'
    Get-CppFunction $menu 'bool GameplayMenu::Begin(Action action)'
    Get-CppFunction $menu 'float GameplayMenu::TransitionProgress() const'
    Get-CppFunction $menu 'GameplayMenu::Action GameplayMenu::UpdateTransition()'
    Get-CppFunction $runtime 'bool GameRuntime::HandleRuntimeShortcuts()'
) -join "`n"

$stub = @'
#include "app/ui/GameplayMenu.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

using BYTE = unsigned char;
enum : BYTE { DIK_ESCAPE = 1, DIK_H, DIK_RETURN, DIK_SPACE, DIK_R, DIK_LMENU, DIK_RMENU,
    DIK_F1, DIK_F2, DIK_F3, DIK_F4, DIK_F5 };

class Input {
public:
    BYTE pressed = 0;
    BYTE held = 0;
    bool TriggerKey(BYTE key) const { return key == pressed; }
    bool PushKey(BYTE key) const { return key == held; }
};

namespace ImGui {
struct Viewport {
    Math::Vector2 Pos{};
    Math::Vector2 Size{};
};
double time = 0.0;
double GetTime() { return time; }
const Viewport* GetMainViewport()
{
    static const Viewport viewport{};
    return &viewport;
}
}

namespace MenuUi {
bool InputBlocked = false;
void Wipe(Math::Vector2, Math::Vector2, float) {}
}

struct StageResults {
    bool waiting = false;
    int selectionResets = 0;
    bool IsWaiting() const { return waiting; }
    void ResetSelection() { ++selectionResets; }
};

struct GameRuntime {
    Input input;
    Input* input_ = &input;
    GameplayMenu gameplayMenu_;
    StageResults stageResults_;
    bool isGameOver_ = false;
    bool isGameClear_ = false;
    bool isEditorOverlayVisible_ = false;
    bool isPerformanceOverlayVisible_ = false;
    bool isPostEffectBypassEnabled_ = false;
    bool showSkybox_ = true;

    bool HandleRuntimeShortcuts();

    bool Press(BYTE key)
    {
        input.pressed = key;
        return HandleRuntimeShortcuts();
    }
};
'@
$tests = @'
using Action = GameplayMenu::Action;

void Check(bool passed, const char* message)
{
    if (!passed) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

void TestDebugKeys()
{
    constexpr bool debugShortcuts = EXPECT_DEBUG_SHORTCUTS != 0;
    for (const BYTE key : { DIK_F1, DIK_F2, DIK_F3, DIK_F4, DIK_F5 }) {
        GameRuntime game;
        const bool handled = debugShortcuts && key != DIK_F2;
        Check(game.Press(key) == handled, "Wrong debug key handling for this configuration");
        Check(game.isEditorOverlayVisible_ == (handled && key == DIK_F1), "F1 editor toggle");
        Check(game.isPerformanceOverlayVisible_ == (handled && key == DIK_F3), "F3 performance toggle");
        Check(game.isPostEffectBypassEnabled_ == (handled && key == DIK_F4), "F4 post effect toggle");
        Check(game.showSkybox_ == !(handled && key == DIK_F5), "F5 skybox toggle");
        Check(!game.gameplayMenu_.IsPaused(), "Debug key changed the pause menu");

        Check(game.Press(key) == handled, "Second debug key press handled differently");
        Check(!game.isEditorOverlayVisible_ && !game.isPerformanceOverlayVisible_ &&
            !game.isPostEffectBypassEnabled_ && game.showSkybox_, "Debug toggles did not return to their initial state");
    }
}

void CompleteTransition(GameRuntime& game, Action action, const char* message)
{
    Check(game.gameplayMenu_.PendingAction() == action, message);
    Check(game.gameplayMenu_.UpdateTransition() == Action::None, "Transition finished without advancing time");
    ImGui::time += 1.0;
    Check(game.gameplayMenu_.UpdateTransition() == action, message);
}

void TestPauseAndControls()
{
    GameRuntime game;
    Check(!game.Press(DIK_ESCAPE) && game.gameplayMenu_.IsPaused(), "Escape did not pause");
    game.Press(DIK_ESCAPE);
    CompleteTransition(game, Action::Resume, "Escape did not resume");
    Check(!game.gameplayMenu_.IsPaused(), "Resume left the game paused");

    game.Press(DIK_H);
    CompleteTransition(game, Action::Controls, "H did not open controls");
    Check(game.gameplayMenu_.IsControlsVisible() && game.gameplayMenu_.IsPaused(), "Controls did not pause the game");
    game.input.held = DIK_LMENU;
    game.Press(DIK_RETURN);
    Check(game.gameplayMenu_.PendingAction() == Action::None, "Alt+Enter closed the controls");
    game.input.held = 0;
    game.Press(DIK_RETURN);
    CompleteTransition(game, Action::Back, "Enter did not close controls");
    Check(!game.gameplayMenu_.IsControlsVisible() && game.gameplayMenu_.IsPaused(), "Closing controls did not return to pause");

    game.Press(DIK_H);
    CompleteTransition(game, Action::Controls, "H did not reopen controls");
    game.Press(DIK_SPACE);
    CompleteTransition(game, Action::Back, "Space did not close controls");
    Check(!game.gameplayMenu_.IsControlsVisible(), "Controls remained visible");
}

void TestRetry()
{
    GameRuntime playing;
    Check(!playing.Press(DIK_R) && playing.gameplayMenu_.PendingAction() == Action::None,
        "R started a retry during normal gameplay");
    for (const bool defeat : { true, false }) {
        GameRuntime game;
        game.isGameOver_ = defeat;
        game.isGameClear_ = !defeat;
        game.stageResults_.waiting = true;
        Check(!game.Press(DIK_R) && game.gameplayMenu_.PendingAction() == Action::None,
            "R started a retry before the result was ready");
        game.stageResults_.waiting = false;
        game.Press(DIK_ESCAPE);
        game.Press(DIK_H);
        Check(!game.gameplayMenu_.IsPaused(), "Pause or controls opened on the result");
        Check(!game.Press(DIK_R) && game.gameplayMenu_.PendingAction() == Action::Retry,
            "R did not schedule a retry on the result");
        game.Press(DIK_R);
        Check(game.stageResults_.selectionResets == (defeat ? 1 : 0), "Wrong result selection reset count");
        CompleteTransition(game, Action::Retry, "Retry transition did not finish");
        Check(game.gameplayMenu_.SceneAction() == Action::Retry, "Retry was not handed off to the scene");
    }
}

int main()
{
    TestDebugKeys();
    TestPauseAndControls();
    TestRetry();
}
'@
$source = Join-Path $output 'RuntimeShortcuts.cpp'
$inputCode = "namespace MenuUi {`n$inputFunctions`n}"
[IO.File]::WriteAllText($source, (@($stub, $inputCode, $gameFunctions, $tests) -join "`n"), [Text.UTF8Encoding]::new($false))
$vc = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231'
$sdk = 'C:\Program Files (x86)\Windows Kits\10'
[xml]$configuration = Get-Content -LiteralPath (Join-Path $project 'CG2.vcxproj') -Raw
$sdkVersion = ($configuration.Project.PropertyGroup | Where-Object Label -eq 'Globals').WindowsTargetPlatformVersion
$sdkLibraries = Join-Path $sdk "Lib\$sdkVersion"
$ucrtInclude = Join-Path $sdk "Include\$sdkVersion\ucrt"
$include = Join-Path $project 'src'
foreach ($name in @('Debug', 'Release', 'Development')) {
    $group = $configuration.Project.ItemDefinitionGroup | Where-Object { $_.Condition -like "*'$name|x64'*" }
    $definitions = @($group.ClCompile.PreprocessorDefinitions.Split(';') | Where-Object { $_ -and $_ -notlike '%*' } | ForEach-Object { "/D$_" })
    $expectDebugShortcuts = [int]($name -ne 'Release')
    $runtimeLibrary = if ($name -eq 'Debug') { '/MTd' } else { '/MT' }
    $exe = Join-Path $output "$name.exe"
    & (Join-Path $vc 'bin\Hostx64\x64\cl.exe') /nologo /W4 /WX /EHsc $runtimeLibrary /std:c++20 /utf-8 $definitions `
        "/DEXPECT_DEBUG_SHORTCUTS=$expectDebugShortcuts" "/I$include" "/I$vc\include" "/I$ucrtInclude" `
        $source "/Fo$output\$name.obj" "/Fe$exe" /link "/LIBPATH:$vc\lib\x64" `
        "/LIBPATH:$sdkLibraries\ucrt\x64" "/LIBPATH:$sdkLibraries\um\x64"
    if ($LASTEXITCODE -ne 0) { throw "$name shortcut test compilation failed." }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "$name shortcut regression failed: $LASTEXITCODE" }
    Write-Output "SHORTCUT_TEST_PASS configuration=$name debug_shortcuts=$expectDebugShortcuts f2_ignored=1 pause=1 controls=1 retry=1"
}
