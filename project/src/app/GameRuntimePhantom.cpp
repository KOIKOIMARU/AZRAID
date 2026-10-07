#include "app/GameRuntime.h"
#include "app/CombatHud.h"
#include "engine/3d/Model.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/Logger.h"
#include "engine/audio/SoundManager.h"
#include "engine/io/Input.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string_view>

void GameRuntime::InitializePhantomAudio()
{
    phantomLocalAudio_ = false;
#ifdef _DEBUG
    phantomAudioPlays_.fill(0);
#endif
    if (!sound_) { return; }
    // Sonniss素材の加工WAVは公開リポジトリへ入れない。通常配布は既存CC0バンクを使う。
    // 入場時だけ探索・デコードし、プレイ中のファイルアクセスやボイス追加はしない。
    const std::string local = "../generated/audio-phantom-runtime/";
    char* setting = nullptr;
    size_t settingLength = 0;
    const bool originalOnly = _dupenv_s(&setting, &settingLength, "AZRAID_PHANTOM_AUDIO") == 0 &&
        setting && std::string_view(setting) == "original";
    std::free(setting);
    bool complete = !originalOnly;
    for (const char* name : { "slash.wav", "slash_02.wav", "slash_03.wav", "slash_finish.wav" }) {
        std::error_code error;
        complete = complete && std::filesystem::is_regular_file(local + name, error);
    }
    if (complete) {
        const bool slashLoaded = sound_->LoadVariations("slash",
            { local + "slash.wav", local + "slash_02.wav", local + "slash_03.wav" },
            3, 0.66f, 0.060f, 0.006f, 0.025f);
        const bool finishLoaded = sound_->Load("slash_finish", local + "slash_finish.wav",
            1, 0.78f, 0.4f);
        phantomLocalAudio_ = slashLoaded && finishLoaded;
        if (!phantomLocalAudio_) {
            // 一部だけ成功した場合も両方を戻す。欠落や破損で連撃が無音にならない。
            sound_->Unload("slash");
            sound_->Unload("slash_finish");
        }
    }
    if (!phantomLocalAudio_) {
        sound_->LoadVariations("slash", { "resources/audio/combat/slash.wav",
            "resources/audio/combat/slash_02.wav", "resources/audio/combat/slash_03.wav" },
            3, 0.57f, 0.060f, 0.012f, 0.05f);
        sound_->Load("slash_finish", "resources/audio/combat/slash_finish.wav", 1, 0.68f, 0.4f);
    }
    Logger::Log(phantomLocalAudio_ ? "Phantom audio: local cinematic bank loaded.\n" :
        "Phantom audio: original CC0 bank loaded.\n");
}

void GameRuntime::ResetPhantomRaid()
{
    phantomReady_ = true;
    phantomEmpowered_ = false;
    phantomFinished_ = false;
    phantomClock_ = -1.0f;
    phantomCooldown_ = 0.0f;
    phantomReadyFlash_ = 0.0f;
    phantomNoTargetNotice_ = 0.0f;
    phantomTargetCount_ = 0;
    phantomStrikeCount_ = 3;
    phantomNextStrike_ = 0;
    phantomActivationCount_ = 0;
    phantomDefeatCount_ = 0;
    phantomTargets_.fill({});
    for (auto& slash : phantomSlashes_) {
        slash.age = -1.0f;
    }
}

void GameRuntime::InitializePhantomRaid()
{
    ResetPhantomRaid();
    phantomModelCenter_ = {};
    if (!playerModel_ || !object3dCommon_) {
        return;
    }
    if (!playerModel_->GetVertices().empty()) {
        Math::Vector3 min{ (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)() };
        Math::Vector3 max{ -min.x, -min.y, -min.z };
        for (const auto& vertex : playerModel_->GetVertices()) {
            min.x = (std::min)(min.x, vertex.position.x);
            min.y = (std::min)(min.y, vertex.position.y);
            min.z = (std::min)(min.z, vertex.position.z);
            max.x = (std::max)(max.x, vertex.position.x);
            max.y = (std::max)(max.y, vertex.position.y);
            max.z = (std::max)(max.z, vertex.position.z);
        }
        phantomModelCenter_ = { (min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f, (min.z + max.z) * 0.5f };
    }
    for (auto& slash : phantomSlashes_) {
        slash.ghost = std::make_unique<Object3d>();
        slash.ghost->Initialize(object3dCommon_.get());
        slash.ghost->SetModel(playerModel_);
        slash.ghost->SetTextureFilePath("resources/human/white.png");
        slash.ghost->SetLightingMode(0);
        slash.ghost->SetEnvironmentCoefficient(0.0f);
        slash.ghost->SetColor({ 0.25f, 0.80f, 1.0f, 0.0f });
        slash.ghost->Update();
    }
}

void GameRuntime::GrantPhantomRaid()
{
    if (phantomReady_ || IsPhantomRaidActive() || phantomCooldown_ > 0.0f || isGameOver_ || isGameClear_) {
        return;
    }
    phantomReady_ = true; // 1回分を保持。時間切れで消さず、使いどころを選べる。
    phantomReadyFlash_ = 72.0f;
    PlaySfx("skill_ready");
}

void GameRuntime::RecoverPhantomRaidOnHit(bool charged, bool destroyed)
{
    if (phantomReady_ || IsPhantomRaidActive() || !player_ || player_->IsDead() || isGameOver_ || isGameClear_) {
        return;
    }
    // 通常射撃の命中0.2秒、チャージ命中0.4秒、撃破はさらに0.5秒短縮。
    // 連撃自身のダメージでは呼ばず、スキルによる自己再充填を防ぐ。
    const float recovery = (charged ? 24.0f : 12.0f) + (destroyed ? 30.0f : 0.0f);
    phantomCooldown_ = (std::max)(0.0f, phantomCooldown_ - recovery);
    if (phantomCooldown_ <= 0.0f) {
        GrantPhantomRaid();
    }
}

Enemy* GameRuntime::FindPhantomTarget(const Enemy* target) const
{
    for (const auto& enemy : enemies_) {
        if (enemy.get() == target && !enemy->IsDead() && enemy->IsTargetable()) {
            return enemy.get();
        }
    }
    return nullptr;
}

bool GameRuntime::TryActivatePhantomRaid()
{
    if (IsTutorial() && (tutorial_.success > 0.0f ||
        (tutorial_.lesson != TutorialLesson::Skill && tutorial_.lesson != TutorialLesson::Fever))) { return false; }
    if (!phantomReady_ || IsPhantomRaidActive() || !player_ || player_->IsDead() || isGameClear_ || isGameOver_) {
        return false;
    }
    std::array<PhantomTarget, 5> selected{};
    Math::Vector2 min{}, size{};
    GetEffectiveHudViewportRect(min, size);
    const int capacity = feverTimer_ > 0 ? 5 : 3;
    int count = 0;
    // 照準に近い敵を最初に選び、画面内・前方75m以内だけへ連鎖する。
    for (int slot = 0; slot < capacity; ++slot) {
        Enemy* best = nullptr;
        float bestDistance = (std::numeric_limits<float>::max)();
        for (const auto& enemy : enemies_) {
            if (!enemy || enemy->IsDead() || !enemy->IsTargetable()) {
                continue;
            }
            bool duplicate = false;
            for (int index = 0; index < count; ++index) {
                duplicate |= selected[index].enemy == enemy.get();
            }
            if (duplicate) {
                continue;
            }
            const auto position = enemy->GetAimPosition();
            const float ahead = position.z - player_->GetTranslate().z;
            Math::Vector2 screen{};
            if (ahead < 1.0f || ahead > 75.0f || !TryProjectToScreen(position, screen) ||
                screen.x < min.x || screen.x > min.x + size.x || screen.y < min.y || screen.y > min.y + size.y) {
                continue;
            }
            const float dx = screen.x - reticleScreen_.x;
            const float dy = screen.y - reticleScreen_.y;
            const float distance = dx * dx + dy * dy;
            if (distance < bestDistance) {
                best = enemy.get();
                bestDistance = distance;
            }
        }
        if (!best) {
            break;
        }
        selected[count++] = { best, best->GetAimPosition(), 0 };
        if (best->IsBoss()) {
            break; // ボスを狙ったときは一体への集中連撃。
        }
    }
    if (count == 0) {
        phantomNoTargetNotice_ = 60.0f;
        return false; // 対象なしでは消費しない。
    }
    phantomTargets_ = selected;
    phantomTargetCount_ = count;
    phantomEmpowered_ = feverTimer_ > 0;
    phantomStrikeCount_ = capacity;
    phantomNextStrike_ = 0;
    phantomFinished_ = false;
    phantomReady_ = false;
    phantomReadyFlash_ = 0.0f;
    phantomNoTargetNotice_ = 0.0f;
    phantomClock_ = 0.0f;
    phantomCooldown_ = kPhantomCooldownFrames;
    ++phantomActivationCount_;
    for (auto& slash : phantomSlashes_) {
        slash.age = -1.0f;
    }
    PlaySfx("skill_start");
    return true;
}

void GameRuntime::DealPhantomDamage(Enemy& enemy, int damage)
{
    if (enemy.IsDead()) {
        return;
    }
    const bool boss = enemy.IsBoss();
    const auto position = enemy.GetAimPosition();
    const bool destroyed = enemy.Damage(boss && bossCounterTimer_ > 0 ? damage * 2 : damage);
    TriggerHitConfirm(position, true, boss, destroyed);
    AddFeverGauge(destroyed ? 16 : 3);
    if (destroyed) {
        ++phantomDefeatCount_;
        PlaySfx("destroy");
        OnEnemyDestroyed(enemy, true, phantomEmpowered_);
    } else {
        AddEnemyImpactEffect(position, boss ? 1.35f : 0.80f);
    }
}

void GameRuntime::UpdatePhantomRaid()
{
    const float step = dxCommon_ ? std::clamp(dxCommon_->GetDeltaTime() * 60.0f, 0.0f, 4.0f) : 1.0f;
    phantomCooldown_ = (std::max)(0.0f, phantomCooldown_ - step);
    phantomReadyFlash_ = (std::max)(0.0f, phantomReadyFlash_ - step);
    phantomNoTargetNotice_ = (std::max)(0.0f, phantomNoTargetNotice_ - step);
    for (auto& slash : phantomSlashes_) {
        if (slash.age >= 0.0f) {
            slash.age += step;
            if (slash.age >= 22.0f) {
                slash.age = -1.0f;
            }
        }
    }
    if (!player_ || player_->IsDead() || isGameOver_) {
        phantomClock_ = -1.0f;
        phantomTargets_.fill({});
        return;
    }
    if (!IsPhantomRaidActive()) {
        // 回避や命中がなくても時間経過だけで回復し、使いどころを自由に選べる。
        GrantPhantomRaid();
        if (input_ && input_->TriggerKey(DIK_Q)) {
            TryActivatePhantomRaid();
        }
        return;
    }
    phantomClock_ += step;
    for (int index = 0; index < phantomTargetCount_; ++index) {
        if (const Enemy* enemy = FindPhantomTarget(phantomTargets_[index].enemy)) {
            phantomTargets_[index].position = enemy->GetAimPosition();
        }
    }
    while (phantomNextStrike_ < phantomStrikeCount_ && phantomClock_ >= 12.0f + static_cast<float>(phantomNextStrike_) * 7.0f) {
        auto& target = phantomTargets_[phantomNextStrike_ % phantomTargetCount_];
        Enemy* enemy = FindPhantomTarget(target.enemy);
        if (enemy) {
            target.position = enemy->GetAimPosition();
            ++target.marks;
            auto& slash = phantomSlashes_[phantomNextStrike_];
            slash.position = target.position;
            slash.angle = phantomNextStrike_ % 2 == 0 ? 0.55f : -0.65f;
            slash.age = 0.0f;
            // 連撃の段階が耳でも分かるよう、2打目以降だけ少しずつ上げる。
            PlaySfx("slash", 0.98f + 0.035f * static_cast<float>(phantomNextStrike_));
            AddCameraShake(0.045f, 5);
            if (enemy->IsBoss()) {
                DealPhantomDamage(*enemy, 1);
            }
        }
        ++phantomNextStrike_;
    }
    if (!phantomFinished_ && phantomClock_ >= PhantomFinisherTime()) {
        phantomFinished_ = true;
        phantomFinishPosition_ = {};
        for (int index = 0; index < phantomTargetCount_; ++index) {
            auto& target = phantomTargets_[index];
            if (Enemy* enemy = FindPhantomTarget(target.enemy)) {
                target.position = enemy->GetAimPosition();
                if (target.marks > 0) {
                    DealPhantomDamage(*enemy, enemy->IsBoss() ? (phantomEmpowered_ ? 9 : 5) : (phantomEmpowered_ ? 9 : 6));
                }
            }
            phantomFinishPosition_.x += target.position.x / static_cast<float>(phantomTargetCount_);
            phantomFinishPosition_.y += target.position.y / static_cast<float>(phantomTargetCount_);
            phantomFinishPosition_.z += target.position.z / static_cast<float>(phantomTargetCount_);
        }
        // 斬った場所の近くの弾だけを払う。画面全体を無条件に消さない。
        for (auto& bullet : enemyBullets_) {
            for (int index = 0; index < phantomTargetCount_; ++index) {
                const auto& p = phantomTargets_[index].position;
                const auto& b = bullet->GetTranslate();
                const float dx = p.x - b.x, dy = p.y - b.y, dz = p.z - b.z;
                if (dx * dx + dy * dy + dz * dz < 49.0f) {
                    bullet->Kill();
                }
            }
        }
        PlaySfx("slash_finish");
        AddCameraShake(phantomEmpowered_ ? 0.19f : 0.13f, 13);
    }
    if (phantomClock_ >= PhantomFinisherTime() + 20.0f) {
        phantomClock_ = -1.0f;
        phantomTargets_.fill({});
    }
}

void GameRuntime::DrawPhantomRaidObjects()
{
    if (!object3dCommon_ || !camera_ || std::none_of(phantomSlashes_.begin(), phantomSlashes_.end(),
        [](const PhantomSlash& slash) { return slash.age >= 0.0f && slash.ghost; })) {
        return;
    }
    const auto previousBlend = object3dCommon_->GetBlendMode();
    const auto previousDepth = object3dCommon_->GetDepthDrawMode();
    object3dCommon_->SetBlendMode(BlendMode::Add);
    object3dCommon_->SetDepthDrawMode(DepthDrawMode::ReadOnly);
    object3dCommon_->CommonDrawSetting();
    for (auto& slash : phantomSlashes_) {
        if (slash.age < 0.0f || !slash.ghost) {
            continue;
        }
        const float rate = std::clamp(slash.age / 22.0f, 0.0f, 1.0f);
        const float travel = -3.8f + 8.0f * std::clamp(slash.age / 9.0f, 0.0f, 1.0f);
        const float scale = phantomEmpowered_ ? 1.95f : 1.65f;
        const float roll = -slash.angle;
        const float cx = phantomModelCenter_.x * scale, cy = phantomModelCenter_.y * scale;
        slash.ghost->SetTranslate({
            slash.position.x + std::cos(slash.angle) * travel - (cx * std::cos(roll) - cy * std::sin(roll)),
            slash.position.y + std::sin(slash.angle) * travel - (cx * std::sin(roll) + cy * std::cos(roll)),
            slash.position.z - 1.2f - phantomModelCenter_.z * scale });
        slash.ghost->SetRotate({ 0.0f, 0.0f, roll });
        slash.ghost->SetScale({ scale, scale, scale });
        const float alpha = 0.50f * (1.0f - rate) * (1.0f - rate);
        slash.ghost->SetColor(phantomEmpowered_ ? Math::Vector4{ 0.92f, 0.55f, 0.20f, alpha } :
            Math::Vector4{ 0.18f, 0.72f, 1.0f, alpha });
        slash.ghost->Update();
        slash.ghost->Draw();
    }
    object3dCommon_->SetBlendMode(previousBlend);
    object3dCommon_->SetDepthDrawMode(previousDepth);
    object3dCommon_->CommonDrawSetting();
}

namespace {
// 曲がった刃を明るい芯＋色付きの縁で描く。画像素材や追加GPUパスは不要。
void DrawPhantomBlade(ImDrawList* draw, ImVec2 center, float radius, float angle,
    float progress, float opacity, bool empowered, float thickness = 1.0f)
{
    constexpr int count = 25;
    std::array<ImVec2, count> points{}, inner{}, glowPoints{}, core{};
    const float c = std::cos(angle), s = std::sin(angle);
    const float width = std::clamp(radius * 0.16f, 7.0f, 30.0f) * thickness;
    for (int index = 0; index < count; ++index) {
        const float u = static_cast<float>(index) / static_cast<float>(count - 1);
        const float arc = -1.15f + 2.30f * u * std::clamp(progress, 0.02f, 1.0f);
        const float x = std::sin(arc) * radius;
        const float y = (std::cos(arc) - 1.0f) * radius * 0.42f;
        const float taper = std::pow((std::max)(0.0f, std::sin(u * 3.14159265f)), 0.80f);
        points[index] = { center.x + x * c - y * s, center.y + x * s + y * c };
        inner[index] = { points[index].x - s * width * taper, points[index].y + c * width * taper };
        glowPoints[index] = { points[index].x - s * width * taper * 1.8f, points[index].y + c * width * taper * 1.8f };
        core[index] = { points[index].x - s * width * taper * 0.18f, points[index].y + c * width * taper * 0.18f };
    }
    const int alpha = static_cast<int>(std::clamp(opacity, 0.0f, 1.0f) * 255.0f);
    const ImU32 glow = empowered ? IM_COL32(255, 132, 49, alpha / 8) : IM_COL32(25, 133, 255, alpha / 8);
    const ImU32 edge = empowered ? IM_COL32(255, 208, 104, alpha / 2) : IM_COL32(59, 193, 255, alpha / 2);
    for (int index = 0; index < count - 1; ++index) {
        draw->AddQuadFilled(points[index], points[index + 1], glowPoints[index + 1], glowPoints[index], glow);
        draw->AddQuadFilled(points[index], points[index + 1], inner[index + 1], inner[index], edge);
        draw->AddQuadFilled(points[index], points[index + 1], core[index + 1], core[index], IM_COL32(241, 253, 255, alpha));
    }
    draw->AddPolyline(points.data(), count, IM_COL32(239, 253, 255, alpha), 0, 1.1f * thickness);
}

void PhantomText(ImDrawList* draw, ImVec2 position, float size, ImU32 color, const char* text)
{
    CombatHud::Readout(draw, position, size, color, text);
}
}

void GameRuntime::DrawPhantomRaidOverlay()
{
    if (!player_ || isGameOver_ || (isGameClear_ && resultTransitionTimer_ <= 0)) {
        return;
    }
    Math::Vector2 min{}, size{};
    GetEffectiveHudViewportRect(min, size);
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->PushClipRect({ min.x, min.y }, { min.x + size.x, min.y + size.y }, true);
    const bool active = IsPhantomRaidActive();
    const bool gold = active ? phantomEmpowered_ : feverTimer_ > 0;
    const ImU32 accent = gold ? CombatHud::Gold : CombatHud::Blue;
    const float hudScale = CombatHud::Scale(size);
    const auto project = [this](const Math::Vector3& world, ImVec2& screen, float& radius, float worldRadius) {
        Math::Vector2 center{}, edge{};
        if (!TryProjectToScreen(world, center) || !TryProjectToScreen({ world.x + worldRadius, world.y, world.z }, edge)) {
            return false;
        }
        screen = { center.x, center.y };
        radius = std::clamp(std::abs(edge.x - center.x), 16.0f, 230.0f);
        return true;
    };
    if (active) {
        const float endFade = 1.0f - std::clamp((phantomClock_ - PhantomFinisherTime() - 6.0f) / 14.0f, 0.0f, 1.0f);
        draw->AddRectFilled({ min.x, min.y }, { min.x + size.x, min.y + size.y },
            IM_COL32(2, 8, 24, static_cast<int>(28.0f * endFade)));
        if (phantomClock_ < 12.0f) {
            ImVec2 center{};
            float radius = 0.0f;
            if (project(player_->GetTranslate(), center, radius, 2.0f)) {
                const float progress = std::clamp(phantomClock_ / 9.0f, 0.0f, 1.0f);
                DrawPhantomBlade(draw, center, radius, -0.30f, progress, 0.85f, gold);
                DrawPhantomBlade(draw, center, radius, 2.85f, progress, 0.70f, gold);
            }
        }
        for (int index = 0; index < phantomTargetCount_; ++index) {
            const auto& target = phantomTargets_[index];
            if (!FindPhantomTarget(target.enemy)) {
                continue;
            }
            ImVec2 center{};
            float radius = 0.0f;
            if (project(target.position, center, radius, 2.4f) && target.marks > 0 && !phantomFinished_) {
                const float r = radius * 0.62f;
                draw->AddLine({ center.x - r, center.y + r * 0.48f }, { center.x + r, center.y - r * 0.48f }, accent, 2.0f);
            }
        }
        if (phantomFinished_) {
            const float age = phantomClock_ - PhantomFinisherTime();
            ImVec2 center{};
            float radius = 0.0f;
            if (project(phantomFinishPosition_, center, radius, gold ? 8.2f : 6.2f)) {
                const float fade = 1.0f - std::clamp(age / 20.0f, 0.0f, 1.0f);
                DrawPhantomBlade(draw, center, radius * (1.0f + age * 0.018f), 0.58f,
                    std::clamp(age / 2.0f, 0.1f, 1.0f), fade, gold, 1.45f);
                DrawPhantomBlade(draw, center, radius, -0.58f,
                    std::clamp(age / 2.0f, 0.1f, 1.0f), fade * 0.8f, gold, 1.20f);
            }
            if (age < 4.0f) {
                draw->AddRectFilled({ min.x, min.y }, { min.x + size.x, min.y + size.y },
                    IM_COL32(205, 239, 255, static_cast<int>((1.0f - age / 4.0f) * 34.0f)));
            }
        }
        const char* title = "残像連撃";
        const float textSize = 30.0f * hudScale;
        const float width = CombatHud::ReadoutWidth(title, textSize, true);
        const float titleX = min.x + (size.x - width) * 0.5f;
        const float titleY = min.y + size.y - 152.0f * hudScale;
        CombatHud::Heading(draw, { titleX, titleY }, textSize, CombatHud::White, title);
    }
    for (const auto& slash : phantomSlashes_) {
        if (slash.age < 0.0f) {
            continue;
        }
        ImVec2 center{};
        float radius = 0.0f;
        if (project(slash.position, center, radius, 4.6f)) {
            const float fade = 1.0f - std::clamp(slash.age / 22.0f, 0.0f, 1.0f);
            DrawPhantomBlade(draw, center, radius, slash.angle, std::clamp(slash.age / 4.0f, 0.05f, 1.0f),
                fade, phantomEmpowered_);
        }
    }
    draw->PopClipRect();
}

void GameRuntime::DrawPhantomRaidHud()
{
    if (!player_ || isGameOver_ || (isGameClear_ && resultTransitionTimer_ <= 0)) { return; }
    Math::Vector2 min{}, size{};
    GetEffectiveHudViewportRect(min, size);
    const CombatHud::Layout layout(min, size);
    const float hudScale = layout.scale;
    const ImVec2 panel = layout.skill;
    auto* draw = ImGui::GetForegroundDrawList();
    const bool active = IsPhantomRaidActive();
    const auto p = [&](float x, float y) { return ImVec2(panel.x + x * hudScale, panel.y + y * hudScale); };
    // フィーバー中も「スキル＝青」の意味は変えない。強化演出の金色は斬撃側だけに使う。
    const ImU32 hudAccent = CombatHud::Energy;
    const bool lessonLocked = IsTutorial() && tutorial_.lesson < TutorialLesson::Skill;
    const bool ready = !lessonLocked && (phantomReady_ || active);
    const float readyBurst = std::clamp(phantomReadyFlash_ / 72.0f, 0.0f, 1.0f);
    CombatHud::Heading(draw, p(0, 0), 26.0f * hudScale, CombatHud::White, "残像連撃");
    CombatHud::KeyBadge(draw, p(212, 2), p(244, 28), hudScale, "Q", ready);
    if (lessonLocked) {
        PhantomText(draw, p(0, 36), 16.0f * hudScale, CombatHud::Muted, "練習待機");
    } else if (!active && phantomNoTargetNotice_ > 0.0f) {
        PhantomText(draw, p(0, 36), 16.0f * hudScale, CombatHud::Muted, "対象なし");
    } else if (!active && !phantomReady_ && phantomCooldown_ > 0.0f) {
        char cooldown[24]{};
        std::snprintf(cooldown, sizeof(cooldown), "%.1f秒", static_cast<double>(phantomCooldown_ / 60.0f));
        CombatHud::Readout(draw, p(0, 36), 18.0f * hudScale, CombatHud::White, cooldown);
    } else {
        PhantomText(draw, p(0, 36), 18.0f * hudScale, CombatHud::White, active ? "発動中" : "使用可能");
    }
    const float recovery = lessonLocked ? 0.0f : (ready ? 1.0f : 1.0f - std::clamp(phantomCooldown_ / kPhantomCooldownFrames, 0.0f, 1.0f));
    CombatHud::WingMeter(draw, p(0, 76), 244 * hudScale, 12 * hudScale, recovery,
        ready ? CombatHud::White : hudAccent, hudScale,
        ready ? std::fmod(cameraTimer_ * 0.45f, 1.0f) : -1.0f);
    if (readyBurst > 0.0f) {
        draw->AddQuadFilled(p(10, 75.25f), p(244, 57.7f), p(234, 70.45f), p(0, 88),
            CombatHud::SurfaceColor(IM_COL32(239, 241, 244, static_cast<int>(80 * readyBurst * readyBurst))));
    }
}
