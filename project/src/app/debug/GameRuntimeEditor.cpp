#include "app/runtime/GameRuntime.h"

#ifdef ENABLE_DEBUG_GUI
#include "app/runtime/GameRuntimeTuning.h"
#include "app/stage/SceneryAssets.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/Logger.h"
#include "engine/3d/Model.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <IconsFontAwesome6.h>
#include <ImGuiFileDialog.h>
#include <implot.h>
#include <imgui_node_editor.h>
#include <TextEditor.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace {
using namespace GameRuntimeTuning;
using namespace SceneryAssets;
}

// 開発用に指定された開始地点を読み取り、準備完了後に該当する区間へ移す。
void GameRuntime::ApplyDebugStartPhase()
{
    char* debugStartPhase = nullptr;
    size_t debugStartPhaseLength = 0;
    if (_dupenv_s(
            &debugStartPhase,
            &debugStartPhaseLength,
            "CG2_DEBUG_START_PHASE") == 0 &&
        debugStartPhase) {
        const std::string_view startPhase(debugStartPhase);
        if (startPhase == "Wave2") {
            DebugJumpToStagePhase(1);
        } else if (startPhase == "Wave3") {
            DebugJumpToStagePhase(2);
        } else if (startPhase == "Boss") {
            DebugJumpToStagePhase(3);
        } else if (startPhase == "SniperWing") {
            DebugJumpToStagePhase(4);
        }
    }
    std::free(debugStartPhase);
}

// 開発中に指定地点へ移る。敵・弾・演出の残りを整理して、その地点の確認を始められるようにする。
void GameRuntime::DebugJumpToStagePhase(int phaseIndex)
{
    if (IsTutorial()) {
        editorStatusMessage_ = "本編のフェーズ移動はチュートリアルでは使用できません。";
        return;
    }
    if (isGameOver_ || isGameClear_) {
        editorStatusMessage_ = "フェーズ移動はゲーム進行中のみ使用できます。";
        return;
    }

    playerControl_.Aim().ClearTarget();
    projectiles_.RecycleAll();
    ResetPhantomRaid();
    stageEnemies_.Reset(EnemyTargetReferences());
    ResetRifts();

    const auto phase = stageFlight_.SeekPhaseForDebug(phaseIndex);
    const float targetProgress = stageFlight_.Schedule().Progress();
    // 明示的なデバッグ移動だけは、対象街区の中から始める。通常の遷移は前方から行う。
    railScenery_.SetDistricts(IsTutorial(),
        targetProgress >= 104.0f ? stageFlight_.Distance() - 600.0f : kDefenseStartWorldZ,
        targetProgress >= 230.0f ? stageFlight_.Distance() - 400.0f : kPlazaStartWorldZ);
    combatResolver_.Reset(phase.skippedEnemies);
    playerControl_.ResetShotCountForDebug();
    rewards_.Fever().SetActivationCountForDebug(0);
    rewards_.ResetJustDodgesForDebug();
    combatProgress_.ResetChain();
    gameplayElapsedSeconds_ = 0.0f;
    enemyAttackPatterns_.Reset();
    bossPresentation_.Reset();
    if (phase.startsAtBoss) { bossPresentation_.TryBeginWarning(); }
    bossAttack_.Reset();
    bossShotsFired_ = 0;
    stageResults_.SetDelay(-1);

    PrepareStageFrame();
    editorStatusMessage_ = std::string("デバッグ移動: ") + phase.name;
}

// 編集中のオブジェクトをシーンファイルへ保存し、結果を開発画面へ知らせる。
bool GameRuntime::SaveSceneObjects(const char* path)
{
    if (!path || path[0] == '\0') {
        editorStatusMessage_ = "保存失敗: パスが無効です。";
        return false;
    }
    SceneSerializer::SceneSettings settings{};
    if (camera_) {
        settings.hasCamera = true;
        settings.cameraTranslate = camera_->GetTranslate();
        settings.cameraRotate = camera_->GetRotate();
    }
    if (!sceneObjects_.Save(path, settings)) {
        editorStatusMessage_ = "保存失敗: " + std::string(path);
        return false;
    }
    editorStatusMessage_ = "保存しました: " + sceneObjects_.Path();
    return true;
}

// 配置の編集、シーン移動、戦闘の調整を行う開発用UIを描き、操作を各担当へ渡す。
void GameRuntime::DrawEditorOverlay()
{
    if (!isEditorOverlayVisible_) {
        hasEditorOverlayViewportRect_ = false;
        return;
    }

    hasEditorOverlayViewportRect_ = false;

    const char* kWindowHierarchy = "ヒエラルキー";
    const char* kWindowInspector = "インスペクター";
    const char* kWindowGameView = "ゲームビュー";
    const char* kWindowProject = "プロジェクト";
    const char* kWindowConsole = "コンソール";
    const char* kWindowStats = "統計";
    const char* kWindowTuning = "ゲーム調整";
    const char* kWindowNodeGraph = "ノードグラフ";
    const char* kWindowTextView = "テキスト表示";

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiID dockspaceId = ImGui::GetID("GameOverlayDockSpace");
    const ImGuiDockNodeFlags dockspaceFlags =
        ImGuiDockNodeFlags_NoWindowMenuButton |
        ImGuiDockNodeFlags_NoCloseButton;
    bool requestDockLayoutReset = false;
    static bool showAdvancedDebugPanels = false;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("\xE3\x83\x95\xE3\x82\xA1\xE3\x82\xA4\xE3\x83\xAB")) {
            if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " \xE8\xAA\xAD\xE3\x81\xBF\xE8\xBE\xBC\xE3\x81\xBF", "Ctrl+O")) {
                IGFD::FileDialogConfig config{};
                config.path = "resources";
                ImGuiFileDialog::Instance()->OpenDialog(
                    "GameSceneOpenDialog",
                    "シーンを開く",
                    ".json",
                    config);
            }
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " \xE4\xBF\x9D\xE5\xAD\x98", "Ctrl+S")) {
                SaveSceneObjects(sceneObjects_.Path().c_str());
            }
            if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " 名前を付けて保存")) {
                IGFD::FileDialogConfig config{};
                config.path = "resources";
                config.fileName = "game_scene.json";
                ImGuiFileDialog::Instance()->OpenDialog(
                    "GameSceneSaveDialog",
                    "名前を付けてシーンを保存",
                    ".json",
                    config);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("\xE3\x82\xBF\xE3\x82\xA4\xE3\x83\x88\xE3\x83\xAB\xE3\x81\xB8\xE6\x88\xBB\xE3\x82\x8B", "F2")) {
                editorExitRequested_ = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("\xE7\xB7\xA8\xE9\x9B\x86")) {
            ImGui::MenuItem(ICON_FA_ROTATE_LEFT " \xE5\x85\x83\xE3\x81\xAB\xE6\x88\xBB\xE3\x81\x99", "Ctrl+Z", false, false);
            ImGui::MenuItem(ICON_FA_ROTATE_RIGHT " \xE3\x82\x84\xE3\x82\x8A\xE7\x9B\xB4\xE3\x81\x97", "Ctrl+Y", false, false);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE3\x82\xAA\xE3\x83\x96\xE3\x82\xB8\xE3\x82\xA7\xE3\x82\xAF\xE3\x83\x88")) {
            ImGui::MenuItem(ICON_FA_USER " プレイヤー", nullptr, false, false);
            ImGui::MenuItem(ICON_FA_BULLSEYE " 敵", nullptr, false, false);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("\xE3\x82\xA6\xE3\x82\xA3\xE3\x83\xB3\xE3\x83\x89\xE3\x82\xA6")) {
            if (ImGui::MenuItem(ICON_FA_TABLE_COLUMNS " \xE5\x88\x9D\xE6\x9C\x9F\xE3\x83\xAC\xE3\x82\xA4\xE3\x82\xA2\xE3\x82\xA6\xE3\x83\x88\xE3\x81\xAB\xE6\x88\xBB\xE3\x81\x99")) {
                requestDockLayoutReset = true;
            }
            ImGui::MenuItem(
                ICON_FA_CODE_BRANCH " 詳細デバッグパネル",
                nullptr,
                &showAdvancedDebugPanels);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("\xE5\x86\x8D\xE7\x94\x9F")) {
            ImGui::MenuItem(ICON_FA_PLAY " \xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE8\xA1\xA8\xE7\xA4\xBA\xE4\xB8\xAD", nullptr, true, false);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::TextDisabled("F1 GUI表示切り替え / F2 タイトルへ戻る");
        ImGui::EndMainMenuBar();
    }

    ImGui::DockSpaceOverViewport(dockspaceId, viewport, dockspaceFlags);

    static bool hasBuiltDefaultDockLayout = false;
    if (!hasBuiltDefaultDockLayout || requestDockLayoutReset) {
        hasBuiltDefaultDockLayout = true;
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, dockspaceFlags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

        ImGuiID mainId = dockspaceId;
        ImGuiID leftId = 0;
        ImGuiID rightId = 0;
        ImGuiID rightBottomId = 0;
        ImGuiID bottomId = 0;
        ImGuiID centerId = 0;
        leftId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, 0.22f, nullptr, &mainId);
        rightId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Right, 0.27f, nullptr, &mainId);
        bottomId = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, 0.24f, nullptr, &centerId);
        rightBottomId = ImGui::DockBuilderSplitNode(rightId, ImGuiDir_Down, 0.48f, nullptr, &rightId);

        ImGui::DockBuilderDockWindow(kWindowHierarchy, leftId);
        ImGui::DockBuilderDockWindow(kWindowInspector, rightId);
        ImGui::DockBuilderDockWindow(kWindowProject, bottomId);
        ImGui::DockBuilderDockWindow(kWindowConsole, bottomId);
        ImGui::DockBuilderDockWindow(kWindowTuning, rightBottomId);
        ImGui::DockBuilderDockWindow(kWindowStats, rightBottomId);
        ImGui::DockBuilderDockWindow(kWindowNodeGraph, bottomId);
        ImGui::DockBuilderDockWindow(kWindowTextView, bottomId);
        ImGui::DockBuilderDockWindow(kWindowGameView, centerId);
        ImGui::DockBuilderFinish(dockspaceId);
    }

    const ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin(kWindowHierarchy, nullptr, panelFlags)) {
        ImGui::TextUnformatted("検索");
        ImGui::SameLine();
        ImGui::Button(ICON_FA_PLUS, ImVec2(32.0f, 0.0f));
        ImGui::SameLine();
        ImGui::Button(ICON_FA_MINUS, ImVec2(32.0f, 0.0f));
        ImGui::Separator();
        ImGui::Selectable(ICON_FA_USER " プレイヤー", true);
        ImGui::Selectable((std::string(ICON_FA_BULLSEYE " 敵 x ") + std::to_string(stageEnemies_.Active().size())).c_str(), false);
        ImGui::Selectable((std::string(ICON_FA_CIRCLE " 自弾 x ") + std::to_string(projectiles_.PlayerPool().Active().size())).c_str(), false);
        ImGui::Selectable((std::string(ICON_FA_CIRCLE_DOT " 敵弾 x ") + std::to_string(projectiles_.EnemyPool().Active().size())).c_str(), false);
        ImGui::Selectable((std::string(ICON_FA_CUBE " シーンオブジェクト x ") + std::to_string(sceneObjects_.Count())).c_str(), false);
        ImGui::Separator();
        ImGui::Text("ウェーブ: %d / %d", (std::min)(stageFlight_.WaveIndex() + 1, kWaveCount), kWaveCount);
        ImGui::Text(
            "出現数: %d / %d",
            stageFlight_.Schedule().EmittedEnemyCount(), stageFlight_.Schedule().EnemyCount());
    }
    ImGui::End();

    if (ImGui::Begin(kWindowInspector, nullptr, panelFlags)) {
        if (ImGui::CollapsingHeader(ICON_FA_PALETTE " 描画", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox(ICON_FA_EYE " スカイボックス", &showSkybox_);
            const char* postEffectItems[] = {
                "なし",
                "グレースケール",
                "ビネット",
                "ボックス 3x3",
                "ボックス 5x5",
                "ガウシアン",
                "輝度アウトライン",
                "深度アウトライン",
                "ラジアルブラー",
                "ディゾルブ",
                "ランダム",
                "ビネット + スムージング",
                "ゲームトーン 自動",
                "ゲームトーン 低HP",
                "ゲームトーン クリア",
                "ゲームトーン ゲームオーバー",
                "ブルーム"
            };
            ImGui::Combo("ポストエフェクト", &postEffectMode_, postEffectItems, IM_ARRAYSIZE(postEffectItems));
        }
        if (ImGui::CollapsingHeader(ICON_FA_USER " プレイヤー", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("HP: %d / %d", GetPlayerHp(), GetPlayerMaxHp());
            ImGui::Text("スコア: %d", combatProgress_.Score());
            ImGui::Text("チャージ: %s", playerControl_.Shooting().IsChargeReady() ? "完了" : "蓄積中");
            ImGui::Text("チャージ補助: %s", playerControl_.Aim().Target() ? "ON" : "OFF");
        }
        if (ImGui::CollapsingHeader(ICON_FA_LOCATION_DOT " トランスフォーム", ImGuiTreeNodeFlags_DefaultOpen)) {
            const Math::Vector3 playerPosition = player_ ? player_->GetTranslate() : Math::Vector3{};
            ImGui::Text("位置 X: %.2f", playerPosition.x);
            ImGui::Text("位置 Y: %.2f", playerPosition.y);
            ImGui::Text("位置 Z: %.2f", playerPosition.z);
            ImGui::Text("レール距離: %.1f", stageFlight_.Distance());
        }
        if (ImGui::CollapsingHeader(ICON_FA_GAMEPAD " ゲーム進行", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("撃破数: %d / %d", combatResolver_.Stats().defeated, GetTotalEnemyTargetCount());
            ImGui::Text("敵数: %zu", stageEnemies_.Active().size());
            ImGui::Text("自弾: %zu", projectiles_.PlayerPool().Active().size());
            ImGui::Text("敵弾: %zu", projectiles_.EnemyPool().Active().size());
        }
    }
    ImGui::End();

    if (ImGui::Begin(kWindowGameView, nullptr, panelFlags)) {
        const ImVec2 contentMin = ImGui::GetCursorScreenPos();
        const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        constexpr float kGameViewAspect = 16.0f / 9.0f;
        ImVec2 imageSize(
            (std::max)(contentAvail.x, 1.0f),
            (std::max)(contentAvail.y, 1.0f));
        if (imageSize.x / imageSize.y > kGameViewAspect) {
            imageSize.x = imageSize.y * kGameViewAspect;
        } else {
            imageSize.y = imageSize.x / kGameViewAspect;
        }
        const ImVec2 imageOffset(
            (std::max)(contentAvail.x - imageSize.x, 0.0f) * 0.5f,
            (std::max)(contentAvail.y - imageSize.y, 0.0f) * 0.5f);
        ImGui::SetCursorScreenPos(
            ImVec2(contentMin.x + imageOffset.x, contentMin.y + imageOffset.y));
        if (dxCommon_) {
            ImGui::Image(
                static_cast<ImTextureID>(dxCommon_->GetRenderTextureGpuDescriptorHandle().ptr),
                imageSize);
            editorOverlayViewportMin_ = {
                contentMin.x + imageOffset.x,
                contentMin.y + imageOffset.y
            };
            editorOverlayViewportSize_ = { imageSize.x, imageSize.y };
            hasEditorOverlayViewportRect_ = imageSize.x > 1.0f && imageSize.y > 1.0f;
        }
    }
    ImGui::End();

    if (ImGui::Begin(kWindowProject, nullptr, panelFlags)) {
        if (ImGui::Button(ICON_FA_FOLDER_OPEN " 開く")) {
            IGFD::FileDialogConfig config{};
            config.path = "resources";
            ImGuiFileDialog::Instance()->OpenDialog(
                "ProjectAssetDialog",
                "アセットを開く",
                ".json,.png,.gltf,.obj,.dds",
                config);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("ImGuiFileDialog");
        ImGui::Text("現在のシーン: %s", sceneObjects_.Path().c_str());
        ImGui::TextDisabled("%s", editorStatusMessage_.c_str());
        ImGui::Separator();
        ImGui::Columns(4, "GameOverlayAssetsRich", false);
        ImGui::TextUnformatted("シーン");
        ImGui::NextColumn();
        ImGui::TextUnformatted("プレハブ");
        ImGui::NextColumn();
        ImGui::TextUnformatted("モデル");
        ImGui::NextColumn();
        ImGui::TextUnformatted("テクスチャ");
        ImGui::NextColumn();
        ImGui::Separator();
        ImGui::TextUnformatted("resources/game_scene.json");
        ImGui::NextColumn();
        ImGui::TextUnformatted("resources/prefab_00.json");
        ImGui::NextColumn();
        ImGui::TextUnformatted("game_player");
        ImGui::NextColumn();
        ImGui::TextUnformatted("resources/checkerBoard.png");
        ImGui::NextColumn();
        ImGui::TextUnformatted("resources/scene_01.json");
        ImGui::NextColumn();
        ImGui::TextUnformatted("resources/prefab_01.json");
        ImGui::NextColumn();
        ImGui::TextUnformatted("primitive_sphere");
        ImGui::NextColumn();
        ImGui::TextUnformatted("resources/uvChecker.png");
        ImGui::Columns(1);
    }
    ImGui::End();

    if (ImGui::Begin(kWindowStats, nullptr, panelFlags)) {
        static float historyTime[120]{};
        static float enemyHistory[120]{};
        static float bulletHistory[120]{};
        static float frameMsHistory[120]{};
        static int historyOffset = 0;
        const ImGuiIO& io = ImGui::GetIO();
        const float frameMs = io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f;
        const size_t hitVisualCount = combatEffects_.ActiveVisualCount();
        const size_t activePlayerBulletCount = projectiles_.PlayerPool().Active().size();
        const size_t activeEnemyBulletCount = projectiles_.EnemyPool().Active().size();
        const size_t activeBulletCount = activePlayerBulletCount + activeEnemyBulletCount;
        const size_t activeBulletDrawEstimate =
            activePlayerBulletCount * 3u +
            activeEnemyBulletCount * 2u;
        const size_t pooledBulletCount = projectiles_.PlayerPool().AvailableCount() + projectiles_.EnemyPool().AvailableCount();
        const size_t activeSceneryCount = railScenery_.VisibleCount();
        const size_t totalSceneryCount = railScenery_.Objects().size();
        const size_t activeDepthCueCount = flightSpeedEffects_.WorldCueCount();
        const size_t estimatedActiveDrawObjects =
            sceneObjects_.Count() +
            activeSceneryCount +
            activeDepthCueCount +
            (player_ ? 1u : 0u) +
            stageEnemies_.Active().size() +
            activeBulletDrawEstimate +
            hitVisualCount;
        const size_t estimatedPooledBulletObjects = pooledBulletCount * 5u;
        historyTime[historyOffset] = static_cast<float>(historyOffset);
        enemyHistory[historyOffset] = static_cast<float>(stageEnemies_.Active().size());
        bulletHistory[historyOffset] =
            static_cast<float>(projectiles_.PlayerPool().Active().size() + projectiles_.EnemyPool().Active().size());
        frameMsHistory[historyOffset] = frameMs;
        historyOffset = (historyOffset + 1) % 120;

        ImGui::Text("Perf: %.1f FPS / %.2f ms", io.Framerate, frameMs);
        ImGui::Text(
            "Objects: draw est %zu / pooled bullet objects %zu",
            estimatedActiveDrawObjects,
            estimatedPooledBulletObjects);
        ImGui::Text(
            "Active: depth %zu, scenery %zu/%zu, enemies %zu, bullets %zu, hit effects %zu, hit visuals %zu",
            activeDepthCueCount,
            activeSceneryCount,
            totalSceneryCount,
            stageEnemies_.Active().size(),
            activeBulletCount,
            combatEffects_.ActiveCount(),
            hitVisualCount);
        ImGui::Text(
            "Pools: player %zu, enemy %zu",
            projectiles_.PlayerPool().AvailableCount(),
            projectiles_.EnemyPool().AvailableCount());
        ImGui::Text(
            "Scene: objects %zu", sceneObjects_.Count());
        ImGui::Separator();
        if (ImPlot::BeginPlot("Frame time", ImVec2(-1.0f, 130.0f))) {
            ImPlot::SetupAxes("frame", "ms", ImPlotAxisFlags_NoTickLabels, 0);
            ImPlot::PlotLine("ms", historyTime, frameMsHistory, 120);
            ImPlot::EndPlot();
        }

        ImGui::Text("スコア: %d", combatProgress_.Score());
        ImGui::Text("敵数: %zu", stageEnemies_.Active().size());
        ImGui::Text("弾数: %zu", projectiles_.PlayerPool().Active().size() + projectiles_.EnemyPool().Active().size());
        if (ImPlot::BeginPlot("実行中カウント", ImVec2(-1.0f, 170.0f))) {
            ImPlot::SetupAxes("フレーム", "数", ImPlotAxisFlags_NoTickLabels, 0);
            ImPlot::PlotLine("敵", historyTime, enemyHistory, 120);
            ImPlot::PlotLine("弾", historyTime, bulletHistory, 120);
            ImPlot::EndPlot();
        }
    }
    ImGui::End();

    if (ImGui::Begin(kWindowTuning, nullptr, panelFlags)) {
        if (ImGui::Button(ICON_FA_ROTATE_LEFT " 調整をリセット")) {
            stageFlight_.ResetSpeedForDebug();
            playerControl_.SetShotTuning({});
            enemyBulletSpeed_ = 0.36f;
            playerControl_.Aim().SetLockRadius(GameplayAim::kDefaultLockRadius);
            playerControl_.Shooting().SetTuning({ 72, 14, 22 });
            enemyShotInterval_ = 64;
            editorStatusMessage_ = "ゲーム調整をリセットしました。";
        }

#ifdef ENABLE_DEBUG_GUI
        if (ImGui::CollapsingHeader("デバッグ操作", ImGuiTreeNodeFlags_DefaultOpen)) {
            const bool isFeverActive = rewards_.Fever().IsActive();
            const float feverRate = isFeverActive ?
                static_cast<float>(rewards_.Fever().RemainingFrames()) / static_cast<float>(kFeverDurationFrames) :
                static_cast<float>(rewards_.Fever().Gauge()) / static_cast<float>(kFeverGaugeMax);
            char feverLabel[64]{};
            if (isFeverActive) {
                std::snprintf(
                    feverLabel,
                    sizeof(feverLabel),
                    "FEVER %.1f sec",
                    static_cast<double>(rewards_.Fever().RemainingFrames()) / 60.0);
            } else {
                std::snprintf(
                    feverLabel,
                    sizeof(feverLabel),
                    "Gauge %d / %d",
                    rewards_.Fever().Gauge(),
                    kFeverGaugeMax);
            }
            ImGui::ProgressBar(
                (std::clamp)(feverRate, 0.0f, 1.0f),
                ImVec2(-1.0f, 0.0f),
                feverLabel);

            const float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;
            const float halfButtonWidth =
                (ImGui::GetContentRegionAvail().x - buttonSpacing) * 0.5f;
            if (ImGui::Button("MAX → 自動発動", ImVec2(halfButtonWidth, 0.0f))) {
                rewards_.Fever().SetGauge(kFeverGaugeMax);
                editorStatusMessage_ = "ゲージを最大にしました。通常更新時に自動発動します。";
            }
            ImGui::SameLine();
            if (ImGui::Button("即時発動", ImVec2(halfButtonWidth, 0.0f))) {
                ActivateFever();
                editorStatusMessage_ = "フィーバーを発動しました。";
            }
            ImGui::BeginDisabled(!isFeverActive);
            if (ImGui::Button("終了", ImVec2(-1.0f, 0.0f))) {
                rewards_.Fever().Stop();
                editorStatusMessage_ = "フィーバーを終了しました。";
            }
            ImGui::EndDisabled();

            ImGui::SeparatorText("スキル");
            ImGui::BeginDisabled(isGameOver_ || isGameClear_ || IsPhantomRaidActive());
            if (ImGui::Button("スキルを即回復", ImVec2(halfButtonWidth, 0.0f))) {
                playerSkill_.ClearCooldownForDebug();
                GrantPhantomRaid();
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!playerSkill_.IsReady());
            if (ImGui::Button("スキル [Q]", ImVec2(halfButtonWidth, 0.0f))) {
                TryActivatePhantomRaid();
            }
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            ImGui::SeparatorText("フェーズ移動");
            const bool phaseJumpDisabled = isGameOver_ || isGameClear_;
            ImGui::BeginDisabled(phaseJumpDisabled);
            if (ImGui::Button("Wave 1", ImVec2(halfButtonWidth, 0.0f))) {
                DebugJumpToStagePhase(0);
            }
            ImGui::SameLine();
            if (ImGui::Button("Wave 2", ImVec2(halfButtonWidth, 0.0f))) {
                DebugJumpToStagePhase(1);
            }
            if (ImGui::Button("Wave 3", ImVec2(halfButtonWidth, 0.0f))) {
                DebugJumpToStagePhase(2);
            }
            ImGui::SameLine();
            if (ImGui::Button("Boss", ImVec2(halfButtonWidth, 0.0f))) {
                DebugJumpToStagePhase(3);
            }
            if (ImGui::Button("狙撃機＋小型編隊", ImVec2(-1.0f, 0.0f))) {
                DebugJumpToStagePhase(4);
            }
            ImGui::EndDisabled();

            Enemy* debugBoss = nullptr;
            for (const auto& enemy : stageEnemies_.Active()) {
                if (enemy && enemy->IsBoss() && !enemy->IsDead()) {
                    debugBoss = enemy.get();
                    break;
                }
            }
            ImGui::SeparatorText("Boss Test");
            ImGui::BeginDisabled(debugBoss == nullptr || bossAttack_.Phase() >= 2);
            if (ImGui::Button("Boss HP 50%", ImVec2(halfButtonWidth, 0.0f)) && debugBoss) {
                const int targetHp = (std::max)(1, debugBoss->GetMaxHp() / 2);
                const int damage = (std::max)(0, debugBoss->GetHp() - targetHp);
                if (damage > 0) {
                    debugBoss->Damage(damage);
                }
                editorStatusMessage_ = "Boss phase 2 trigger armed.";
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(debugBoss == nullptr || bossPresentation_.IsPhaseTransitionPlaying());
            if (ImGui::Button("Next Boss Attack", ImVec2(halfButtonWidth, 0.0f))) {
                bossAttack_.RequestNextForDebug();
                editorStatusMessage_ = "Boss attack advanced.";
            }
            ImGui::EndDisabled();
            if (debugBoss) {
                ImGui::Text(
                    "Phase %d / Pattern %d / Cooldown %d / Counter %d",
                    bossAttack_.Phase(),
                    static_cast<int>(bossAttack_.CurrentPattern()) + 1,
                    bossAttack_.CooldownFrames(),
                    bossAttack_.CounterFrames());
            }
            if (phaseJumpDisabled) {
                ImGui::TextDisabled("フェーズ移動はゲーム進行中のみ使用できます。 ");
            }
        }
#endif

        if (ImGui::CollapsingHeader(ICON_FA_WAND_MAGIC_SPARKLES " 操作感", ImGuiTreeNodeFlags_DefaultOpen)) {
            float railSpeed = stageFlight_.Speed();
            if (ImGui::SliderFloat("レール速度", &railSpeed, 0.010f, 0.900f, "%.3f")) {
                stageFlight_.SetSpeedForDebug(railSpeed);
            }
            auto shotTuning = playerControl_.GetShotTuning();
            bool shotTuningChanged = ImGui::SliderFloat("自弾速度", &shotTuning.normalSpeed, 0.30f, 1.80f, "%.2f");
            shotTuningChanged |= ImGui::SliderFloat("チャージ弾速度", &shotTuning.chargedSpeed, 0.70f, 2.20f, "%.2f");
            shotTuningChanged |= ImGui::SliderFloat("チャージ弾速度倍率", &shotTuning.chargeMultiplier, 1.00f, 2.00f, "%.2f");
            if (shotTuningChanged) { playerControl_.SetShotTuning(shotTuning); }
            ImGui::SliderFloat("敵弾速度", &enemyBulletSpeed_, 0.12f, 0.70f, "%.2f");
            float lockRadius = playerControl_.Aim().GetLockRadius();
            if (ImGui::SliderFloat("チャージ補助範囲", &lockRadius, 40.0f, 260.0f, "%.0f px")) {
                playerControl_.Aim().SetLockRadius(lockRadius);
            }
            auto shootingTuning = playerControl_.Shooting().GetTuning();
            bool shootingChanged = ImGui::SliderInt("チャージ必要フレーム", &shootingTuning.chargeFrames,
                20, PlayerShooting::kMaxChargeFrames);
            shootingChanged |= ImGui::SliderInt("通常射撃クールダウン", &shootingTuning.normalIntervalFrames, 2, 24);
            shootingChanged |= ImGui::SliderInt("チャージ射撃クールダウン", &shootingTuning.chargedIntervalFrames, 4, 36);
            if (shootingChanged) { playerControl_.Shooting().SetTuning(shootingTuning); }
            ImGui::SliderInt("敵射撃間隔", &enemyShotInterval_, 20, 180);
        }

        if (ImGui::CollapsingHeader(ICON_FA_LAYER_GROUP " 敵の配置", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("出現済み: %d / %d", stageFlight_.Schedule().EmittedEnemyCount(), stageFlight_.Schedule().EnemyCount());
            ImGui::TextUnformatted("航空都市の配置データに従って出現します。");
        }

        ImGui::Separator();
        ImGui::Text("現在のウェーブ: %d / %d", (std::min)(stageFlight_.WaveIndex() + 1, kWaveCount), kWaveCount);
        ImGui::Text("出現済み: %d", stageFlight_.Schedule().EmittedEnemyCount());
        ImGui::Text("合計敵数: %d", GetTotalEnemyTargetCount());
    }
    ImGui::End();

    if (showAdvancedDebugPanels) {
        if (ImGui::Begin(kWindowNodeGraph, &showAdvancedDebugPanels, panelFlags)) {
            namespace ed = ax::NodeEditor;
            static ed::EditorContext* editorContext = ed::CreateEditor();
            ed::SetCurrentEditor(editorContext);
            ed::Begin("RuntimeFlow");
            ed::BeginNode(1);
            ImGui::TextUnformatted(ICON_FA_PLAY " 実行処理");
            ed::BeginPin(11, ed::PinKind::Output);
            ImGui::TextUnformatted("更新");
            ed::EndPin();
            ed::EndNode();
            ed::BeginNode(2);
            ImGui::TextUnformatted(ICON_FA_CHART_LINE " 統計");
            ed::BeginPin(21, ed::PinKind::Input);
            ImGui::Text("スコア %d", combatProgress_.Score());
            ed::EndPin();
            ed::EndNode();
            ed::Link(100, 11, 21);
            ed::End();
            ed::SetCurrentEditor(nullptr);
        }
        ImGui::End();

        if (ImGui::Begin(kWindowTextView, &showAdvancedDebugPanels, panelFlags)) {
            static TextEditor editor;
            static bool isEditorInitialized = false;
            if (!isEditorInitialized) {
                editor.SetLanguageDefinition(TextEditor::LanguageDefinition::HLSL());
                editor.SetReadOnly(true);
                editor.SetText(
                    "{\n"
                    "  \"scene\": \"resources/game_scene.json\",\n"
                    "  \"score\": 0,\n"
                    "  \"features\": [\"ImGuiFileDialog\", \"ImPlot\", \"NodeEditor\", \"ColorTextEdit\"]\n"
                    "}\n");
                isEditorInitialized = true;
            }
            static std::string lastTextViewScenePath;
            static int lastTextViewScore = -1;
            if (lastTextViewScenePath != sceneObjects_.Path() ||
                lastTextViewScore != combatProgress_.Score()) {
                char buffer[512]{};
                std::snprintf(
                    buffer,
                    sizeof(buffer),
                    "{\n"
                    "  \"scene\": \"%s\",\n"
                    "  \"score\": %d,\n"
                    "  \"sceneObjects\": %zu,\n"
                    "  \"features\": [\"ImGuiFileDialog\", \"ImPlot\", \"NodeEditor\", \"ColorTextEdit\"]\n"
                    "}\n",
                    sceneObjects_.Path().c_str(),
                    combatProgress_.Score(),
                    sceneObjects_.Count());
                editor.SetText(buffer);
                lastTextViewScenePath = sceneObjects_.Path();
                lastTextViewScore = combatProgress_.Score();
            }
            ImGui::TextDisabled("ImGuiColorTextEdit");
            editor.Render("RuntimeJsonPreview", ImVec2(-1.0f, -1.0f));
        }
        ImGui::End();
    }

    if (ImGui::Begin(kWindowConsole, nullptr, panelFlags)) {
        ImGui::TextUnformatted("F1: GUI表示切り替え");
        ImGui::TextUnformatted("F2: タイトルへ戻る");
        ImGui::TextUnformatted("タブをドラッグするとレイアウトを並べ替えできます。");
        ImGui::Separator();
        ImGui::TextWrapped("%s", editorStatusMessage_.c_str());
    }
    ImGui::End();

    if (ImGuiFileDialog::Instance()->Display("GameSceneOpenDialog", ImGuiWindowFlags_NoCollapse, ImVec2(760, 460))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            GameplaySceneSetup::LoadSceneObjects(SceneSetupTargets(),
                ImGuiFileDialog::Instance()->GetFilePathName().c_str(), IsTutorial());
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("GameSceneSaveDialog", ImGuiWindowFlags_NoCollapse, ImVec2(760, 460))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            SaveSceneObjects(ImGuiFileDialog::Instance()->GetFilePathName().c_str());
        }
        ImGuiFileDialog::Instance()->Close();
    }
    if (ImGuiFileDialog::Instance()->Display("ProjectAssetDialog", ImGuiWindowFlags_NoCollapse, ImVec2(760, 460))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            editorStatusMessage_ =
                "選択中のアセット: " +
                ImGuiFileDialog::Instance()->GetFilePathName();
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

// 更新・描画・GPU待ちなどのフレーム時間を、開発用の画面に表示する。
void GameRuntime::DrawPerformanceOverlay()
{
    if (!isPerformanceOverlayVisible_) {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();

    const size_t hitVisualCount = combatEffects_.ActiveVisualCount();

    const size_t activePlayerBulletCount = projectiles_.PlayerPool().Active().size();
    const size_t activeEnemyBulletCount = projectiles_.EnemyPool().Active().size();
    const size_t activeBulletCount = activePlayerBulletCount + activeEnemyBulletCount;
    const size_t activeBulletDrawEstimate =
        activePlayerBulletCount * 3u +
        activeEnemyBulletCount * 2u;
    const size_t pooledBulletCount = projectiles_.PlayerPool().AvailableCount() + projectiles_.EnemyPool().AvailableCount();
    const size_t pooledHitEffectObjects = combatEffects_.AvailableCount();
    const size_t activeSceneryCount = railScenery_.VisibleCount();
    const size_t totalSceneryCount = railScenery_.Objects().size();
    const size_t activeDepthCueCount = flightSpeedEffects_.WorldCueCount();
    const size_t estimatedActiveDrawObjects =
        sceneObjects_.Count() +
        activeSceneryCount +
        activeDepthCueCount +
        (showSkybox_ && skybox_ ? 1u : 0u) +
        (player_ ? 1u : 0u) +
        stageEnemies_.Active().size() +
        activeBulletDrawEstimate +
        hitVisualCount;
    const size_t estimatedPooledBulletObjects = pooledBulletCount * 5u;
    DirectXCommon::FrameTiming timing{};
    if (dxCommon_) {
        timing = dxCommon_->GetFrameTiming();
    }
    const float measuredFrameMs =
        timing.frameCpuMs > 0.0f ?
        timing.frameCpuMs :
        (io.Framerate > 0.0f ? 1000.0f / io.Framerate : 0.0f);
    const float measuredFps =
        measuredFrameMs > 0.0f ? 1000.0f / measuredFrameMs : 0.0f;

    Math::Vector2 hudMin{};
    Math::Vector2 hudSize{};
    GetEffectiveHudViewportRect(hudMin, hudSize);

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImVec2 panelMin(hudMin.x + 18.0f, hudMin.y + hudSize.y - 206.0f);
    const ImVec2 panelMax(panelMin.x + 540.0f, panelMin.y + 186.0f);
    const ImU32 accentColor =
        measuredFrameMs > 28.0f ? IM_COL32(255, 90, 90, 235) :
        measuredFrameMs > 18.0f ? IM_COL32(255, 205, 92, 235) :
                                  IM_COL32(116, 242, 190, 235);

    drawList->AddRectFilled(
        ImVec2(panelMin.x + 3.0f, panelMin.y + 4.0f),
        ImVec2(panelMax.x + 3.0f, panelMax.y + 4.0f),
        IM_COL32(0, 0, 0, 82),
        6.0f);
    drawList->AddRectFilled(panelMin, panelMax, IM_COL32(6, 10, 18, 180), 6.0f);
    drawList->AddRect(panelMin, panelMax, accentColor, 6.0f, 0, 2.0f);

    char line[256]{};
    std::snprintf(
        line,
        sizeof(line),
        "F3 PERF inst %.1f FPS / %.2f ms  avg %.1f FPS",
        measuredFps,
        measuredFrameMs,
        io.Framerate);
    drawList->AddText(ImVec2(panelMin.x + 12.0f, panelMin.y + 10.0f), accentColor, line);

    std::snprintf(
        line,
        sizeof(line),
        "draw est %zu  depth %zu  scenery %zu/%zu  bullets %zu  enemies %zu",
        estimatedActiveDrawObjects,
        activeDepthCueCount,
        activeSceneryCount,
        totalSceneryCount,
        activeBulletCount,
        stageEnemies_.Active().size());
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 35.0f),
        IM_COL32(224, 238, 248, 230),
        line);

    std::snprintf(
        line,
        sizeof(line),
        "pool P %zu/%d E %zu/%d  miss P/E %zu/%zu  preallocated",
        projectiles_.PlayerPool().AvailableCount(),
        kTargetPlayerBulletPoolCount,
        projectiles_.EnemyPool().AvailableCount(),
        kTargetEnemyBulletPoolCount,
        projectiles_.PlayerPool().MissCount(),
        projectiles_.EnemyPool().MissCount());
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 58.0f),
        IM_COL32(188, 210, 228, 220),
        line);

    std::snprintf(
        line,
        sizeof(line),
        "pool objs bullet %zu  hitFx %zu/%d miss %zu  post %d%s sky %s",
        estimatedPooledBulletObjects,
        pooledHitEffectObjects,
        kTargetHitEffectObjectPoolCount,
        combatEffects_.MissCount(),
        GetPostEffectMode(),
        isPostEffectBypassEnabled_ ? " BYPASS" : "",
        showSkybox_ ? "ON" : "OFF");
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 82.0f),
        IM_COL32(188, 210, 228, 220),
        line);

    std::snprintf(
        line,
        sizeof(line),
        "cpu upd %.1f pre %.1f scene %.1f post %.1f total %.1f",
        timing.updateMs,
        timing.preDrawMs,
        timing.sceneDrawMs,
        timing.postEffectMs,
        timing.frameCpuMs);
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 106.0f),
        IM_COL32(206, 224, 238, 224),
        line);

    std::snprintf(
        line,
        sizeof(line),
        "rail %.1f speed %.3f  stage %.1f pace %.3f  %s / %s",
        stageFlight_.Distance(),
        stageFlight_.Speed(),
        stageFlight_.Schedule().Progress(),
        stageFlight_.Schedule().TimelineSpeed(),
        stageFlight_.SectionName(),
        stageFlight_.CombatBeat());
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 130.0f),
        IM_COL32(206, 224, 238, 224),
        line);

    std::snprintf(
        line,
        sizeof(line),
        "wait present %.1f fence %.1f fps %.1f  F4 post/F5 sky",
        timing.presentMs,
        timing.fenceWaitMs,
        timing.fpsWaitMs);
    drawList->AddText(
        ImVec2(panelMin.x + 12.0f, panelMin.y + 154.0f),
        IM_COL32(206, 224, 238, 224),
        line);
}

#endif
