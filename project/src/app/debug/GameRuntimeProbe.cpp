// ===== GameRuntimeProbe.cpp =====
// 明示的に起動したDebug版の検証で、操作入力を再現し、戦闘や通しプレイの結果を確かめる。

#ifdef _DEBUG
#include "app/runtime/GameRuntime.h"
#include "app/runtime/GameRuntimeMath.h"
#include "app/runtime/GameRuntimeTuning.h"
#include "app/player/JustDodgeFeedback.h"
#include "engine/3d/ModelManager.h"
#include "engine/io/Input.h"
#include "engine/audio/SoundManager.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <iterator>
#include <cstdlib>
#include <string_view>

namespace {
using GameRuntimeMath::DistanceSquared;
using GameRuntimeTuning::kChargeSplashRadius;
using GameRuntimeTuning::kFeverDurationFrames;
using GameRuntimeTuning::kRiftPassScore;

// 配置ファイルの保存と再読込を試し、読込失敗時に元の配置が保たれるか確認する。
void CheckSceneObjectRoundTrip(Object3dCommon& common, const std::string& logPath)
{
    namespace fs = std::filesystem;
    const auto directory = fs::path(logPath + ".scene-fixture");
    if (!fs::create_directory(directory)) {
        throw std::runtime_error("Scene fixture directory already exists");
    }
    struct FixtureFiles {
        fs::path directory;
        ~FixtureFiles() {
            std::error_code error;
            fs::remove(directory / "scene.json", error);
            fs::remove(directory, error);
        }
    } cleanup{ directory };
    const auto path = (directory / "scene.json").string();

    SceneSerializer::ObjectRecord object;
    object.name = "Scene fixture";
    object.primitive = true;
    object.modelIndex = 0;
    object.translate = { 1.0f, 2.0f, 3.0f };
    auto invalid = object;
    invalid.modelIndex = 999;
    SceneSerializer::SceneSettings settings;
    settings.hasCamera = true;
    settings.cameraTranslate = { 4.0f, 5.0f, 6.0f };
    if (!SceneSerializer::SaveScene(path.c_str(), { object, invalid }, settings)) {
        throw std::runtime_error("Could not write scene fixture");
    }

    RuntimeSceneObjects scene;
    SceneSerializer::SceneSettings loaded;
    if (!scene.Load(path.c_str(), common, loaded) || scene.Count() != 1 ||
        !loaded.hasCamera || loaded.cameraTranslate.z != 6.0f) {
        throw std::runtime_error("Scene fixture load lost object or camera information");
    }
    auto* placedObject = scene.Objects().front().get();
    placedObject->SetTranslate({ 7.0f, 8.0f, 9.0f });
    if (scene.Load(nullptr, common, loaded) || scene.Count() != 1 ||
        scene.Objects().front().get() != placedObject || scene.Path() != path) {
        throw std::runtime_error("Failed scene load changed the current scene");
    }

    std::vector<SceneSerializer::ObjectRecord> records;
    if (!scene.Save(scene.Path().c_str(), loaded) || !SceneSerializer::LoadScene(path.c_str(), records, loaded) ||
        records.size() != 1 || records.front().name != object.name || records.front().modelIndex != object.modelIndex ||
        records.front().translate.x != 7.0f || records.front().translate.z != 9.0f ||
        !loaded.hasCamera || loaded.cameraTranslate.z != 6.0f) {
        throw std::runtime_error("Scene save did not preserve placement and metadata");
    }
}

// 弾の進行方向やカメラの向きが変わっても、弾本体と光の軌跡が正しく置かれるか確認する。
void CheckProjectileVisualProjection()
{
    const auto require = [](bool ok, const char* message) {
        if (!ok) { throw std::runtime_error(message); }
    };
    const auto identity = Math::MakeIdentity4x4();
    require(Bullet::CalculateTrailVisual({ 0, 0, 20 }, { 0, 0, 1 }, 4, identity).length == 0.0f,
        "Head-on shot acquired a fake vertical trail");
    require(Bullet::CalculateTrailVisual({ 0, 0, -1 }, { 0, 0, 1 }, 4, identity).length == 0.0f,
        "Projectile behind camera retained a trail");
    const auto nearTrail = Bullet::CalculateTrailVisual({ 1, 0, 0.1f }, { 0, 0, 1 }, 4, identity);
    require(std::isfinite(nearTrail.length) && nearTrail.length <= 0.076f,
        "Near-camera projectile produced an unbounded trail");
    for (const Math::Vector3 velocity : { Math::Vector3{ 1, 2, 3 },
        Math::Vector3{ -1, -0.3f, -4 }, Math::Vector3{ 0, 2, 0 } }) {
        const auto body = Math::MakeAffineMatrix({ 1, 1, 1 }, Bullet::CalculateBodyRotation(velocity), {});
        const auto direction = Math::Normalize(velocity);
        require(body.m[2][0] * direction.x + body.m[2][1] * direction.y + body.m[2][2] * direction.z > 0.9999f,
            "Projectile body did not follow its flight direction");
    }
    // 俯角・旋回・バンク・カメラ移動を含め、軌跡の両端が本当の弾道へ重なるか検証。
    const auto camera = Math::MakeAffineMatrix({ 1, 1, 1 }, Math::Vector3{ 0.16f, -0.12f, 0.11f }, { 3, -1, -5 });
    const auto toWorld = [&](const Math::Vector3& p, float w) {
        return Math::Vector3{ p.x * camera.m[0][0] + p.y * camera.m[1][0] + p.z * camera.m[2][0] + w * camera.m[3][0],
            p.x * camera.m[0][1] + p.y * camera.m[1][1] + p.z * camera.m[2][1] + w * camera.m[3][1],
            p.x * camera.m[0][2] + p.y * camera.m[1][2] + p.z * camera.m[2][2] + w * camera.m[3][2] };
    };
    const auto head = toWorld({ 6, 3, 40 }, 1);
    const auto velocity = toWorld({ 0.14f, -0.06f, 1.2f }, 0);
    const auto direction = Math::Normalize(velocity);
    const Math::Vector3 tail{ head.x - direction.x * 3, head.y - direction.y * 3, head.z - direction.z * 3 };
    const auto trail = Bullet::CalculateTrailVisual(head, velocity, 3, camera);
    const auto transform = Math::MakeAffineMatrix({ 1, 1, 1 }, trail.rotate, trail.center);
    const Math::Vector3 drawnHead{ trail.center.x + transform.m[1][0] * trail.length * 0.5f,
        trail.center.y + transform.m[1][1] * trail.length * 0.5f,
        trail.center.z + transform.m[1][2] * trail.length * 0.5f };
    const Math::Vector3 drawnTail{ trail.center.x - transform.m[1][0] * trail.length * 0.5f,
        trail.center.y - transform.m[1][1] * trail.length * 0.5f,
        trail.center.z - transform.m[1][2] * trail.length * 0.5f };
    const auto view = Math::Inverse(camera);
    const auto project = [&](const Math::Vector3& p) {
        const float z = p.x * view.m[0][2] + p.y * view.m[1][2] + p.z * view.m[2][2] + view.m[3][2];
        return Math::Vector2{ (p.x * view.m[0][0] + p.y * view.m[1][0] + p.z * view.m[2][0] + view.m[3][0]) / z,
            (p.x * view.m[0][1] + p.y * view.m[1][1] + p.z * view.m[2][1] + view.m[3][1]) / z };
    };
    const auto a = project(head), b = project(drawnHead), c = project(tail), d = project(drawnTail);
    require(std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(c.x - d.x) + std::abs(c.y - d.y) < 0.0002f,
        "Billboard trail detached from projectile under camera pitch/bank");
}

// 命中・被弾などの状態をHUDに渡し、描画の追加と表示終了を確認する。
void CheckGameplayHudRendering(const GameplayHud::Frame& gameplay)
{
    const auto require = [](bool ok, const char* message) {
        if (!ok) { throw std::runtime_error(message); }
    };
    GameplayHud hud;
    for (int kind = 0; kind < 4; ++kind) {
        const bool charged = kind == 1;
        const bool destroyed = kind >= 2;
        const bool boss = kind == 3;
        hud.RecordHit({ 300, 200 }, charged, boss, destroyed);
        const int duration = kind == 0 ? 16 : kind == 1 ? 20 : kind == 2 ? 24 : 30;
        require(hud.Inspect().hitFrames == duration, "HUD hit duration changed");
        for (int frame = 0; frame < duration; ++frame) { hud.TickFeedback(); }
        require(hud.Inspect().hitFrames == 0 && hud.Inspect().hitDuration == 1, "HUD hit failed to expire");
    }
    hud.RecordDamage({ 320, 240 }, { 3, 4 });
    auto snapshot = hud.Inspect();
    require(std::abs(snapshot.damageDirection.x + 0.6f) < 0.0001f &&
        std::abs(snapshot.damageDirection.y - 0.8f) < 0.0001f, "HUD damage direction changed");
    for (int frame = 0; frame < 26; ++frame) { hud.TickFeedback(); }
    require(hud.Inspect().damageFrames == 0 && hud.Inspect().damageDuration == 1, "HUD damage failed to expire");
    hud.RecordDamage({ 320, 240 }, {});
    require(hud.Inspect().damageDirection.y == -1.0f, "HUD damage fallback direction changed");
    hud.FlashCharge(30);
    hud.FlashCharge(24);
    hud.TickFeedback();
    require(hud.Inspect().chargeFlash == 30, "HUD charge flash did not preserve the stronger notice");
    hud.TickCharge();
    require(hud.Inspect().chargeFlash == 29, "HUD charge flash advanced outside shooting update");
    hud.FlashChargeShot();
    require(hud.Inspect().chargeFlash == 18, "HUD charged-shot flash did not restart");
    hud.ClearChargeFlash();
    require(hud.Inspect().chargeFlash == 0, "Tutorial retained HUD charge flash");
    hud.RecordHit({ 300, 200 }, true, true, true);
    snapshot = hud.Inspect();

    // 画面へは送らない描画リストで、各状態と表示領域を確認する。
    // 描画を繰り返しても、表示時間・ゲーム状態・フォントの積み方を変えない。
    for (const Math::Vector2 size : { Math::Vector2{ 640, 360 }, Math::Vector2{ 1280, 720 },
        Math::Vector2{ 1920, 1080 }, Math::Vector2{ 1000, 720 } }) {
        for (int state = 0; state < 12; ++state) {
            auto view = gameplay;
            view.min = { 100, 50 };
            view.size = size;
            view.reticle = { view.min.x + size.x * 0.5f, view.min.y + size.y * 0.5f };
            view.hp = state % 3 == 0 ? 10 : state % 3 == 1 ? 60 : 100;
            view.charge = state % 2 ? 1.0f : 0.4f;
            view.chargeReady = view.charge == 1.0f;
            view.tutorial = state == 11;
            view.fever = { state >= 4 && state <= 7, state == 8, 0.8f, state == 7 ? 1.0f : 5.0f, 0.5f };
            view.chain = { state == 9 ? 0 : 8, 3, 0.7f, state == 9 ? 0.8f : 0.0f, 0.4f };
            view.boss = { true, static_cast<GameplayHud::BossCue>(state % 7), 0.6f, 0.7f };
            view.skill = { state == 1, state == 2, state == 3, state == 11, state == 10, 3.5f, 0.8f, 0.5f };
            view.ringNoticeFrames = 30;
            view.ringExtendedFever = state % 2 == 0;
            view.bossRadioSeconds = 3.0f;
            view.bossRadioElapsed = 0.8f;
            view.justDodge = view.bossDefeat = 0.5f;
            ImDrawList draw(ImGui::GetDrawListSharedData());
            draw._ResetForNewFrame();
            draw.PushClipRectFullScreen();
            hud.DrawRingNotice(view, &draw);
            hud.DrawCore(view, &draw);
            hud.DrawBoss(view, &draw);
            hud.DrawCombatFeedback(view, &draw);
            hud.DrawStatus(view, &draw);
            require(draw.VtxBuffer.Size > 0 && draw._ClipRectStack.Size == 1, "HUD did not draw or leaked a clip rectangle");
            for (const auto& vertex : draw.VtxBuffer) {
                require(std::isfinite(vertex.pos.x) && std::isfinite(vertex.pos.y), "HUD emitted a non-finite vertex");
            }
            const auto afterDraw = hud.Inspect();
            require(afterDraw.hitFrames == snapshot.hitFrames && afterDraw.damageFrames == snapshot.damageFrames &&
                afterDraw.chargeFlash == snapshot.chargeFlash, "HUD drawing advanced a feedback clock");
        }
    }
    hud.Reset();
    require(hud.Inspect().hitFrames == 0 && hud.Inspect().damageFrames == 0 && hud.Inspect().chargeFlash == 0,
        "Retry retained HUD feedback");
}

// 自機の噴射・チャージ光・残像が準備され、リセットや状態変化に追従するか確認する。
void CheckPlayerFlightEffects(PlayerFlightEffects& effects, Math::Vector3 position)
{
    const auto require = [](bool ok, const char* message) {
        if (!ok) { throw std::runtime_error(message); }
    };
    require(effects.Inspect().objects == 24, "Flight effects were not fully prepared before gameplay");
    effects.Reset();
    PlayerFlightEffects::FlightFrame flight{};
    flight.position = position;
    flight.speed = 1.0f;
    flight.charge = 1.0f;
    effects.UpdateFlight(flight);
    require(effects.Inspect().chargeLights == 2 && effects.Inspect().plumes > 0,
        "Charge lights or engine plumes were missing");
    flight.feverActive = true;
    flight.fever = 1.0f;
    effects.UpdateFlight(flight);
    require(effects.Inspect().chargeLights == 0 && effects.Inspect().thrust > 0.0f,
        "Fever retained ordinary charge lights or lost engine thrust");
    // 寿命は実時間に合わせる。停止・低fps・早い更新でも残像を保持し続けない。
    for (const float step : { 0.5f, 1.0f, 4.0f }) {
        effects.Reset();
        effects.FlashDodgeTrail(position, -1);
        PlayerFlightEffects::DodgeFrame dodge{};
        dodge.deltaSeconds = step / 60.0f;
        dodge.alive = true;
        require(effects.Inspect().afterimages == 1, "Dodge trail was not emitted immediately");
        const int steps = static_cast<int>(std::ceil(34.0f / step));
        for (int index = 0; index < steps; ++index) { effects.UpdateAfterimages(dodge); }
        require(effects.Inspect().afterimages == 0, "Dodge trails survived their lifetime");
    }
    effects.Reset();
    PlayerFlightEffects::DodgeFrame dodge{};
    dodge.position = position;
    dodge.alive = dodge.dodging = true;
    require(effects.UpdateAfterimages(dodge) && !effects.UpdateAfterimages(dodge),
        "Dodge-begin feedback was repeated while holding dodge");
    for (int index = 0; index < 48; ++index) { effects.FlashDodgeTrail(position, index % 2 ? 1 : -1); }
    require(effects.Inspect().objects == 24 && effects.Inspect().afterimages == 16,
        "Dodge trails did not reuse their fixed pool");
    effects.SetCinematicThrust(2.1f);
    effects.Reset();
    const auto reset = effects.Inspect();
    require(reset.objects == 24 && reset.afterimages == 0 && reset.plumes == 0 &&
        reset.chargeLights == 0 && reset.thrust == 0.0f && reset.afterimageAge == 0.0f,
        "Death/retry reset retained flight effects or destroyed prepared objects");
}

// 狙撃機の構え中の位置、発射後の反動、反撃の受付を確認する。
void CheckSniperPosture(Object3dCommon* common, Model* model)
{
    const auto require = [](bool ok, const char* message) {
        if (!ok) { throw std::runtime_error(message); }
    };
    // 実際のEnemy::Updateで構えの静止とレール追従を検証。ゲームの敵HPや無敵時間は変えない。
    Enemy sniper;
    sniper.Initialize(common, model, { 5.3f, 2.0f, 48.0f }, Enemy::Behavior::Sniper);
    float rail = 0.0f;
    for (int frame = 0; frame < 112; ++frame) {
        rail += 0.2f;
        sniper.Update(rail);
    }
    require(sniper.CanShoot(), "sniper did not finish entry");
    auto& fire = sniper.GetFireControl();
    const EnemyFireControl::Pattern pattern{ 60.0f, 18.0f, 128.0f, 2 };
    fire.Advance(1.0f, true, pattern);
    sniper.Update(rail);
    const auto anchor = sniper.GetTranslate();
    const float ahead = anchor.z - rail;
    for (int frame = 1; frame <= 120; ++frame) {
        rail += 0.45f;
        const auto event = fire.Advance(1.0f, sniper.CanShoot(), pattern);
        sniper.Update(rail);
        const auto position = sniper.GetTranslate();
        require(std::abs(position.x - anchor.x) < 0.001f, "sniper drifted laterally while braced/recovering");
        if (frame < 60) {
            require(std::abs(position.z - rail - ahead) < 0.001f, "sniper brace lost rail tracking");
        }
        if (frame == 60 || frame == 78) {
            require(event == EnemyFireControl::Event::Fire && fire.ShotFlash() == 1.0f,
                "sniper recoil is not tied to the shot");
        }
        if (frame == 110) {
            require(!fire.IsBraced() && std::abs(fire.RecoveryElapsed() - 32.0f) < 0.001f &&
                position.z - rail < ahead - 2.5f, "sniper did not expose recovery opening");
        }
    }
    sniper.Kill();
    fire.Advance(1.0f, sniper.CanShoot(), pattern);
    require(!fire.IsBraced() && fire.ShotFlash() == 0.0f, "destroyed sniper kept charging/firing");
}

// 迎撃方向と射撃周期を確認する。構え中の照準固定とスロー時の連射も試す。
void CheckEnemyFireControl()
{
    const auto require = [](bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    };
    // 通常速度・フィーバー速度・弾速とレール速度が同じ場合も、左右からの狙いがずれない。
    for (const float rail : { 0.16f, 0.235f, 0.45f, 0.65f }) {
        for (const float x : { -10.0f, 0.0f, 10.0f }) {
            const Math::Vector3 origin{ x, 4.0f, 40.0f };
            const Math::Vector3 target{ 3.0f, 0.0f, 0.0f };
            const auto velocity = EnemyFireControl::SolveShotDirection(origin, target, rail, 0.45f) * 0.45f;
            const float t = (origin.z - target.z) / (rail - velocity.z);
            const float dx = origin.x + velocity.x * t - target.x;
            const float dy = origin.y + velocity.y * t - target.y;
            require(std::isfinite(t) && t > 0 && std::abs(dx) < 0.001f && std::abs(dy) < 0.001f,
                "enemy shot misses stationary lateral target during rail movement");
            // 発射後に横へ3m移動すれば、この固定照準弾の当たり判定から抜ける。
            require(std::abs(dx - 3.0f) > 1.5f, "enemy shot followed an evasive movement");
        }
    }
    const EnemyFireControl::Pattern pattern{ 28.0f, 12.0f, 86.0f, 3 };
    for (const float step : { 0.25f, 0.5f, 1.0f }) {
        EnemyFireControl::Cycle control;
        require(control.Advance(step, true, pattern) == EnemyFireControl::Event::Aim,
            "enemy fired before windup");
        int shots = 0;
        for (float elapsed = step; elapsed <= 130.0f; elapsed += step) {
            const auto event = control.Advance(step, true, pattern);
            if (event == EnemyFireControl::Event::Fire) {
                require(std::abs(elapsed - (28.0f + shots * 12.0f)) < 0.01f,
                    "enemy burst ignored slow-motion clock");
                require(control.ShotIndex() == shots, "enemy burst order is incorrect");
                ++shots;
            }
            if (elapsed >= 21.0f && elapsed < 28.0f) {
                require(!control.IsTracking(), "enemy keeps tracking in final windup");
            }
        }
        require(shots == 3, "enemy burst count/recovery is incorrect");
        control.Advance(step, false, pattern);
        require(control.ChargeRate() == 0.0f &&
            control.Advance(step, true, pattern) == EnemyFireControl::Event::Aim,
            "offscreen enemy retained an immediate shot");
    }
    // 狙撃だけは22フレームの固定予告。スロー中も同じ世界時間で二射する。
    for (const float step : { 0.25f, 0.5f, 1.0f }) {
        EnemyFireControl::Cycle sniper;
        const EnemyFireControl::Pattern pattern{ 60.0f, 18.0f, 128.0f, 2, 22.0f };
        sniper.Advance(step, true, pattern);
        int shots = 0;
        for (float elapsed = step; elapsed <= 78.0f; elapsed += step) {
            const auto event = sniper.Advance(step, true, pattern);
            require(sniper.IsTracking() == (elapsed < 38.0f), "sniper lock cue did not match its tracking clock");
            if (event == EnemyFireControl::Event::Fire) {
                require(std::abs(elapsed - (60.0f + shots * 18.0f)) < 0.01f, "sniper burst timing changed");
                ++shots;
            }
        }
        require(shots == 2, "sniper must fire exactly two fixed-aim shots");
    }
}

// 実際のPlayerと入力経路を使う回避回帰試験。描画しない独立自機なので本編へ影響しない。
void CheckDodgeControls(Object3dCommon* common, Model* model)
{
    const auto require = [](bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    };
    const PlayerControl controls;
    const auto update = [&](Player& player, Input& input, float slow = 1.0f, float step = 1.0f) {
        controls.UpdateMovement(player, &input, slow, step, player.GetTranslate().z);
    };
    {
        Player probe;
        Input input;
        probe.Initialize(common, model);
        std::array<BYTE, 256> keys{};
        keys[DIK_D] = keys[DIK_RIGHT] = keys[DIK_W] = keys[DIK_DOWN] = 0x80;
        input.SetTestFrame(keys, {});
        update(probe, input);
        require(std::abs(probe.GetTranslate().x - 0.205f) < 0.001f && probe.GetTranslate().y == 0.0f,
            "movement aliases stacked or opposite inputs did not cancel");
    }
    {
        Player damaged;
        Input input;
        damaged.Initialize(common, model);
        damaged.Damage(10);
        require(damaged.GetHp() == 90 && damaged.IsInvincible() && !damaged.IsDodging(),
            "damage did not grant recovery protection independently of dodge");
        for (int frame = 0; frame < 23; ++frame) {
            input.SetTestFrame({}, {});
            update(damaged, input, 0.08f);
            require(damaged.IsInvincible(), "damage protection ended before its recovery window");
        }
        update(damaged, input, 0.08f);
        require(!damaged.IsInvincible(), "world slowdown stretched damage protection");
        damaged.Reset({});
        damaged.Damage(0);
        require(!damaged.IsInvincible(), "zero damage granted invulnerability");
    }
    for (const float step : { 0.5f, 1.0f, 2.0f }) {
        for (const float slow : { 1.0f, 0.08f }) {
            Player probe;
            Input input;
            probe.Initialize(common, model);
            const int frames = static_cast<int>(16.0f / step);
            for (int frame = 0; frame < frames; ++frame) {
                std::array<BYTE, 256> keys{};
                if (frame == 0) { keys[DIK_D] = keys[DIK_LSHIFT] = 0x80; }
                input.SetTestFrame(keys, {});
                update(probe, input, slow, step);
                require(probe.IsInvincible(), "dodge lost protection before movement ended");
                if (frame == 0 && step == 1.0f) {
                    require(probe.GetTranslate().x > 0.50f, "dodge initial response too slow");
                }
            }
            require(!probe.IsDodging() && std::abs(probe.GetTranslate().x - 4.4f) < 0.001f,
                "dodge distance/duration depends on slow motion or frame rate");
            input.SetTestFrame({}, {});
            update(probe, input, slow, step);
            require(!probe.IsInvincible(), "dodge protection remained after finish");
        }
    }
    for (const bool nearReady : { false, true }) {
        Player probe;
        Input input;
        probe.Initialize(common, model);
        for (int frame = 1; frame <= 29; ++frame) {
            std::array<BYTE, 256> keys{};
            if (frame == 1) { keys[DIK_D] = keys[DIK_LSHIFT] = 0x80; }
            if (frame == (nearReady ? 25 : 19)) { keys[DIK_A] = keys[DIK_LSHIFT] = 0x80; }
            if (frame == 26) { keys[DIK_D] = 0x80; } // 方向を変えても予約した左回避は維持。
            input.SetTestFrame(keys, {});
            update(probe, input);
        }
        require(probe.IsDodging() == nearReady, "dodge input buffer did not expire/activate correctly");
        if (nearReady) { require(probe.GetDodgeDirection() == -1, "buffered dodge direction changed"); }
    }
    Player edge;
    Input input;
    edge.Initialize(common, model);
    std::array<BYTE, 256> right{};
    right[DIK_D] = 0x80;
    input.SetTestFrame(right, {});
    for (int frame = 0; frame < 60; ++frame) { update(edge, input); }
    const float limit = edge.GetTranslate().x;
    for (int frame = 0; frame < 16; ++frame) {
        std::array<BYTE, 256> keys{};
        if (frame == 0) { keys[DIK_D] = keys[DIK_LSHIFT] = 0x80; }
        input.SetTestFrame(keys, {});
        update(edge, input);
        require(std::abs(edge.GetTranslate().x - limit) < 0.001f, "dodge crossed playfield boundary");
    }
    std::array<BYTE, 256> left{};
    left[DIK_A] = 0x80;
    input.SetTestFrame(left, {});
    update(edge, input, 0.08f);
    require(!edge.IsDodging() && edge.GetTranslate().x < limit - 0.15f,
        "movement did not resume promptly after edge dodge in slow motion");
}
} // namespace

// 自動操作で出撃からクリア・再挑戦までを検証する。本編と同じ衝突・ダメージ処理を通る。
bool GameRuntime::RunPlaythroughProbe(const std::string& logPath, bool tutorialPreview, bool tutorialDeparture)
{
    static bool wasTutorial = false;
    if (IsTutorial()) { wasTutorial = true; return RunTutorialProbe(logPath, tutorialPreview, tutorialDeparture); }
    if (wasTutorial) {
        if (player_->GetHp() != 100 || combatProgress_.Score() != 0 || tutorial_.Started() ||
            rewards_.Fever().ActivationCount() != 0 || isGameClear_ || IsMainGameRequested()) {
            throw std::runtime_error("Main game inherited tutorial state");
        }
        std::ofstream file(logPath, std::ios::app);
        file << "TUTORIAL MAIN_ENTRY_OK fresh_hp_score_fever=1\n";
        wasTutorial = false;
        if (tutorialDeparture) {
            if (!stageEntryPresentation_.HasStageDepartureForProbe() ||
                stageEntryPresentation_.BriefingTime() < kStageStartOverviewDuration) {
                throw std::runtime_error("Tutorial bypassed normal departure cinematic");
            }
            file << "TUTORIAL DEPARTURE_ARRIVAL_OK normal_cinematic=1 clean_state=1\n";
            return true;
        }
    }
    static int phase = 0; // 0: 通常戦闘 1: クリア後の再挑戦待ち 2: 完了
    static int frame = 0;
    static int resultFrames = 0;
    static float longestEmptyGap = 0.0f;
    static int encounterGapCount = 0;
    static bool wasEmpty = false;
    static int overlappingEncounters = 0;
    static int maximumStageEnemies = 0;
    static int maximumStagePressure = 0;
    static size_t previousScheduledEnemies = 0;
    static const char* previousCombatBeat = "Intro";
    static std::vector<const Enemy*> previousStageEnemies;
    static std::vector<float> pausedState;
    static int delayedEscapeEvents = 0;
    static bool resumeVerified = false;
    static bool retryFlightSeen = false;
    static unsigned int sceneryCoverage = 0;
    static unsigned int sceneryPreviewSeen = 0;
    static bool bossApronSeen = false;
    static bool flightReleaseSeen = false;
    static float maximumFlightSpeed = 0.0f;
    static float minimumCameraBank = 0.0f;
    static float maximumCameraBank = 0.0f;
    static float maximumFlightBlur = 0.0f;
    // 同じ敵・ダメージ・入力経路で、連射のみとリングを狙う攻略を比較する。製品版には含めない。
    static const int balanceProfile = [] {
        char value[16]{};
        GetEnvironmentVariableA("AZRAID_BALANCE_PROFILE", value, sizeof(value));
        return std::string_view(value) == "rapid" ? 1 : std::string_view(value) == "route" ? 2 :
            std::string_view(value) == "reckless" ? 3 : 0;
    }();
    static const int sceneryPreviewMode = [] {
        char* value = nullptr;
        size_t length = 0;
        const int mode = _dupenv_s(&value, &length, "CG2_SCENERY_PREVIEW") == 0 && value ?
            std::atoi(value) : 0;
        std::free(value);
        return mode;
    }();
    const bool sceneryPreview = sceneryPreviewMode > 0 && sceneryPreviewMode != 8;
    struct ScenerySnapshot {
        Math::Vector3 position{};
        Math::Vector3 scale{};
        bool visible = false;
    };
    static std::vector<ScenerySnapshot> previousScenery;
    const auto log = [&](const std::string& message) {
        std::ofstream file(logPath, std::ios::app);
        file << "PLAYTHROUGH " << message << '\n';
    };
    if (!input_ || !player_ || IsTutorial()) {
        throw std::runtime_error("Playthrough requires initialized main game");
    }
    std::array<BYTE, 256> keys{};
    Math::Vector2 mouse = input_->GetMousePosition();
    if (phase == 1 && !isGameClear_) {
        // ワイプと短い再出撃中は戦闘を進めず、機体とカメラの前進だけを許可する。
        if (stageFlight_.Schedule().HandoffFrames() == 0.0f) {
            if (stageEntryPresentation_.RetryTimeForProbe() > 0.0f) { retryFlightSeen = true; }
            const float flightTime = stageEntryPresentation_.IsRetryActive() ?
                stageEntryPresentation_.RetryTimeForProbe() :
                (retryFlightSeen ? kRetryEntryDuration : 0.0f);
            const float expectedRail = stageEntryPresentation_.RetryStartRailForProbe() + stageFlight_.Speed() * 60.0f * flightTime;
            if (gameplayElapsedSeconds_ != 0.0f || stageFlight_.Schedule().Progress() != 0.0f ||
                std::abs(stageFlight_.Distance() - expectedRail) > 0.001f || !stageEnemies_.Active().empty() ||
                !projectiles_.PlayerPool().Active().empty() || !projectiles_.EnemyPool().Active().empty() || combatProgress_.Score() != 0 || player_->GetHp() != 100) {
                throw std::runtime_error("Combat advanced during retry wipe/flight, or flight left its expected path");
            }
            input_->SetTestFrame(keys, mouse);
            return false;
        }
        if (!retryFlightSeen || stageEntryPresentation_.IsRetryActive() ||
            player_->GetHp() != 100 || combatProgress_.Score() != 0 || rewards_.Fever().Gauge() != 0 ||
            rewards_.Fever().RemainingFrames() != 0 || rewards_.Fever().ActivationCount() != 0 ||
            playerControl_.ShotsFired() != 0 || !stageEnemies_.Active().empty() ||
            railScenery_.DefenseStartZ() != 450.0f || railScenery_.PlazaStartZ() != 866.0f ||
            stageFlight_.ReleaseKick() != 0.0f || stageFlight_.PreviousWorldScaleForProbe() != 1.0f || audio_.AccentEndForProbe() != 0.0f ||
            bossPresentation_.WasRadioRequestedForProbe() ||
            bossPresentation_.RadioRemaining() != 0.0f ||
            stageFlight_.Schedule().HandoffFrames() != 1.0f || // 再入場後の最初の通常Updateで開始カウントが一フレーム進む。
            // 最初から巡航速度で再開するため、その速度の通常ブラーだけを許容する。
            gameplayCamera_.MotionState().blurStrength > stageFlight_.SpeedRate() * 0.22f + 0.01f ||
            std::abs(gameplayCamera_.MotionState().acceleration) > 0.03f ||
            (stageFlight_.Schedule().EmittedEnemyCount() > 0)) {
            throw std::runtime_error("Retry did not reset gameplay state");
        }
        // 再入場の最初のUpdateは既に通っているので、新しい通常曲が一つだけ再生される。
        if (!audio_.SoundForProbe() || audio_.SoundForProbe()->GetVoiceCount() != 29 || audio_.SoundForProbe()->GetPlayCount() != 0 ||
            audio_.SoundForProbe()->GetLoopCount() != 1 || audio_.TrackForProbe() != GameplayAudio::Track::Stage) {
            throw std::runtime_error("Retry did not recreate the bounded audio bank");
        }
        if (!stageResults_.RecordsForProbe() || stageResults_.RecordsForProbe()->Get().clearCount != 1 || stageResults_.RecordCommittedForProbe()) {
            throw std::runtime_error("Retry lost the saved clear record or retained the previous result flag");
        }
        log("RECORD_RETRY_OK clear_count=1 result_flag_reset=1");
        log("RETRY_RESET_OK hp=100 score=0 fever=0 voices=29 music_loops=1 track=stage scenery=avenue camera_inertia_reset=1 sfx_priority_reset=1 arrival_combat_frozen=1 short_entry_verified=1");
        phase = 2;
        input_->SetTestFrame(keys, mouse);
        return true;
    }
    if (phase == 2) {
        return true;
    }
    // 通常プレイを最後まで通し、街区の到達と、表示中の建物が変形しないことを検証する。
    const auto district = railScenery_.DistrictWeights(stageFlight_.Distance() + 48.0f);
    const unsigned int districtBit = district.y > 0.98f ? 4u :
        (district.x > 0.98f && district.y < 0.02f ? 2u : (district.x < 0.02f ? 1u : 0u));
    if (districtBit != 0 && (sceneryCoverage & districtBit) == 0) {
        sceneryCoverage |= districtBit;
        log("SCENERY_DISTRICT bit=" + std::to_string(districtBit) +
            " stage=" + std::to_string(stageFlight_.Schedule().Progress()) + " rail=" + std::to_string(stageFlight_.Distance()));
    }
    if (previousScenery.empty()) { previousScenery.resize(railScenery_.Objects().size()); }
    if (previousScenery.size() != railScenery_.Objects().size()) {
        throw std::runtime_error("Scenery object count changed during play");
    }
    if (std::count_if(railScenery_.Objects().begin(), railScenery_.Objects().end(),
        [](const auto& object) { return object.isTrackside; }) != 18) {
        throw std::runtime_error("Flight trackside pool must contain exactly eighteen shared modules");
    }
    // 地表の実際の表示範囲を毎フレーム検証する。街区到達やビルド成功では穴を検出できない。
    constexpr float kTerrainSurfaceY = -3.5f;
    std::array<Math::Vector2, 4> terrainIntervals{};
    size_t terrainCount = 0;
    for (const auto& scenery : railScenery_.Objects()) {
        if (scenery.isRoad && scenery.object && scenery.isVisible &&
            kTerrainSurfaceY >= scenery.object->GetTranslate().y +
                scenery.boundsMin.y * scenery.object->GetScale().y - 0.05f) {
            throw std::runtime_error("Terrain overlaps the road's lowest surface; depth fighting possible");
        }
        if (!scenery.isTerrain || !scenery.object || !scenery.isVisible) { continue; }
        if (terrainCount >= terrainIntervals.size() || scenery.object->HasTransparentMaterials() ||
            scenery.halfDepth != 128.0f || std::abs(scenery.object->GetTranslate().y - kTerrainSurfaceY) > 0.001f) {
            throw std::runtime_error("Terrain is not an opaque, flush, bounded ground plane");
        }
        const float z = scenery.object->GetTranslate().z;
        terrainIntervals[terrainCount++] = { z - scenery.halfDepth, z + scenery.halfDepth };
    }
    std::sort(terrainIntervals.begin(), terrainIntervals.begin() + terrainCount,
        [](const auto& a, const auto& b) { return a.x < b.x; });
    float coveredUntil = stageFlight_.Distance() - 40.0f;
    for (size_t index = 0; index < terrainCount; ++index) {
        const auto& interval = terrainIntervals[index];
        if (interval.y < coveredUntil) { continue; }
        if (interval.x > coveredUntil + 0.01f) {
            throw std::runtime_error("Visible ground has a gap between terrain tiles");
        }
        coveredUntil = (std::max)(coveredUntil, interval.y);
    }
    if (coveredUntil < stageFlight_.Distance() + 500.0f) {
        throw std::runtime_error("Terrain ended before the camera's visible distance");
    }
    // 実カメラの左右端・上下端・中央から地面へ向かう視線も確認する。
    // カメラの傾き/FOVが変わっても、画面の下や横に空が抜ける場所を作らない。
    const auto inverseViewProjection = Math::Inverse(camera_->GetViewProjectionMatrix());
    const auto unproject = [&](float x, float y, float z) {
        const auto& m = inverseViewProjection.m;
        const float w = x * m[0][3] + y * m[1][3] + z * m[2][3] + m[3][3];
        if (std::abs(w) < 0.000001f) { throw std::runtime_error("Ground ray has an invalid camera projection"); }
        return Math::Vector3{
            (x * m[0][0] + y * m[1][0] + z * m[2][0] + m[3][0]) / w,
            (x * m[0][1] + y * m[1][1] + z * m[2][1] + m[3][1]) / w,
            (x * m[0][2] + y * m[1][2] + z * m[2][2] + m[3][2]) / w };
    };
    for (float x : { -1.0f, 0.0f, 1.0f }) {
        for (float y : { -1.0f, 0.0f, 1.0f }) {
            const auto nearPoint = unproject(x, y, 0.0f);
            const auto farPoint = unproject(x, y, 1.0f);
            const float heightDifference = farPoint.y - nearPoint.y;
            if (std::abs(heightDifference) < 0.001f) { continue; }
            const float t = (kTerrainSurfaceY - nearPoint.y) / heightDifference;
            if (t < 0.0f || t > 1.0f) { continue; }
            const Math::Vector3 hit{
                nearPoint.x + (farPoint.x - nearPoint.x) * t,
                nearPoint.y + (farPoint.y - nearPoint.y) * t,
                nearPoint.z + (farPoint.z - nearPoint.z) * t };
            const bool groundCovered = std::abs(hit.x) <= 800.0f &&
                std::any_of(railScenery_.Objects().begin(), railScenery_.Objects().end(), [&](const auto& scenery) {
                    if (!scenery.isTerrain || !scenery.object || !scenery.isVisible || !scenery.isInView) { return false; }
                    const float z = scenery.object->GetTranslate().z;
                    return hit.z >= z - scenery.halfDepth - 0.01f && hit.z <= z + scenery.halfDepth + 0.01f;
                });
            if (!groundCovered) {
                throw std::runtime_error("Camera saw sky through an uncovered ground ray");
            }
        }
    }
    maximumFlightSpeed = (std::max)(maximumFlightSpeed, stageFlight_.Speed());
    minimumCameraBank = (std::min)(minimumCameraBank, gameplayCamera_.FollowPose().rotation.z);
    maximumCameraBank = (std::max)(maximumCameraBank, gameplayCamera_.FollowPose().rotation.z);
    maximumFlightBlur = (std::max)(maximumFlightBlur, gameplayCamera_.MotionState().blurStrength);
    if (!std::isfinite(gameplayCamera_.FollowPose().rotation.x) || !std::isfinite(gameplayCamera_.FollowPose().rotation.y) || !std::isfinite(gameplayCamera_.FollowPose().rotation.z) ||
        std::abs(gameplayCamera_.FollowPose().rotation.z) > 0.18f || std::abs(gameplayCamera_.FollowPose().rotation.x) > 0.25f ||
        !std::isfinite(gameplayCamera_.MotionState().blurStrength) || gameplayCamera_.MotionState().blurStrength < 0.0f ||
        gameplayCamera_.MotionState().blurStrength > 0.851f) {
        throw std::runtime_error("Flight inertia/blur left its readable envelope");
    }
    flightReleaseSeen |= stageFlight_.ReleaseKick() > 0.5f;
    if (frame > 120 && (!std::isfinite(gameplayCamera_.FollowPose().fov) || gameplayCamera_.FollowPose().fov < 0.65f || gameplayCamera_.FollowPose().fov > 1.15f ||
        stageFlight_.Distance() - gameplayCamera_.FollowPose().position.z < 8.0f || stageFlight_.Distance() - gameplayCamera_.FollowPose().position.z > 20.5f)) {
        throw std::runtime_error("Flight camera left its readable FOV/distance envelope");
    }
    for (size_t i = 0; i < railScenery_.Objects().size(); ++i) {
        const auto& scenery = railScenery_.Objects()[i];
        if (scenery.isTrackside && (scenery.halfDepth != 18.0f || scenery.loopLength != 648.0f)) {
            throw std::runtime_error("Trackside wrap does not include full module depth");
        }
        if (scenery.isLandmark && scenery.object) {
            const auto position = scenery.object->GetTranslate();
            if (std::abs(position.z - scenery.anchor.z) > 0.001f ||
                std::abs(scenery.currentLocalZ + stageFlight_.Distance() - scenery.anchor.z) > 0.001f ||
                scenery.loopLength != 0.0f || scenery.halfDepth < 53.7f) {
                throw std::runtime_error("Landmark recycled or culled without its full bounds");
            }
        }
        if (!scenery.object) { continue; }
        const float bossApronStartZ = railScenery_.BossApronStartZ();
        if (scenery.isVisible && bossApronStartZ != -1.0f &&
            (scenery.isRoad || scenery.isTrackside || scenery.isRoadDetail) &&
            scenery.object->GetTranslate().z + (scenery.isRoad ? 9.0f : scenery.halfDepth) >= bossApronStartZ) {
            throw std::runtime_error("Car road or trackside equipment continued into boss apron");
        }
        if (scenery.isBossApron && scenery.isVisible &&
            std::abs(scenery.object->GetTranslate().z - stageFlight_.Distance()) < scenery.halfDepth) {
            bossApronSeen = true;
        }
        if (!scenery.isBuilding && !scenery.isRoad && !scenery.isDefenseDistrict &&
            !scenery.isBossApron && !scenery.isTerrain) { continue; }
        const ScenerySnapshot current{ scenery.object->GetTranslate(), scenery.object->GetScale(), scenery.isVisible };
        const auto& previous = previousScenery[i];
        if (frame > 1 && !previous.visible && current.visible &&
            current.position.z - scenery.halfDepth < stageFlight_.Distance() + 500.0f &&
            current.position.z + scenery.halfDepth > stageFlight_.Distance() - 40.0f) {
            throw std::runtime_error("Scenery appeared inside the visible flight corridor");
        }
        if ((scenery.isDefenseDistrict || scenery.isBossApron || scenery.isTerrain) && previous.visible && current.visible &&
            std::abs(current.position.z - previous.position.z) > 1.0f) {
            throw std::runtime_error("Aviation district recycled while visible");
        }
        if (previous.visible && current.visible && std::abs(current.position.z - previous.position.z) < 0.05f &&
            (std::abs(current.position.x - previous.position.x) > 0.01f ||
                std::abs(current.scale.x - previous.scale.x) > 0.01f ||
                std::abs(current.scale.y - previous.scale.y) > 0.01f ||
                std::abs(current.scale.z - previous.scale.z) > 0.01f)) {
            throw std::runtime_error("Visible scenery moved sideways or changed scale");
        }
        // 6mの元モデルを18mにして進行方向へ接続する。街区の幅変更で長さを変えない。
        if (scenery.isRoad && (std::abs(current.scale.x * 6.0f - 18.0f) > 0.01f ||
            std::abs(scenery.object->GetRotate().y - 1.57079632679f) > 0.001f)) {
            throw std::runtime_error("Road orientation or segment connection length changed");
        }
        if (scenery.isBuilding && std::abs(current.position.x) < 31.49f) {
            throw std::runtime_error("Building entered central combat corridor");
        }
        previousScenery[i] = current;
    }
    // 明示指定されたDebug試験だけ、各街区で画面確認用に停止する。通常起動/Releaseには入らない。
    // モード2は新施設の接近・通過・退出を同じ通常プレイ経路で確認する。
    // モード3は左右・下降・フィーバーのカメラを通常入力で確認する専用プレビュー。
    // モード4は中盤の敷地への進入・中央・退出を通常プレイの同じ経路で確認する。
    // モード5は広場への進入・広場内・実際のボス戦を通常経路で確認する。
    const float defenseStartZ = railScenery_.DefenseStartZ() >= 0.0f ?
        (std::max)(railScenery_.DefenseStartZ(), 450.0f) : railScenery_.DefenseStartZ();
    const float bossApronStartZ = railScenery_.BossApronStartZ();
    unsigned int previewBit = sceneryPreviewMode == 3 ?
        (frame == 351 ? 1u : frame == 401 ? 2u : frame == 451 ? 4u :
            rewards_.Fever().IsActive() && gameplayCamera_.MotionState().blurStrength > 0.32f ? 8u : 0u) : sceneryPreviewMode == 2 ?
        (stageFlight_.Distance() >= 325.0f ? 8u : stageFlight_.Distance() >= 245.0f ? 4u :
            stageFlight_.Distance() >= 185.0f ? 2u : stageFlight_.Distance() >= 100.0f ? 1u : 0u) : sceneryPreviewMode == 4 ?
        (railScenery_.PlazaStartZ() != -1.0f && stageFlight_.Distance() >= railScenery_.PlazaStartZ() + 80.0f ? 4u :
            railScenery_.DefenseStartZ() != -1.0f && stageFlight_.Distance() >= defenseStartZ + 140.0f ? 2u :
            railScenery_.DefenseStartZ() != -1.0f && stageFlight_.Distance() >= defenseStartZ - 60.0f ? 1u : 0u) : sceneryPreviewMode == 5 ?
        (stageEnemies_.BossSpawned() && !bossPresentation_.IsIntroPlaying() ? 4u :
            bossApronStartZ != -1.0f && stageFlight_.Distance() >= bossApronStartZ + 90.0f ? 2u :
            bossApronStartZ != -1.0f && stageFlight_.Distance() >= bossApronStartZ - 40.0f ? 1u : 0u) : districtBit;
    if (sceneryPreviewMode == 6) {
        // 全区間の細部を一度の通常プレイで確認。停止点は既存の画面確認専用の経路だけで使う。
        previewBit = 0;
        if (stageFlight_.Distance() >= 180.0f && !(sceneryPreviewSeen & 1u)) { previewBit = 1u; }
        else if (railScenery_.DefenseStartZ() != -1.0f && stageFlight_.Distance() >= defenseStartZ + 40.0f &&
            !(sceneryPreviewSeen & 2u)) { previewBit = 2u; }
        else if (railScenery_.DefenseStartZ() != -1.0f && stageFlight_.Distance() >= defenseStartZ + 175.0f &&
            !(sceneryPreviewSeen & 4u)) { previewBit = 4u; }
        else if (bossApronStartZ != -1.0f && stageFlight_.Distance() >= bossApronStartZ - 40.0f &&
            !(sceneryPreviewSeen & 8u)) { previewBit = 8u; }
        else if (stageEnemies_.BossSpawned() && !bossPresentation_.IsIntroPlaying() && !(sceneryPreviewSeen & 16u)) { previewBit = 16u; }
        else if (bossApronStartZ != -1.0f && stageFlight_.Distance() >= bossApronStartZ + 245.0f &&
            !(sceneryPreviewSeen & 32u)) { previewBit = 32u; }
    }
    if (sceneryPreviewMode == 7) {
        // 接続部の直前・境界・直後を見比べ、点の静止画で隙間を見落とさない。
        previewBit = 0;
        const std::array<float, 8> stops{ 20.0f, defenseStartZ - 15.0f, defenseStartZ,
            defenseStartZ + 15.0f, bossApronStartZ - 15.0f, bossApronStartZ,
            bossApronStartZ + 15.0f, bossApronStartZ + 270.0f };
        for (size_t index = 0; index < stops.size(); ++index) {
            const unsigned int bit = 1u << index;
            if (stageFlight_.Distance() >= stops[index] && !(sceneryPreviewSeen & bit)) {
                previewBit = bit;
                break;
            }
        }
    }
    if (sceneryPreview && frame > 30 && previewBit != 0 && (sceneryPreviewSeen & previewBit) == 0) {
        sceneryPreviewSeen |= previewBit;
        log("SCENERY_PREVIEW rail=" + std::to_string(stageFlight_.Distance()));
        phantomPreviewPaused_ = true;
    }
    if (sceneryPreview && phantomPreviewPaused_) {
        ImGui::SetNextWindowPos({ 20.0f, 155.0f }, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ 180.0f, 72.0f }, ImGuiCond_Always);
        ImGui::Begin("Scenery QA", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
        const bool next = ImGui::Button("NEXT / F8") || ImGui::IsKeyPressed(ImGuiKey_F8, false);
        ImGui::End();
        if (next) { phantomPreviewPaused_ = false; }
        input_->SetTestFrame({}, mouse);
        return false;
    }
    if (isGameOver_) {
        if (balanceProfile == 3) {
            if (combatResolver_.Stats().playerDamage < 4 || isGameClear_ || !player_->IsDead()) {
                throw std::runtime_error("Stationary-fire test did not exercise real damage and defeat");
            }
            log("CHALLENGE_PASS stationary_fire_defeated=1 aim_assist_unchanged=1 no_move_dodge_skill=1 damage_hits=" +
                std::to_string(combatResolver_.Stats().playerDamage) + " progress=" + std::to_string(stageFlight_.Schedule().Progress()) +
                " seconds=" + std::to_string(gameplayElapsedSeconds_));
            input_->SetTestFrame({}, mouse);
            return true;
        }
        log("FAIL player_dead progress=" + std::to_string(stageFlight_.Schedule().Progress()));
        throw std::runtime_error("Playthrough pilot was defeated; no invulnerability or forced clear used");
    }
    if (frame == 0) {
        CheckSceneObjectRoundTrip(*object3dCommon_, logPath);
        log("SCENE_OBJECTS_ROUND_TRIP_OK valid_models=1 failed_load_preserved=1 save_updated_transform=1");
        // 本編の四機編隊が分断されず、一機残しから入場できることを実アクターで検証。
        StageEnemies admission;
        admission.Prepare(*object3dCommon_, *ModelManager::GetInstance());
        const auto addFixture = [&](Enemy::Behavior behavior) {
            auto* enemy = admission.SpawnForProbe({ 0.0f, 1.0f, stageFlight_.Distance() + 44.0f },
                behavior, Enemy::EntryStyle::Direct);
            if (!enemy) { throw std::runtime_error("Could not create an enemy for the admission test"); }
            return enemy;
        };
        const auto canAdmit = [&](size_t eventIndex) {
            return stageFlight_.Schedule().CanAdmitGroup(eventIndex, admission.Load());
        };
        addFixture(Enemy::Behavior::Formation);
        if (!canAdmit(11)) { throw std::runtime_error("Last light enemy prevented sniper-wing overlap"); }
        auto* second = addFixture(Enemy::Behavior::Formation);
        if (canAdmit(11)) { throw std::runtime_error("Four-ship group admitted without all four slots"); }
        second->Kill();
        if (!canAdmit(11)) { throw std::runtime_error("Destroyed enemy retained an admission slot"); }
        admission.Reset(EnemyTargetReferences());
        addFixture(Enemy::Behavior::Sniper);
        if (canAdmit(27) || canAdmit(28)) {
            throw std::runtime_error("Sniper overlapped another primary threat or crossfire pair");
        }
        if (!canAdmit(0)) { throw std::runtime_error("Primary threat prevented light reinforcements"); }

        // 一機の撤退が一度だけ集計され、出現枠が空くことを通常更新で確認する。
        admission.Reset(EnemyTargetReferences());
        addFixture(Enemy::Behavior::Formation);
        const StageEnemies::Frame enemyFrame{ stageFlight_.Distance(), 1.0f, false, false, 0.0f, bossAttack_ };
        int escapeEvents = 0;
        for (int step = 0; step < 1200; ++step) {
            if (admission.Update(enemyFrame, EnemyTargetReferences())) { ++escapeEvents; }
        }
        if (escapeEvents != 1 || admission.EscapedCount() != 1 || !admission.Active().empty() ||
            admission.Load().count != 0) {
            throw std::runtime_error("Escaped enemy did not release its slot exactly once");
        }
        log("ENEMY_ESCAPE_OK counted_once=1 slot_released=1");
        previousStageEnemies.reserve(5);
        log("FLOW_ADMISSION_OK last_enemy_overlap=1 whole_formation_reserved=1 dead_slots_released=1 primary_threat_bounded=1");
        CheckEnemyFireControl();
        CheckSniperPosture(object3dCommon_.get(), stageEnemies_.ShooterModelForProbe());
        log("SNIPER_POSTURE_OK brace_still=1 rail_relative=1 recoil_on_shot=1 recovery_open=1 death_cancels=1");
        log("ENEMY_FIRE_CONTROL_OK rail_intercept=1 fixed_aim=1 windup_burst_recovery=1 slow_steps=0.25,0.5,1");
        if (!audio_.SoundForProbe() || audio_.SoundForProbe()->GetVoiceCount() != 29) {
            throw std::runtime_error("Combat audio bank failed to load all 28 voices");
        }
        log("START normal_damage=1 normal_collisions=1 audio_voices=29");
    }
    ++frame;
    // 実際のESC経路で停止し、各フレームの進行値と全アクター位置が不変であることを検証する。
    const auto snapshot = [&]() {
        std::vector<float> values{ gameplayElapsedSeconds_, stageFlight_.Schedule().Progress(), stageFlight_.Distance(), gameplayCamera_.Clock(),
            stageFlight_.Schedule().HandoffFrames(), stageFlight_.Schedule().EmptyFrames(), stageFlight_.Schedule().TimelineSpeed(),
            stageFlight_.ReleaseKick(), stageFlight_.PreviousWorldScaleForProbe(), gameplayCamera_.FollowPose().fov, audio_.AccentEndForProbe(),
            gameplayCamera_.MotionState().fovVelocity, gameplayCamera_.MotionState().acceleration,
            gameplayCamera_.MotionState().previousSpeed, gameplayCamera_.MotionState().blurStrength,
            static_cast<float>(playerControl_.Shooting().ChargeFrames()), static_cast<float>(rewards_.Fever().RemainingFrames()), static_cast<float>(rewards_.Fever().Gauge()), playerSkill_.CooldownFrames(),
            static_cast<float>(bossPresentation_.HasRadio()), bossPresentation_.RadioRemaining(),
            static_cast<float>(bossPresentation_.WasRadioRequestedForProbe()),
            static_cast<float>(combatProgress_.Score()), static_cast<float>(player_->GetHp()),
            static_cast<float>(stageEnemies_.Active().size()), static_cast<float>(projectiles_.PlayerPool().Active().size()),
            static_cast<float>(projectiles_.EnemyPool().Active().size()), static_cast<float>(playerControl_.ShotsFired()) };
        const auto position = [&](const Math::Vector3& p) { values.insert(values.end(), { p.x, p.y, p.z }); };
        position(player_->GetTranslate());
        position(gameplayCamera_.FollowPose().position);
        position(gameplayCamera_.FollowPose().rotation);
        position(gameplayCamera_.MotionState().translationVelocity);
        position(gameplayCamera_.MotionState().rotationVelocity);
        for (const auto& enemy : stageEnemies_.Active()) { position(enemy->GetTranslate()); }
        for (const auto& bullet : projectiles_.PlayerPool().Active()) { position(bullet->GetTranslate()); }
        for (const auto& bullet : projectiles_.EnemyPool().Active()) { position(bullet->GetTranslate()); }
        return values;
    };
    if (frame == 721) {
        if (!gameplayMenu_.IsPaused() || gameplayMenu_.IsControlsVisible()) { throw std::runtime_error("ESC did not open pause menu"); }
        pausedState = snapshot();
    }
    if (frame > 721 && frame <= 840 && (!gameplayMenu_.IsPaused() || snapshot() != pausedState)) {
        throw std::runtime_error("Gameplay changed while pause menu was open");
    }
    if (frame >= 841 && !resumeVerified) {
        if (snapshot() != pausedState) { throw std::runtime_error("Gameplay advanced before resume transition finished"); }
        if (gameplayMenu_.IsPaused()) {
            if (gameplayMenu_.PendingAction() != GameplayMenu::Action::Resume) { throw std::runtime_error("ESC did not begin resume transition"); }
        } else {
            if (delayedEscapeEvents != 2) { throw std::runtime_error("Delayed UI input fixture did not execute"); }
            resumeVerified = true;
            log("PAUSE_OK frames=120 time_rail_actors_bullets_hp_charge_fever_skill_frozen=1 resume_transition_frozen=1 resume=1");
            log("MENU_INPUT_OK escape_hold=31_frames delayed_ui_duplicates_ignored=2 wasd_arrows_hold=1");
        }
    }
    if ((frame >= 720 && frame <= 750) || (frame >= 840 && frame <= 860)) { keys[DIK_ESCAPE] = 0x80; }
    // 物理キーとUIイベントが別フレームに届く状況を再現。旧OR判定はここでポーズを再度切り替える。
    if (frame == 722 || frame == 780) { ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, true); }
    if (frame == 725 || frame == 782) { ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false); }
    if (frame > 720 && frame < 840 && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) { ++delayedEscapeEvents; }
    if (frame >= 735 && frame <= 744) { keys[DIK_S] = 0x80; }
    if (frame >= 755 && frame <= 764) { keys[DIK_W] = 0x80; }
    if (frame >= 775 && frame <= 784) { keys[DIK_DOWN] = 0x80; }
    if (frame >= 805 && frame <= 814) { keys[DIK_UP] = 0x80; }
    if (frame == 739) { ImGui::GetIO().AddKeyEvent(ImGuiKey_S, true); }
    if (frame == 742) { ImGui::GetIO().AddKeyEvent(ImGuiKey_S, false); }
    if (frame == 778) { ImGui::GetIO().AddKeyEvent(ImGuiKey_DownArrow, true); }
    if (frame == 781) { ImGui::GetIO().AddKeyEvent(ImGuiKey_DownArrow, false); }
    if (frame > 735 && frame <= 840) {
        const int expected = frame <= 755 || (frame > 775 && frame <= 805) ? 1 : 0;
        if (gameplayMenu_.SelectedPauseItem() != expected) { throw std::runtime_error("Pause selection repeated during key hold or delayed UI event"); }
    }
    if (frame > 841 && frame <= 900 && resumeVerified && gameplayMenu_.IsPaused()) { throw std::runtime_error("Held ESC reopened pause after resume"); }
    if (frame == 940 && !resumeVerified) { throw std::runtime_error("Resume transition did not finish"); }
    const bool empty = !stageEnemies_.BossSpawned() && stageEnemies_.Active().empty() && combatResolver_.Stats().defeated > 0;
    if (empty && stageFlight_.Schedule().HasPendingEnemies()) {
        longestEmptyGap = (std::max)(longestEmptyGap, stageFlight_.Schedule().EmptyFrames());
        if (stageFlight_.Schedule().EmptyFrames() > 14.0f) {
            throw std::runtime_error("Empty encounter gap exceeded the continuous-flow deadline: frames=" +
                std::to_string(stageFlight_.Schedule().EmptyFrames()) + " stage=" + std::to_string(stageFlight_.Schedule().Progress()) +
                " speed=" + std::to_string(stageFlight_.Schedule().TimelineSpeed()) + " slow=" +
                std::to_string(GetCinematicWorldTimeScale()) + " handoff=" +
                std::to_string(stageFlight_.Schedule().HandoffFrames()));
        }
    }
    if (wasEmpty && !empty) { ++encounterGapCount; }
    wasEmpty = empty;
    int activeStageEnemies = 0;
    int pressure = 0;
    bool retainedEnemy = false;
    for (const auto& enemy : stageEnemies_.Active()) {
        if (!enemy || enemy->IsDead() || enemy->IsBoss()) { continue; }
        ++activeStageEnemies;
        const auto behavior = enemy->GetBehavior();
        pressure += behavior == Enemy::Behavior::Sniper || behavior == Enemy::Behavior::Shield ||
            behavior == Enemy::Behavior::Support ? 2 :
            behavior == Enemy::Behavior::Crossfire || behavior == Enemy::Behavior::StrafeShooter ? 1 : 0;
        retainedEnemy |= std::find(previousStageEnemies.begin(), previousStageEnemies.end(), enemy.get()) !=
            previousStageEnemies.end();
    }
    maximumStageEnemies = (std::max)(maximumStageEnemies, activeStageEnemies);
    maximumStagePressure = (std::max)(maximumStagePressure, pressure);
    if (activeStageEnemies > 5 || pressure > 2) {
        throw std::runtime_error("Continuous flow exceeded the live enemy or attack-pressure bound");
    }
    const size_t scheduledEnemies = static_cast<size_t>(stageFlight_.Schedule().EmittedEnemyCount());
    if (scheduledEnemies > previousScheduledEnemies && retainedEnemy &&
        std::string_view(previousCombatBeat) != std::string_view(stageFlight_.CombatBeat())) {
        ++overlappingEncounters;
        log("FLOW_OVERLAP beat=" + std::string(stageFlight_.CombatBeat()) +
            " active=" + std::to_string(activeStageEnemies));
    }
    previousStageEnemies.clear();
    for (const auto& enemy : stageEnemies_.Active()) {
        if (enemy && !enemy->IsDead() && !enemy->IsBoss()) { previousStageEnemies.push_back(enemy.get()); }
    }
    previousScheduledEnemies = scheduledEnemies;
    previousCombatBeat = stageFlight_.CombatBeat();
    if (frame == 61 && (!gameplayMenu_.IsPaused() || gameplayMenu_.PendingAction() != GameplayMenu::Action::Controls)) {
        throw std::runtime_error("H did not open controls help");
    }
    if (frame == 301 && (!gameplayMenu_.IsPaused() || gameplayMenu_.PendingAction() != GameplayMenu::Action::Resume)) {
        throw std::runtime_error("H did not close controls help");
    }
    if (frame == 61) {
        log("HELP_OPEN_OK");
    } else if (frame == 301) {
        log("HELP_CLOSE_OK");
    }
    if (frame == 60 || frame == 300) {
        keys[DIK_H] = 0x80;
    }
    if (frame >= 720 && frame < 840) {
        input_->SetTestFrame(keys, mouse);
        return false; // この区間はメニュー試験のキーだけを入力する。
    }
    if (isGameClear_) {
        if (balanceProfile == 3) { throw std::runtime_error("Stationary continuous fire still cleared the entire stage"); }
        if (!stageResults_.IsWaiting()) {
            // この区間はキーの立ち上がりを試す。実際のマウス移動によるホバー選択を混ぜない。
            ImGui::GetIO().MouseDelta = { 0, 0 };
            if (resultFrames == 0) { stageResults_.ResetSelection(); }
            if (resultFrames == 5) {
                if (!HasResultPresentation() ||
                    &GetProjectionMatrix() != &stageResults_.Presentation().GetProjectionMatrix()) {
                    throw std::runtime_error("Clear result did not use its dedicated camera");
                }
                log("CLEAR_PRESENTATION_OK dedicated_camera=1");
                auto recordPath = std::filesystem::path(logPath);
                recordPath += L".record";
                StageRecords reloaded;
                if (!stageResults_.RecordsForProbe() || !stageResults_.RecordCommittedForProbe() || !stageResults_.RecordSavedForProbe() ||
                    !reloaded.Load(recordPath) || reloaded.Get().clearCount != 1 ||
                    reloaded.Get().bestScore != combatProgress_.Score() ||
                    reloaded.Get().bestRank != StageRecords::EvaluateRank(combatProgress_.Score())) {
                    throw std::runtime_error("Clear result did not persist the actual score and rank exactly once");
                }
                log("RECORD_SAVE_OK once_per_clear=1 score_matches=1 rank_matches=1 reload=1");
                log("BALANCE_RESULT profile=" + std::to_string(balanceProfile) +
                    " score=" + std::to_string(combatProgress_.Score()) +
                    " hp=" + std::to_string(player_->GetHp()) +
                    " seconds=" + std::to_string(gameplayElapsedSeconds_) +
                    " rings=" + std::to_string(ringGates_.PassCount()) + "/" + std::to_string(ringGates_.SpawnCount()) +
                    " escaped=" + std::to_string(stageEnemies_.EscapedCount()) +
                    " skills=" + std::to_string(playerSkill_.ActivationCount()));
            }
            constexpr BYTE directKeys[] = { DIK_RIGHT, DIK_LEFT, DIK_DOWN, DIK_UP, DIK_D, DIK_A, DIK_S, DIK_W };
            constexpr ImGuiKey uiKeys[] = { ImGuiKey_RightArrow, ImGuiKey_LeftArrow, ImGuiKey_DownArrow, ImGuiKey_UpArrow,
                ImGuiKey_D, ImGuiKey_A, ImGuiKey_S, ImGuiKey_W };
            const int action = resultFrames / 30;
            const int offset = resultFrames % 30;
            if (action < 8) {
                if (offset < 10) { keys[directKeys[action]] = 0x80; }
                if (offset == 3) { ImGui::GetIO().AddKeyEvent(uiKeys[action], true); }
                if (offset == 5) { ImGui::GetIO().AddKeyEvent(uiKeys[action], false); }
                if (offset > 0 && stageResults_.MenuForProbe().SelectedItem() != (action + 1) % 2) {
                    throw std::runtime_error("Result selection repeated during key hold or delayed UI event: action=" +
                        std::to_string(action) + " offset=" + std::to_string(offset) + " selected=" + std::to_string(stageResults_.MenuForProbe().SelectedItem()));
                }
            }
            if (resultFrames == 240) { log("RESULT_KEYS_OK all_8_keys_hold=1 delayed_ui_duplicates_ignored=1"); }
            if (resultFrames >= 242 && resultFrames <= 246) { keys[DIK_TAB] = 0x80; }
            if (resultFrames >= 243 && resultFrames <= 247 && !stageResults_.MenuForProbe().IsDetailsOpen()) {
                throw std::runtime_error("Result details did not open once during held TAB");
            }
            if (resultFrames == 250) { keys[DIK_TAB] = 0x80; }
            if (resultFrames == 251) {
                if (stageResults_.MenuForProbe().IsDetailsOpen()) { throw std::runtime_error("Result details did not return to score view"); }
                log("RESULT_DETAILS_OK tab_hold=1 return_to_score=1");
            }
        }
        if (!stageEnemies_.BossDefeated() || stageFlight_.Schedule().HasPendingEnemies()) {
            throw std::runtime_error("Clear without full schedule and boss defeat");
        }
        if (!stageResults_.IsWaiting() && ++resultFrames >= 260 && phase == 0) {
            if (bossShotsFired_ <= 0) {
                throw std::runtime_error("Boss was defeated without firing any attack");
            }
            if (rewards_.Fever().ActivationCount() <= 0) {
                throw std::runtime_error("Fever did not auto activate during normal combat");
            }
            log("FEVER_AUTO_OK no_activation_key=1");
            if (sceneryCoverage != 7u) { throw std::runtime_error("Playthrough did not reach all three scenery districts"); }
            if (!bossApronSeen) { throw std::runtime_error("Playthrough did not enter the boss apron"); }
            log("SCENERY_OK avenue_canyon_plaza=1 boss_apron=1 visible_transforms_stable=1 bounded_objects=1 landmark_world_fixed=1 terrain_opaque_continuous_540m=1 terrain_road_depth_separated=1 no_in_view_appearance=1");
            if (!flightReleaseSeen || maximumFlightSpeed < 0.5f) {
                throw std::runtime_error("Flight test did not exercise fever acceleration and slow release");
            }
            log("FLIGHT_RUSH_OK trackside_pool=18 camera_readable=1 slow_release=1 max_speed=" +
                std::to_string(maximumFlightSpeed));
            if (minimumCameraBank > -0.015f || maximumCameraBank < 0.015f || maximumFlightBlur < 0.25f) {
                throw std::runtime_error("Flight test did not exercise both banks and acceleration blur");
            }
            log("FLIGHT_CAMERA_OK bank_min=" + std::to_string(minimumCameraBank) +
                " bank_max=" + std::to_string(maximumCameraBank) + " blur_max=" + std::to_string(maximumFlightBlur) +
                " pause_freezes_springs=1");
            log("PACING_OK encounter_gaps=" + std::to_string(encounterGapCount) +
                " longest_empty_frames=" + std::to_string(longestEmptyGap));
            if (overlappingEncounters == 0) { throw std::runtime_error("Continuous flow never handed over before a wipe"); }
            if (scheduledEnemies != static_cast<size_t>(stageFlight_.Schedule().EnemyCount()) ||
                projectiles_.PlayerPool().MissCount() != 0 || projectiles_.EnemyPool().MissCount() != 0 || combatEffects_.MissCount() != 0) {
                throw std::runtime_error("Continuous flow skipped scheduled enemies or exhausted a combat pool");
            }
            log("FLOW_OK overlapping_encounters=" + std::to_string(overlappingEncounters) +
                " max_active=" + std::to_string(maximumStageEnemies) +
                " max_pressure=" + std::to_string(maximumStagePressure) + " whole_schedule=1");
            log("CLEAR hp=" + std::to_string(player_->GetHp()) +
                " profile=" + std::to_string(balanceProfile) +
                " score=" + std::to_string(combatProgress_.Score()) +
                " rank=" + std::string(1, StageRecords::EvaluateRank(combatProgress_.Score())) +
                " seconds=" + std::to_string(gameplayElapsedSeconds_) +
                " rings=" + std::to_string(ringGates_.PassCount()) + "/" + std::to_string(ringGates_.SpawnCount()) +
                " defeated=" + std::to_string(combatResolver_.Stats().defeated) +
                " escaped=" + std::to_string(stageEnemies_.EscapedCount()) +
                " shots=" + std::to_string(playerControl_.ShotsFired()) +
                " hits=" + std::to_string(combatResolver_.Stats().directHits) +
                " fever=" + std::to_string(rewards_.Fever().ActivationCount()) +
                " phantom=" + std::to_string(playerSkill_.ActivationCount()) +
                " phantom_kills=" + std::to_string(playerSkill_.DefeatCount()) +
                " boss_attacks_started=" + std::to_string(bossAttack_.AttackCount()) +
                " boss_shots=" + std::to_string(bossShotsFired_) +
                " boss_phase=" + std::to_string(bossAttack_.Phase()) +
                " pool_miss_p=" + std::to_string(projectiles_.PlayerPool().MissCount()) +
                " pool_miss_e=" + std::to_string(projectiles_.EnemyPool().MissCount()) +
                " pool_miss_fx=" + std::to_string(combatEffects_.MissCount()) +
                " max_bullets_p=" + std::to_string(projectiles_.PlayerPool().PeakActiveCount()) +
                " max_bullets_e=" + std::to_string(projectiles_.EnemyPool().PeakActiveCount()) +
                " sounds=" + std::to_string(audio_.SoundForProbe()->GetPlayCount()) +
                " voices=" + std::to_string(audio_.SoundForProbe()->GetVoiceCount()));
            keys[DIK_RETURN] = 0x80; // 選択中の「再挑戦」をEnterで決定する。
            phase = 1;
        }
        input_->SetTestFrame(keys, mouse);
        return false;
    }
    if (frame % 600 == 0) {
        log("PROGRESS stage=" + std::to_string(stageFlight_.Schedule().Progress()) +
            " rail=" + std::to_string(stageFlight_.Distance()) +
            " hp=" + std::to_string(player_->GetHp()) +
            " defeated=" + std::to_string(combatResolver_.Stats().defeated));
    }
    const Enemy* target = nullptr;
    int bestCluster = -1;
    for (const auto& enemy : stageEnemies_.Active()) {
        Math::Vector2 screen{};
        if (enemy && !enemy->IsDead() && enemy->IsTargetable() &&
            TryProjectToScreen(enemy->GetAimPosition(), screen)) {
            int cluster = 0;
            if (balanceProfile == 2 && playerControl_.Shooting().IsChargeReady() && !rewards_.Fever().IsActive()) {
                for (const auto& other : stageEnemies_.Active()) {
                    if (other && !other->IsDead() && other->IsTargetable() &&
                        DistanceSquared(enemy->GetAimPosition(), other->GetAimPosition()) <=
                        kChargeSplashRadius * kChargeSplashRadius) { ++cluster; }
                }
            }
            if (target && cluster <= bestCluster) { continue; }
            target = enemy.get();
            bestCluster = cluster;
            const ImVec2 origin = ImGui::GetMainViewport()->Pos;
            mouse = { screen.x - origin.x, screen.y - origin.y };
            if (balanceProfile != 2) { break; }
        }
    }
    // 撃破速度に依存せず最後の街区とボス敷地も検査する。そこへ通常に飛行してから反撃。
    // フィーバーを持ち込んでも、試験ではボスの初撃と回避を観察してから反撃する。
    // 通常プレイのHP・ダメージ・ボスの被弾条件には手を加えない。
    if (target && target->IsBoss() && (bossShotsFired_ == 0 || sceneryCoverage != 7u || !bossApronSeen)) {
        target = nullptr;
    }
    if (target) {
        // 硬い敵へはチャージ、それ以外には連射。性能・弾・敵HPは通常値のまま。
        bool chargeGroup = false;
        if (balanceProfile == 2 && !target->IsBoss()) {
            int neighbors = 0;
            for (const auto& other : stageEnemies_.Active()) {
                if (other && !other->IsDead() && other->IsTargetable() &&
                    DistanceSquared(target->GetAimPosition(), other->GetAimPosition()) <=
                    kChargeSplashRadius * kChargeSplashRadius) { ++neighbors; }
            }
            chargeGroup = neighbors >= 2;
        }
        if (balanceProfile == 1 || balanceProfile == 3 || (!chargeGroup && target->GetHp() < 7) ||
            playerControl_.Shooting().IsChargeReady() || rewards_.Fever().IsActive()) {
            keys[DIK_SPACE] = 0x80;
        }
    }
    const auto position = player_->GetTranslate();
    const bool edgeScan = sceneryPreviewMode == 7 || sceneryPreviewMode == 8;
    float targetX = (edgeScan ? 8.5f : 4.2f) * std::sin(static_cast<float>(frame) * 0.011f);
    float targetY = 0.7f + (edgeScan ? 2.5f : 1.0f) * std::sin(static_cast<float>(frame) * 0.017f);
    if (balanceProfile == 2) {
        const RingGates::Gate* nearest = nullptr;
        for (const auto& gate : ringGates_.Gates()) {
            if (gate.active && gate.position.z > position.z &&
                (!nearest || gate.position.z < nearest->position.z)) { nearest = &gate; }
        }
        if (nearest) { targetX = nearest->position.x; targetY = nearest->position.y; }
    }
    if (std::abs(targetX - position.x) > 0.3f) {
        keys[targetX > position.x ? DIK_D : DIK_A] = 0x80;
    }
    if (std::abs(targetY - position.y) > 0.3f) {
        keys[targetY > position.y ? DIK_W : DIK_S] = 0x80;
    }
    for (const auto& bullet : projectiles_.EnemyPool().Active()) {
        const auto bulletPosition = bullet->GetTranslate();
        const Math::Vector3 delta{ bulletPosition.x - position.x,
            bulletPosition.y - position.y, bulletPosition.z - position.z };
        if (!bullet->IsDead() && delta.z > 0.0f && delta.z < 12.0f &&
            std::abs(delta.x) < 3.0f && std::abs(delta.y) < 2.5f && frame % 2 == 0) {
            keys[DIK_LSHIFT] = 0x80;
            break;
        }
    }
    if (balanceProfile != 1 && balanceProfile != 3 && playerSkill_.IsReady() && target && frame % 2 == 0 &&
        (balanceProfile != 2 || stageEnemies_.Active().size() >= 3 || target->IsBoss() || target->GetHp() >= 7)) {
        keys[DIK_Q] = 0x80;
    }
    if (sceneryPreviewMode == 3 && frame >= 320 && frame <= 451) {
        keys[DIK_W] = keys[DIK_A] = keys[DIK_S] = keys[DIK_D] = keys[DIK_LSHIFT] = 0;
        if (frame <= 350) { keys[DIK_D] = keys[DIK_W] = 0x80; }
        else if (frame <= 400) { keys[DIK_A] = 0x80; }
        else if (frame <= 450) { keys[DIK_S] = 0x80; }
    }
    if (balanceProfile == 3) {
        keys[DIK_A] = keys[DIK_D] = keys[DIK_W] = keys[DIK_S] = keys[DIK_LSHIFT] = 0;
    }
    input_->SetTestFrame(keys, mouse);
    return false;
}

// スキルの発動条件から連撃・撃破・再使用までを、戦闘中の状態と照合する。
bool GameRuntime::RunPhantomProbe(const std::string& logPath, bool preview)
{
    static int frame = 0;
    static Bullet* removalBullet = nullptr;
    static bool retry = false;
    static std::array<uint32_t, 2> audioBefore{};
    static Math::Vector2 activationMouse{};
    static float dodgeStartX = 0.0f;
    static int feverCountBeforeAutoTest = 0;
    const auto log = [&](const std::string& message) {
        std::ofstream file(logPath, std::ios::app);
        file << "PHANTOM_TEST " << message << '\n';
    };
    const auto require = [&](bool condition, const char* message) {
        if (!condition) {
            log(std::string("FAIL ") + message);
            throw std::runtime_error(message);
        }
    };
    std::array<BYTE, 256> keys{};
    Math::Vector2 mouse{ 640.0f, 300.0f };
    static const bool hudOnlyPreview = [] {
        char value[2]{};
        return GetEnvironmentVariableA("AZRAID_HUD_PREVIEW", value, 2) > 0 && value[0] == '1';
    }();
    if (preview && hudOnlyPreview) {
        static int sample = 0;
        static int sampleFrame = 0;
        if (sampleFrame == 0) {
            DebugJumpToStagePhase(0);
            stageFlight_.DisableSpawnsForProbe();
            player_ = std::make_unique<Player>();
            player_->Initialize(object3dCommon_.get(), sceneSetup_.PlayerModelForProbe());
            player_->SetRailZ(stageFlight_.Distance());
            player_->Damage(sample == 0 ? 0 : sample == 1 ? 45 : 75);
            rewards_.Fever().SetGauge(sample == 1 ? 60 : 0);
            rewards_.Fever().SetRemainingFramesForDebug(sample == 2 ? 600 : sample == 3 ? 150 : 0);
            rewards_.Fever().SetSpeedEffectForDebug(sample >= 2 ? 1.0f : 0.0f);
            playerControl_.Shooting().SetChargeForDebug(sample >= 2 ? playerControl_.Shooting().GetTuning().chargeFrames : 0);
            playerSkill_.SetRechargeForDebug(660.0f, sample == 0 || sample == 2);
            log("HUD_COLOR_SAMPLE case=" + std::to_string(sample));
        }
        stageFlight_.HoldProgressForProbe(0.0f);
        if (sampleFrame++ >= 60) {
            phantomPreviewPaused_ = true;
            ImGui::SetNextWindowPos({ 20, 165 }, ImGuiCond_Always);
            ImGui::Begin("HUD state fixture", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
            ImGui::Text("TEST ONLY: HP / fever / cooldown   sample=%d", sample);
            const bool next = ImGui::Button("NEXT / F8", { 220, 36 }) || ImGui::IsKeyPressed(ImGuiKey_F8, false);
            ImGui::End();
            if (next) {
                phantomPreviewPaused_ = false;
                sampleFrame = 0;
                if (++sample == 4) { input_->SetTestFrame({}, mouse); log("HUD_COLOR_PREVIEW_PASS states=4"); return true; }
            }
        }
        input_->SetTestFrame({}, mouse);
        return false;
    }
    static const bool skillOnlyPreview = [] {
        char value[2]{};
        return GetEnvironmentVariableA("AZRAID_SKILL_PREVIEW", value, 2) > 0 && value[0] == '1';
    }();
    static const bool autoSkillPreview = [] {
        char value[2]{};
        return GetEnvironmentVariableA("AZRAID_SKILL_PREVIEW_AUTO", value, 2) > 0 && value[0] == '1';
    }();
    static unsigned int visualSamples = 0;
    unsigned int visualSample = 0;
    if (preview && skillOnlyPreview && IsPhantomRaidActive()) {
        const unsigned int stage = playerSkill_.IsFinished() && playerSkill_.Clock() - playerSkill_.FinisherTime() >= 1.0f ? 8u :
            !playerSkill_.IsFinished() && playerSkill_.NextStrike() >= 3 ? 4u :
            playerSkillPresentation_.FirstStrikeAgeForProbe() >= 2.0f ? 2u : playerSkill_.Clock() >= 4.0f ? 1u : 0u;
        const unsigned int candidate = stage << (playerSkill_.IsEmpowered() ? 4 : 0);
        if (!(visualSamples & candidate)) { visualSample = candidate; }
    }
    if (preview && (skillOnlyPreview ? visualSample != 0 :
        (frame == 0 || frame == 31 || frame == 38 || frame == 46 || frame == 172 ||
            frame == 184 || frame == 205 || frame == 230 || frame == 248 || frame == 359 || frame == 374))) {
        phantomPreviewPaused_ = true;
        ImGui::SetNextWindowPos({ 20.0f, 165.0f }, ImGuiCond_Always);
        ImGui::Begin("Phantom visual fixture", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted("TEST ONLY: animation frozen for visual inspection");
        ImGui::Text("Sample: %d   Clock: %.1f", frame, playerSkill_.Clock());
        bool next = ImGui::Button("NEXT / F8", { 220.0f, 36.0f }) || ImGui::IsKeyPressed(ImGuiKey_F8, false);
        if (skillOnlyPreview && autoSkillPreview) {
            static unsigned int heldSample = 0;
            static double heldSince = 0.0;
            if (heldSample != visualSample) { heldSample = visualSample; heldSince = ImGui::GetTime(); }
            // 明示した自動映像試験だけ、確認停止を8秒で解除する。
            next |= ImGui::GetTime() - heldSince >= 8.0;
        }
        ImGui::End();
        if (!next) {
            // 表示確認中も長押し試験の入力を保つ。ここで離すと再開時に
            // 新しいSHIFT押下が生まれ、通常プレイにはない再回避を起こしてしまう。
            if (frame >= 31 && frame <= 65) { keys[DIK_LSHIFT] = 0x80; }
            input_->SetTestFrame(keys, mouse);
            return false;
        }
        visualSamples |= visualSample;
        phantomPreviewPaused_ = false;
    }
    if (retry && !isGameClear_) {
        require(playerSkill_.IsReady() && !IsPhantomRaidActive() && playerSkill_.ActivationCount() == 0 &&
            playerSkill_.DefeatCount() == 0 && playerSkill_.CooldownFrames() == 0.0f, "retry skill state reset");
        require(audio_.SoundForProbe() && audio_.SoundForProbe()->GetVoiceCount() == 29, "retry audio pool");
        log("PASS retry_reset voices=29");
        input_->SetTestFrame(keys, mouse);
        return true;
    }
    ++frame;
    require(!isGameOver_, "fixture player died");
    if (frame < 620) {
        // 検証用に全編隊を消化済みにしているため、空の時間を短縮する本編進行は区切る。
        // レール・敵・射撃・回避は通常更新のまま。ボス試験から本編進行へ戻す。
        stageFlight_.HoldProgressForProbe(0.0f);
    }
    if (frame == 1) {
        CheckEnemyFireControl();
        log("ENEMY_FIRE_CONTROL_OK rail_intercept=1 fixed_aim=1 windup_burst_recovery=1");
        CheckDodgeControls(object3dCommon_.get(), sceneSetup_.PlayerModelForProbe());
        log("DODGE_CONTROLS_OK distance=4.4 duration=16 buffer=5 slow_independent=1 steps=0.5,1,2 edge_ok=1");
        require(playerSkill_.IsReady() && !IsPhantomRaidActive() && playerSkill_.CooldownFrames() == 0.0f,
            "startup must be ready without dodge, but must not auto activate");
        require(audio_.SoundForProbe() && audio_.SoundForProbe()->GetVoiceCount() == 29, "all sounds loaded");
        if (!SoundManager::kSoundEffectsEnabled) {
            for (const char* key : { "shot", "charge", "skill_start", "hit", "destroy", "damage",
                "dodge", "gate_boost", "fever", "clear", "fail", "skill_ready", "slash", "slash_finish" }) {
                require(!audio_.SoundForProbe()->Play(key), "disabled sound effect was submitted");
            }
            require(audio_.SoundForProbe()->GetPlayCount() == 0, "disabled sound effects changed playback count");
            require(audio_.SoundForProbe()->PlayLoop("music_stage") && audio_.SoundForProbe()->GetLoopCount() == 1,
                "sound effect disable also stopped BGM");
            log("SFX_DISABLED_OK one_shots_blocked=1 bgm_loop=1");
        }
        log(std::string("AUDIO_BANK mode=") + (audio_.UsesLocalSkillAudioForProbe() ? "local_cinematic" : "original_cc0") +
            " voices=29");
        // 実際の連撃を最後まで進め、HP6の狙撃機を一押しで消せないことを確認。
        for (const bool empowered : { false, true }) {
            DebugJumpToStagePhase(0);
            rewards_.Fever().SetGauge(0);
            rewards_.Fever().SetRemainingFramesForDebug(empowered ? 600 : 0);
            SpawnStageEnemyForProbe(0, 1, 40, Enemy::Behavior::Sniper, Enemy::EntryStyle::Direct, 6, 1.0f);
            auto* sniper = stageEnemies_.Active().back().get();
            for (int step = 0; step < 120; ++step) { sniper->Update(stageFlight_.Distance()); }
            require(TryActivatePhantomRaid(), "skill balance fixture failed to activate");
            for (int step = 0; step < 480 && IsPhantomRaidActive(); ++step) { UpdatePhantomRaid(); }
            require(!IsPhantomRaidActive() && !sniper->IsDead() &&
                sniper->GetHp() == (empowered ? 2 : 3) && playerSkill_.DefeatCount() == 0,
                "skill removed full-health sniper without follow-up shots");
            require(!playerSkill_.IsReady() && playerSkill_.CooldownFrames() > 600.0f,
                "skill damage recharged its own cooldown");
            combatEffects_.Reset();
        }
        DebugJumpToStagePhase(0);
        rewards_.Fever().SetGauge(0);
        rewards_.Fever().SetRemainingFramesForDebug(0);
        hud_.Reset();
        gameplayCamera_.ClearShake();
        log("SKILL_BALANCE_OK sniper_hp=6 normal_remaining=3 fever_remaining=2 no_self_recovery=1");
        stageFlight_.DisableSpawnsForProbe();
        log("BEGIN controlled_enemy_fixture=1 normal_damage_code=1");
        keys[DIK_Q] = 0x80;
    }
    if (frame == 2) {
        require(playerSkill_.ActivationCount() == 0 && playerSkill_.IsReady() && playerSkill_.CooldownFrames() == 0.0f,
            "Q without target consumed startup ready");
        log("STARTUP_READY_WITHOUT_DODGE_OK empty_target_no_consumption=1");
    }
    if (frame == 30) {
        playerSkill_.SetRechargeForDebug(300.0f); // 回避でスキルが再充填されないことを独立検証。
        // 実際の当たり判定を通る近接弾を置き、SHIFTでジャスト回避させる。
        const auto p = player_->GetTranslate();
        dodgeStartX = p.x;
        // 初動の横移動量に左右されないよう、上側のかすり領域へ置く。
        // 機体に直撃する配置では「無敵で弾消去」になり、ジャスト回避試験にならない。
        FireEnemyBullet({ p.x + 0.5f, p.y + 2.2f, p.z + 1.0f });
        keys[DIK_D] = keys[DIK_LSHIFT] = 0x80;
    }
    // 実フレーム間でSHIFTを押し続けても、再使用可能時に勝手に2回目が出ない。
    if (frame >= 31 && frame <= 65) { keys[DIK_LSHIFT] = 0x80; }
    if (frame == 31) {
        require(rewards_.JustDodgeCount() > 0 && !playerSkill_.IsReady() && playerSkill_.CooldownFrames() > 290.0f,
            "just dodge unexpectedly recharged the independent skill");
        for (auto& bullet : projectiles_.EnemyPool().Active()) { bullet->Kill(); }
        playerSkill_.ClearCooldownForDebug();
        GrantPhantomRaid(); // 後続の連撃試験を初期の使用可能状態へ戻す。
        log("DODGE_INDEPENDENT_OK");
    }
    if (frame == 40) { keys[DIK_Q] = 0x80; }
    if (frame == 41) {
        require(playerSkill_.IsReady() && !IsPhantomRaidActive(), "no target consumed ready");
        log("NO_TARGET_NO_CONSUMPTION_OK");
    }
    if (frame == 65) {
        require(!player_->IsDodging() &&
            std::abs(player_->GetTranslate().x - dodgeStartX - 4.4f) < 0.01f,
            "held SHIFT caused an automatic repeat dodge");
        log("DODGE_HELD_KEY_NO_REPEAT_OK");
    }
    const auto spawn = [&](int count) {
        for (int index = 0; index < count; ++index) {
            const float x = (static_cast<float>(index) - static_cast<float>(count - 1) * 0.5f) * 2.6f;
            SpawnStageEnemyForProbe(x, 1.0f + static_cast<float>(index % 2) * 0.7f, 32.0f,
                Enemy::Behavior::Formation, Enemy::EntryStyle::Direct, 3, 1.0f);
        }
    };
    if (frame == 70) {
        // 本編「Sniper wing」と同じ位置・HP・編隊動作で、スキル対象の選択を検証する。
        SpawnStageEnemyForProbe(-4.6f, -0.6f, 44.0f, Enemy::Behavior::Formation,
            Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemyForProbe(-2.0f, 0.5f, 44.0f, Enemy::Behavior::Formation,
            Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemyForProbe(0.6f, -0.6f, 44.0f, Enemy::Behavior::Formation,
            Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemyForProbe(5.3f, 2.0f, 48.0f, Enemy::Behavior::Sniper,
            Enemy::EntryStyle::PopShooter, 6, 1.18f);
    }
    if (frame == 149) {
        // 狙撃機を指した場合も実際の選択処理を通す。時間は進めず、ダメージ前に試験状態を戻す。
        Math::Vector2 screen{};
        require(stageEnemies_.Active().size() == 4 && stageEnemies_.Active().back()->IsSniper() &&
            TryProjectToScreen(stageEnemies_.Active().back()->GetAimPosition(), screen), "sniper selection fixture missing");
        const auto savedReticle = playerControl_.Aim().Reticle();
        playerControl_.Aim().SetReticleForProbe(screen);
        require(TryActivatePhantomRaid() && playerSkill_.TargetCount() == 3 &&
            playerSkill_.Targets()[0].enemy == stageEnemies_.Active().back().get(), "sniper could not be prioritized by aiming");
        ResetPhantomRaid();
        playerControl_.Aim().SetReticleForProbe(savedReticle);
        log("SNIPER_CHOICE_OK aimed_sniper_selected_first=1");
    }
    if (frame == 150) {
        Math::Vector2 screen{};
        require(stageEnemies_.Active().size() == 4 && TryProjectToScreen((*std::next(stageEnemies_.Active().begin()))->GetAimPosition(), screen),
            "formation/sniper fixture missing");
        const ImVec2 origin = ImGui::GetMainViewport()->Pos;
        mouse = { screen.x - origin.x, screen.y - origin.y };
        // UpdateLockOnTargetはUpdateの末尾なので、照準を置く1フレームと発動を分ける。
        activationMouse = mouse;
    }
    if (frame == 151) {
        audioBefore = audio_.SkillPlayCountsForProbe();
        // Input::Updateが毎フレームOSカーソルを取得するため、試験の照準を明示的に維持する。
        mouse = activationMouse;
        keys[DIK_Q] = 0x80;
        log("NORMAL_ACTIVATE");
    }
    if (frame == 152) {
        require(IsPhantomRaidActive() && playerSkill_.TargetCount() == 3, "normal target selection");
        for (int index = 0; index < playerSkill_.TargetCount(); ++index) {
            require(playerSkill_.Targets()[index].enemy && !playerSkill_.Targets()[index].enemy->IsSniper(),
                "aiming at formation failed to prioritize its three targets");
        }
        require(playerSkill_.CooldownFrames() == PlayerSkill::kCooldownFrames, "activation must begin the full skill cooldown");
        const float before = playerSkill_.CooldownFrames();
        CombatResolver::RecoverSkillOnHit(BuildCombatContext(), true, true);
        require(playerSkill_.CooldownFrames() == before, "active skill allowed cooldown recovery");
        log("COOLDOWN_18_SECONDS_OK no_self_recharge=1");
    }
    if (frame == 220) {
        require(audio_.SkillPlayCountsForProbe()[0] - audioBefore[0] == (SoundManager::kSoundEffectsEnabled ? 3u : 0u) &&
            audio_.SkillPlayCountsForProbe()[1] - audioBefore[1] == (SoundManager::kSoundEffectsEnabled ? 1u : 0u),
            "normal skill audio does not match sound effect enable state");
        log(SoundManager::kSoundEffectsEnabled ? "AUDIO_NORMAL_OK slashes=3 finish=1" :
            "AUDIO_NORMAL_OK muted=1 slashes=0 finish=0");
        require(!IsPhantomRaidActive() && playerSkill_.DefeatCount() == 3 && combatResolver_.Stats().defeated == 3, "normal triple finish");
        require(stageEnemies_.Active().size() == 1 && stageEnemies_.Active().front()->IsSniper() && !stageEnemies_.Active().front()->IsDead(),
            "formation clear should leave the independently positioned sniper alive");
        log("FORMATION_CHOICE_OK triple_clear=1 sniper_remains=1");
        stageEnemies_.Active().front()->Kill(); // 次の独立ケースへ持ち越さない。報酬には加算しない。
        require(!playerSkill_.IsReady() && playerSkill_.CooldownFrames() > 0.0f, "normal consumption or cooldown");
        GrantPhantomRaid();
        require(!playerSkill_.IsReady(), "cooldown allowed immediate recharge");
        const float before = playerSkill_.CooldownFrames();
        CombatResolver::RecoverSkillOnHit(BuildCombatContext(), false, false);
        require(std::abs(playerSkill_.CooldownFrames() - (before - 1.5f)) < 0.001f, "normal hit cooldown reduction");
        CombatResolver::RecoverSkillOnHit(BuildCombatContext(), true, true);
        require(std::abs(playerSkill_.CooldownFrames() - (before - 10.5f)) < 0.001f, "charged kill cooldown reduction");
        playerSkill_.SetRechargeForDebug(1.5f);
        CombatResolver::RecoverSkillOnHit(BuildCombatContext(), false, false);
        require(playerSkill_.IsReady() && playerSkill_.CooldownFrames() == 0.0f, "hit did not complete recovery");
        const auto sounds = audio_.SoundForProbe()->GetPlayCount();
        CombatResolver::RecoverSkillOnHit(BuildCombatContext(), true, true);
        require(audio_.SoundForProbe()->GetPlayCount() == sounds, "ready sound repeated on extra hit");
        log("NORMAL_3_KILLS_OK reward_once cooldown_ok");
        log("HIT_RECOVERY_OK normal=1.5 charged_kill=9 clamped=0 notification_once=1");
    }
    if (frame == 221) {
        playerSkill_.SetRechargeForDebug(3.0f); // 最後の短区間を無入力で進め、満了時の自動回復を検証。
    }
    if (frame == 230) {
        require(playerSkill_.IsReady() && playerSkill_.CooldownFrames() == 0.0f, "time expiry did not auto recharge skill");
        log("TIME_ONLY_RECOVERY_OK no_shooting_or_dodge=1");
    }
    if (frame == 232) {
        rewards_.Fever().SetRemainingFramesForDebug(0);
        rewards_.Fever().SetGauge(99);
        feverCountBeforeAutoTest = rewards_.Fever().ActivationCount();
    }
    if (frame == 233) {
        require(rewards_.Fever().RemainingFrames() == 0 && rewards_.Fever().ActivationCount() == feverCountBeforeAutoTest,
            "fever activated below full gauge");
        AddFeverGauge(1); // 本番のゲージ加算経路で満タンにする。Eキーは送らない。
    }
    if (frame == 234) {
        require(rewards_.Fever().IsActive() && rewards_.Fever().Gauge() == 0 &&
            rewards_.Fever().ActivationCount() == feverCountBeforeAutoTest + 1,
            "full gauge did not auto activate fever exactly once");
        AddFeverGauge(100);
    }
    if (frame == 235) {
        require(rewards_.Fever().Gauge() == 0 && rewards_.Fever().ActivationCount() == feverCountBeforeAutoTest + 1,
            "active fever accumulated gauge or activated twice");
        log("FEVER_AUTO_OK threshold=100 gauge_consumed=1 no_key=1 no_double_activation=1");
    }
    if (frame == 240) {
        playerSkill_.ClearCooldownForDebug(); // 次の独立ケース用の試験リセット。
        GrantPhantomRaid();
        spawn(5);
    }
    if (frame == 330) {
        audioBefore = audio_.SkillPlayCountsForProbe();
        keys[DIK_Q] = 0x80;
        log("OVERDRIVE_ACTIVATE");
    }
    if (frame == 331) { require(playerSkill_.IsEmpowered() && playerSkill_.TargetCount() == 5, "fever target selection"); }
    if (frame == 410) {
        require(audio_.SkillPlayCountsForProbe()[0] - audioBefore[0] == (SoundManager::kSoundEffectsEnabled ? 5u : 0u) &&
            audio_.SkillPlayCountsForProbe()[1] - audioBefore[1] == (SoundManager::kSoundEffectsEnabled ? 1u : 0u),
            "fever skill audio does not match sound effect enable state");
        log(SoundManager::kSoundEffectsEnabled ? "AUDIO_FEVER_OK slashes=5 finish=1" :
            "AUDIO_FEVER_OK muted=1 slashes=0 finish=0");
        require(!IsPhantomRaidActive() && playerSkill_.DefeatCount() == 8 && combatResolver_.Stats().defeated == 8, "fever five finish");
        log("OVERDRIVE_5_KILLS_OK");
    }
    if (frame == 430) {
        rewards_.Fever().SetRemainingFramesForDebug(1); // 終了境界も通常のUpdateFever経路で検証する。
        playerSkill_.ClearCooldownForDebug();
        GrantPhantomRaid();
        spawn(3);
    }
    if (frame == 432) {
        require(rewards_.Fever().RemainingFrames() == 0 && rewards_.Fever().Gauge() == 0 &&
            rewards_.Fever().ActivationCount() == feverCountBeforeAutoTest + 1,
            "fever reactivated after expiry without refilling");
        log("FEVER_AUTO_EXPIRY_OK no_retrigger=1");
    }
    if (frame == 520) { keys[DIK_Q] = 0x80; log("TARGET_REMOVAL_ACTIVATE"); }
    if (frame == 526) {
        require(!stageEnemies_.Active().empty(), "target removal fixture had no enemy");
        const Enemy* removed = stageEnemies_.Active().front().get();
        removalBullet = projectiles_.AcquirePlayerForDebug();
        require(removalBullet != nullptr, "target removal fixture had no bullet");
        removalBullet->Initialize(object3dCommon_.get(), projectiles_.BodyModelForDebug(),
            { 0.0f, 1.4f, stageFlight_.Distance() - 4.0f }, { 0.0f, 0.0f, 0.01f });
        projectiles_.SetHomingTargetForDebug(*removalBullet, removed);
        require(removalBullet->TryRegisterHitTarget(removed), "target removal fixture did not record a hit");
        playerControl_.Aim().SetTargetForProbe(*removed);
        stageEnemies_.KillAll(); // 演出途中の外部消滅を模擬。
    }
    if (frame == 527) {
        require(stageEnemies_.Active().empty() && !playerControl_.Aim().Target() &&
            std::none_of(playerSkill_.Targets().begin(), playerSkill_.Targets().end(),
                [](const auto& target) { return target.enemy != nullptr; }),
            "enemy removal kept aim or skill references");
        require(!removalBullet->IsDead() && !projectiles_.HasHomingTargetForProbe(*removalBullet) &&
            !removalBullet->HasHitTargetsForProbe(), "enemy removal kept bullet references or hit history");
        removalBullet->Kill();
        log("ENEMY_REFERENCES_CLEARED aim=1 skill=1 homing=1 hit_history=1");
    }
    if (frame == 590) {
        require(!IsPhantomRaidActive() && playerSkill_.DefeatCount() == 8, "removed targets double rewarded");
        log("REMOVED_TARGETS_SAFE_OK");
    }
    if (frame == 620) {
        DebugJumpToStagePhase(3);
        log("BOSS_FIXTURE_BEGIN");
    }
    if (frame == 820) {
        Enemy* boss = nullptr;
        for (auto& enemy : stageEnemies_.Active()) { if (enemy->IsBoss()) { boss = enemy.get(); break; } }
        require(boss != nullptr, "boss fixture missing");
        boss->Damage((std::max)(0, boss->GetHp() - 7)); // クリア経路を調べる低HP試験配置。
        GrantPhantomRaid();
        ActivateFever();
    }
    if (frame == 850) { keys[DIK_Q] = 0x80; log("BOSS_FINISH_ACTIVATE"); }
    if (frame > 920 && isGameClear_ && !stageResults_.IsWaiting() && !retry) {
        require(stageEnemies_.BossDefeated() && playerSkill_.DefeatCount() == 1 &&
            combatResolver_.Stats().defeated == GetTotalEnemyTargetCount() + 1, "boss clear reward path");
        log("BOSS_SKILL_CLEAR_OK");
        keys[DIK_R] = 0x80;
        retry = true;
    }
    if (frame == 1100) { require(retry, "boss clear not reached"); }
    input_->SetTestFrame(keys, mouse);
    return false;
}
// 確認用にボス戦を用意し、各攻撃、形態変化、撃破後の進行を順番に検証する。
bool GameRuntime::RunBossProbe(const std::string& logPath, bool preview)
{
    static int frame = 0;
    // リザルトを詰める際は、戦闘の確認停止を省いて勝敗の三画面だけを確認できる。
    static const bool resultOnlyPreview = [] {
        char value[2]{};
        return GetEnvironmentVariableA("AZRAID_RESULT_PREVIEW", value, 2) > 0 && value[0] == '1';
    }();
    const auto log = [&](const std::string& message) {
        std::ofstream file(logPath, std::ios::app);
        file << "BOSS_TEST " << message << '\n';
    };
    const auto require = [&](bool ok, const char* message) {
        if (!ok) { log(std::string("FAIL ") + message); throw std::runtime_error(message); }
    };
    if (preview && ((!resultOnlyPreview && (frame == 30 || frame == 31 || frame == 55 || frame == 138 || frame == 172 || frame == 366 || frame == 606 || frame == 700 || frame == 773)) ||
        (resultOnlyPreview && (frame == 890 || frame == 897)) ||
        frame == 880 || frame == 901 || frame == 931)) {
        phantomPreviewPaused_ = true;
        // 同じHUDを背後に残したポーズ・操作方法も、通常起動を変えずに実画面で確認する。
        gameplayMenu_.SetPausedForDebug(frame == 30 || frame == 31, frame == 31);
        bool next = ImGui::IsKeyPressed(ImGuiKey_F8, false);
        if (resultOnlyPreview) {
            static int heldFrame = -1;
            static double heldSince = 0.0;
            if (heldFrame != frame) { heldFrame = frame; heldSince = ImGui::GetTime(); }
            // 記録のために停止してもテストを放置しない。通常の手動映像試験には適用しない。
            next |= ImGui::GetTime() - heldSince >= (frame == 931 ? 3.0 : 15.0);
        }
        if (frame < 880) {
            ImGui::SetNextWindowPos({ 20.0f, 150.0f }, ImGuiCond_Always);
            ImGui::Begin("Boss visual fixture", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextUnformatted("TEST ONLY: pause / controls / boss / results");
            ImGui::Text("Frame %d   Pattern %d   Counter %d", frame,
                static_cast<int>(bossAttack_.CurrentPattern()), bossAttack_.CounterFrames());
            // 全画面の操作説明ウィンドウがマウスを覆っていても、映像試験を進められる。
            next |= ImGui::Button("NEXT / F8", { 220.0f, 36.0f });
            ImGui::End();
        }
        if (!next) { input_->SetTestFrame({}, { 640, 320 }); return false; }
        phantomPreviewPaused_ = false;
        gameplayMenu_.SetPausedForDebug(false);
    }
    ++frame;
    if (frame == 1) {
        require(audio_.SoundForProbe() && audio_.SoundForProbe()->GetVoiceCount() == 29, "BGM/SFX bank incomplete");
        for (const char* key : { "music_stage", "music_boss", "music_fever" }) {
            require(audio_.SoundForProbe()->PlayLoop(key) && audio_.SoundForProbe()->PlayLoop(key) && audio_.SoundForProbe()->GetLoopCount() == 1,
                "loop not queued or duplicate loop created");
            audio_.SoundForProbe()->Stop(key);
            require(audio_.SoundForProbe()->GetLoopCount() == 0, "loop did not stop");
        }
        require(audio_.SoundForProbe()->GetVoiceCount() == 29 && audio_.SoundForProbe()->GetPlayCount() == 0, "loop changed SFX counter/pool");
        log("LOOPS_OK voices=29 duplicate_safe=1 stop_safe=1 sfx_count_unchanged=1");
        const auto reset = [&]() {
            isGameClear_ = false;
            isGameOver_ = false;
            stageResults_.ResetSoundForProbe();
            combatProgress_.SetScoreForDebug(0);
            rewards_.Fever().SetRemainingFramesForDebug(0);
            rewards_.Fever().SetGauge(0);
            DebugJumpToStagePhase(3);
            combatEffects_.Reset();
            SpawnBossEnemy();
            require(bossPresentation_.HasRadio(), "boss entry did not request control warning");
            return stageEnemies_.Active().back().get();
        };
        for (int phase : { 1, 2 }) {
            for (int pattern = 0; pattern < static_cast<int>(BossAttackController::Pattern::Count); ++pattern) {
                auto* boss = reset();
                require(!boss->IsTargetable(), "boss targetable before arrival");
                for (int step = 0; step < 95; ++step) { boss->Update(stageFlight_.Distance()); }
                require(!boss->IsTargetable(), "boss became targetable before entry completed");
                for (int step = 95; step < 300; ++step) { boss->Update(stageFlight_.Distance()); }
                require(boss->CanShoot(), "boss fixture not ready");
                bossPresentation_.EndIntro();
                bossAttack_.PrepareForDebug(phase, static_cast<BossAttackController::Pattern>(pattern));
                float travel = 0.0f;
                const auto tick = [&]() {
                    const auto before = boss->GetTranslate();
                    UpdateBossActions();
                    UpdateEnemies();
                    const auto after = boss->GetTranslate();
                    const float step = std::sqrt(DistanceSquared(before, after));
                    require(step < 1.5f, "boss teleported between flight targets");
                    require(std::abs(DistanceSquared(boss->GetBossMuzzlePosition(-1.0f),
                        boss->GetBossMuzzlePosition(1.0f)) - 19.36f) < 0.01f,
                        "boss muzzle anchors separated from hull rotation");
                    travel += step;
                };
                tick();
                const int windup = bossAttack_.RemainingFrames();
                require(windup >= 38 && projectiles_.EnemyPool().Active().empty(), "boss fired without a readable windup");
                const auto playerPosition = player_->GetTranslate();
                player_->SetCinematicPose({ 3.75f, playerPosition.y, playerPosition.z }, player_->GetVisualRotate());
                while (bossAttack_.RemainingFrames() > BossAttackController::kAimLockFrames) { tick(); }
                // 固定後に実際の自機を戻す。内部の照準値を書き換えず、追尾停止を確認する。
                player_->SetCinematicPose(playerPosition, player_->GetVisualRotate());
                while (bossAttack_.RemainingFrames() > 0) { tick(); }
                require(projectiles_.EnemyPool().Active().empty() && bossAttack_.AimPoint().x == 3.75f, "boss tracked after aim lock");
                tick();
                int guard = 0;
                while (bossAttack_.IsAttacking() && ++guard < 150) { tick(); }
                require(travel > 2.0f, "boss did not move during attack");
                const size_t expected = pattern == 0 ? (phase == 2 ? 7u : 5u) :
                    pattern == 1 || pattern == 4 ? (phase == 2 ? 9u : 7u) :
                    pattern == 3 ? (phase == 2 ? 9u : 6u) : (phase == 2 ? 3u : 1u);
                require(!bossAttack_.IsAttacking() && projectiles_.EnemyPool().Active().size() == expected && bossAttack_.AimPoint().x == 3.75f,
                    "boss pattern count/fixed sweep aim incorrect");
                if (pattern == 2) {
                    const auto p = projectiles_.EnemyPool().Active().front()->GetTranslate();
                    const auto v = projectiles_.EnemyPool().Active().front()->GetVelocity();
                    const float t = (p.z - player_->GetTranslate().z - 0.18f) / (stageFlight_.Speed() - v.z);
                    require(t > 0 && std::abs(p.x + v.x * t - 3.75f) < 0.01f, "charge ignored fixed aim");
                }
                require(bossAttack_.CounterFrames() >= 84 &&
                    bossAttack_.CooldownFrames() > bossAttack_.CounterFrames(), "counter opening too short or overlaps next attack");
                boss->SetBossRecoveryRate(0.5f);
                const auto beforeMove = boss->GetTranslate();
                boss->SetBossFlightOffset({ beforeMove.x - 4.0f, 1.0f, beforeMove.z - stageFlight_.Distance() - 42.0f });
                for (int step = 0; step < 40; ++step) { boss->Update(stageFlight_.Distance()); }
                const auto afterMove = boss->GetTranslate();
                require(afterMove.x < beforeMove.x - 1.0f && afterMove.z < beforeMove.z - 3.0f,
                    "boss movement is frozen during recovery");
                const size_t shotCount = projectiles_.EnemyPool().Active().size();
                for (int step = 0; step < bossAttack_.CounterDuration(); ++step) { UpdateBossActions(); }
                require(bossAttack_.CounterFrames() == 0 && projectiles_.EnemyPool().Active().size() == shotCount,
                    "boss fired inside counter opening");
                log("PATTERN_OK phase=" + std::to_string(phase) + " pattern=" + std::to_string(pattern) +
                    " windup=" + std::to_string(windup) + " shots=" + std::to_string(shotCount) +
                    " flight_distance=" + std::to_string(travel));
            }
        }
        auto* boss = reset();
        for (int step = 0; step < 300; ++step) { boss->Update(stageFlight_.Distance()); }
        boss->Damage(boss->GetMaxHp() / 2);
        FireEnemyBullet(boss->GetAimPosition(), EnemyBulletStyle::BossCharge);
        UpdateBossActions();
        require(bossAttack_.Phase() == 2 && bossPresentation_.IsPhaseTransitionPlaying() && projectiles_.EnemyPool().Active().empty(),
            "phase transition retained dangerous bullets");
        require(bossPresentation_.HasRadio(), "phase transition replaced arrival-only radio");
        const float arrivalRadioRemaining = bossPresentation_.RadioRemaining();
        RequestBossRadio();
        require(bossPresentation_.RadioRemaining() == arrivalRadioRemaining, "boss entry restarted arrival radio");
        for (int step = 0; step < 40; ++step) { UpdateBossRadio(0.1f); }
        UpdateBossActions();
        require(!bossPresentation_.HasRadio(), "phase combat displayed additional radio");
        log("RADIO_OK arrival_only=1 phase_combat_silent=1 no_repeat_on_entry=1");
        log("PHASE_OK hp_half_transition=1 bullets_recycled=1");
        boss->Damage(999);
        combatResolver_.RegisterEnemyDefeat(BuildCombatContext(), *boss, true, false);
        require(isGameClear_ && stageEnemies_.BossDefeated() && projectiles_.EnemyPool().Active().empty() && !stageResults_.SoundPlayedForProbe(),
            "boss defeat cleanup failed");
        size_t expectedEffects = combatEffects_.ActiveCount();
        for (int step = 1; step <= 45; ++step) {
            UpdateResultAndSceneObjects();
            if (step == 12 || step == 24 || step == 36) { ++expectedEffects; }
            require(combatEffects_.ActiveCount() == expectedEffects, "boss defeat burst was missing or mistimed");
            require(stageResults_.SoundPlayedForProbe() == (step == 45), "clear cue interrupted the boss defeat effect");
        }
        require(stageResults_.SoundPlayedForProbe() && !bossPresentation_.IsDefeatPlaying(), "defeat sequence/result cue not completed");
        require(projectiles_.EnemyPool().MissCount() == 0 && combatEffects_.MissCount() == 0, "boss exhausted fixed pools");
        log("DEFEAT_OK delayed_bursts=3 result_after_bursts=1 pool_misses=0");
        reset();
        bossAttack_.PrepareForDebug(1, BossAttackController::Pattern::Rush);
        gameplayCamera_.ClearShake();
        audio_.ResetMixForProbe();
        log("LIVE_VISUAL_BEGIN controlled_fixture=1");
    }
    if (frame == 500) { rewards_.Fever().SetRemainingFramesForDebug(90); }
    if (frame == 530) {
        require(audio_.TrackForProbe() == GameplayAudio::Track::Fever && audio_.SoundForProbe()->GetLoopCount() == 1, "fever crossfade did not settle");
        log("FEVER_MUSIC_OK one_loop=1");
    }
    if (frame == 640) {
        require(audio_.TrackForProbe() == GameplayAudio::Track::Boss && audio_.SoundForProbe()->GetLoopCount() == 1, "boss music did not resume");
        log("BOSS_MUSIC_OK one_loop=1");
    }
    if (frame == 660) {
        // 第2段階の警告も、実際のHP条件と通常Updateから表示する映像試験。
        auto* boss = stageEnemies_.Active().front().get();
        boss->Damage(boss->GetMaxHp() / 2);
    }
    if (frame == 750) {
        auto* boss = stageEnemies_.Active().front().get();
        boss->Damage(999); // 撃破映像の固定サンプル。通常通し試験ではこの操作を使わない。
        combatResolver_.RegisterEnemyDefeat(BuildCombatContext(), *boss, true, false);
    }
    if (frame == 840) {
        require(isGameClear_ && audio_.SoundForProbe()->GetLoopCount() == 0 && stageResults_.SoundPlayedForProbe(), "music remains after clear");
        require(combatEffects_.MissCount() == 0, "live defeat effect pool exhausted");
        log("PASS clear_music_silent=1 voices=29 pool_misses=0");
        input_->SetTestFrame({}, { 640, 320 });
    }
    if (frame == 880) {
        require(HasResultPresentation(), "boss clear did not reach the result presentation");
        log("RESULT_OK clear_screen_after_defeat=1");
        if (!preview) { return true; }
    }
    if (preview && resultOnlyPreview && (frame == 880 || frame == 890 || frame == 897)) {
        combatResolver_.SetPlayerDamageForProbe(frame == 880 ? 2 : (frame == 890 ? 0 : 3));
        log(std::string("RESULT_GRADE_SAMPLE rank=") + (frame == 880 ? "A" : (frame == 890 ? "S" : "B")));
    }
    // リザルトの分岐は映像検証時だけ切り替える。通常プレイ・通常の通し試験は実際の勝敗を使う。
    if (preview && frame == 900) {
        if (resultOnlyPreview) { combatResolver_.SetPlayerDamageForProbe(2); }
        isGameClear_ = false; isGameOver_ = true;
    }
    if (preview && frame == 930) {
        playMode_ = PlayMode::Tutorial;
        isGameClear_ = true;
        isGameOver_ = false;
        stageEnemies_.ClearBossSpawnedForProbe();
        combatResolver_.SetDefeatedForProbe(GetTotalEnemyTargetCount());
    }
    if (preview && frame == 960) { log("RESULT_PREVIEW_COMPLETE"); return true; }
    input_->SetTestFrame({}, { 640, 320 });
    return false;
}

// 確認用の敵弾を用意し、横回避とジャスト回避が成立する場面を検証する。
bool GameRuntime::RunJustDodgeProbe(const std::string& logPath, bool preview)
{
    static int frame = 0;
    static unsigned int samples = 0;
    static uint64_t initialAudio = 0;
    static int pausedFlash = 0, pausedSlow = 0;
    const auto log = [&](const std::string& message) {
        std::ofstream file(logPath, std::ios::app);
        file << "DODGE_TEST " << message << '\n';
    };
    const auto require = [&](bool ok, const char* message) {
        if (!ok) { log(std::string("FAIL ") + message); throw std::runtime_error(message); }
    };
    std::array<BYTE, 256> keys{};
    const Math::Vector2 mouse{ 640, 320 };
    const unsigned int sample = frame == 61 ? 1u : frame == 65 ? 2u : frame == 83 ? 4u :
        frame == 105 ? 8u : frame == 131 ? 16u : frame == 139 ? 32u : 0u;
    if (preview && sample && !(samples & sample)) {
        phantomPreviewPaused_ = true;
        ImGui::SetNextWindowPos({ 20, 165 }, ImGuiCond_Always);
        ImGui::Begin("Just dodge review", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("TEST ONLY: contact / roll / release / cruise / fever   frame=%d", frame);
        const bool next = ImGui::Button("NEXT / F8", { 220, 36 }) || ImGui::IsKeyPressed(ImGuiKey_F8, false);
        ImGui::End();
        if (!next) { input_->SetTestFrame(keys, mouse); return false; }
        samples |= sample;
        phantomPreviewPaused_ = false;
    }
    if (frame == 0) {
        CheckDodgeControls(object3dCommon_.get(), sceneSetup_.PlayerModelForProbe());
        CheckPlayerFlightEffects(playerFlightEffects_, player_->GetTranslate());
        DebugJumpToStagePhase(0);
        stageFlight_.DisableSpawnsForProbe();
        player_->Reset({ 0, 1.4f, stageFlight_.Distance() });
        playerControl_.Shooting().Reset(); playerFlightEffects_.Reset(); combatEffects_.Reset();
        combatProgress_.SetScoreForDebug(0);
        rewards_.Fever().SetRemainingFramesForDebug(0); rewards_.Fever().SetGauge(0);
        rewards_.ResetJustDodgesForDebug();
        playerCombatFeedback_.ClearDodgeForDebug();
        initialAudio = audio_.SoundForProbe()->GetPlayCount();
        log("BEGIN real_input=1 real_bullets=1 fixed_pools=1");
    }
    if (frame == 60 || frame == 130) {
        if (frame == 130) { rewards_.Fever().SetRemainingFramesForDebug(160); }
        const auto position = player_->GetTranslate();
        const float direction = frame == 60 ? 1.0f : -1.0f;
        for (int index = 0; index < 3; ++index) {
            FireEnemyBullet({ position.x + direction * (0.5f + static_cast<float>(index) * 0.06f),
                position.y + 2.2f - static_cast<float>(index) * 0.08f, position.z + 1.0f });
        }
        keys[frame == 60 ? DIK_D : DIK_A] = keys[DIK_LSHIFT] = 0x80;
    }
    if (frame >= 61 && frame < 98) { keys[DIK_LSHIFT] = 0x80; }
    if (frame == 61 || frame == 131) {
        const int expected = frame == 61 ? 1 : 2;
        require(rewards_.JustDodgeCount() == expected && player_->GetHp() == 100,
            "One volley caused missing/multiple dodge rewards or player damage");
        require(audio_.SoundForProbe()->GetPlayCount() == initialAudio + expected, "Just dodge confirmation sound did not play exactly once");
        require(GetCinematicWorldTimeScale() < 0.20f && playerCombatFeedback_.IsDodgeSlowActive(),
            "Just dodge did not briefly hold the incoming bullet");
        require(playerFlightEffects_.Inspect().afterimages > 0, "Just dodge did not emit a fixed-pool afterimage");
        if (frame == 61) {
            require(rewards_.Fever().Gauge() == 8 && playerControl_.Shooting().ChargeFrames() >= 24.0f,
                "A three-bullet volley granted stacked rewards");
        }
        log(frame == 61 ? "RIGHT_OK volley=3 success=1 sound=1 hp=100" : "LEFT_FEVER_OK success=1 sound=1 hp=100");
    }
    if (frame == 66 || frame == 72) { keys[DIK_ESCAPE] = 0x80; }
    if (frame == 67) {
        require(gameplayMenu_.IsPaused(), "Pause input did not reach gameplay");
        pausedFlash = playerCombatFeedback_.DodgeFlashFrames(); pausedSlow = playerCombatFeedback_.DodgeSlowFrames();
    }
    if (frame >= 68 && frame <= 72) {
        require(gameplayMenu_.IsPaused() && playerCombatFeedback_.DodgeFlashFrames() == pausedFlash && playerCombatFeedback_.DodgeSlowFrames() == pausedSlow,
            "Pause advanced just dodge feedback");
    }
    if (frame == 73) {
        // 再開メニューの0.2秒の演出中も世界は停止する。完了後の更新まで待つ。
        if (gameplayMenu_.IsPaused() || playerCombatFeedback_.DodgeSlowFrames() == pausedSlow) {
            require(playerCombatFeedback_.DodgeFlashFrames() == pausedFlash && playerCombatFeedback_.DodgeSlowFrames() == pausedSlow,
                "Resume menu advanced just dodge feedback before gameplay resumed");
            input_->SetTestFrame(keys, mouse);
            return false;
        }
        require(!gameplayMenu_.IsPaused() && playerCombatFeedback_.DodgeFlashFrames() < pausedFlash && playerCombatFeedback_.DodgeSlowFrames() < pausedSlow,
            "Just dodge did not resume after pause");
        log("PAUSE_OK feedback_frozen=1 resume=1");
    }
    if (frame == 105 || frame == 174) {
        require(playerCombatFeedback_.DodgeFlashFrames() == 0 && playerCombatFeedback_.DodgeSlowFrames() == 0 && GetCinematicWorldTimeScale() == 1.0f,
            "Slow motion or feedback survived its release");
        require(!player_->IsDodging() && rewards_.JustDodgeCount() == (frame == 105 ? 1 : 2),
            "Held SHIFT caused a repeated dodge or a volley restarted feedback");
        require(playerFlightEffects_.Inspect().afterimages == 0, "Afterimages survived the release");
        log("RELEASE_OK smooth_world_recovery=1 no_repeat=1 trails_expired=1");
    }
    if (frame == 180) {
        require(player_->GetHp() == 100 && combatEffects_.MissCount() == 0 && projectiles_.EnemyPool().MissCount() == 0,
            "Dodge fixture lost health or exhausted its visual/projectile pool");
        require(playerControl_.Shooting().ChargeFrames() >= 48.0f, "Second dodge did not retain charge recovery");
        log("PASS directions=2 fever=1 volley_dedup=1 sound=1 controls=1 pause=1 pools=0");
        input_->SetTestFrame({}, mouse);
        return true;
    }
    stageFlight_.HoldProgressForProbe(0.0f);
    ++frame;
    input_->SetTestFrame(keys, mouse);
    return false;
}

// チャージの蓄積と発射、爆風の巻き込みを、実際の射撃・衝突処理で検証する。
bool GameRuntime::RunChargeShotProbe(const std::string& logPath, bool preview)
{
    static const bool dodgeReview = [] {
        char value[8]{};
        return GetEnvironmentVariableA("AZRAID_DODGE_REVIEW", value, sizeof(value)) > 0 && value[0] == '1';
    }();
    if (dodgeReview) { return RunJustDodgeProbe(logPath, preview); }
    static int frame = 0;
    static int impactFrame = -1;
    static int strategy = 0;
    static int strategyFrame = 0;
    static int sniperShots = 0;
    static bool counterFired = false;
    static bool sawLockedAim = false;
    static Math::Vector3 lockedAim{};
    static unsigned int previewSamples = 0;
    const auto log = [&](const std::string& message) {
        std::ofstream file(logPath, std::ios::app);
        file << "CHARGE_TEST " << message << '\n';
    };
    const auto require = [&](bool ok, const char* message) {
        if (!ok) { log(std::string("FAIL ") + message); throw std::runtime_error(message); }
    };
    std::array<BYTE, 256> keys{};
    Math::Vector2 mouse = input_->GetMousePosition();
    const auto beginEncounter = [&](int nextStrategy) {
        DebugJumpToStagePhase(0);
        stageFlight_.DisableSpawnsForProbe();
        combatEffects_.Reset();
        player_ = std::make_unique<Player>();
        player_->Initialize(object3dCommon_.get(), sceneSetup_.PlayerModelForProbe());
        player_->SetRailZ(stageFlight_.Distance());
        combatProgress_.SetScoreForDebug(0);
        rewards_.Fever().SetGauge(0);
        rewards_.Fever().SetRemainingFramesForDebug(0);
        playerControl_.Shooting().Reset();
        playerCombatFeedback_.ClearDodgeForDebug();
        playerCombatFeedback_.ClearImpactSlowForDebug();
        gameplayCamera_.ClearShake();
        strategy = nextStrategy;
        strategyFrame = sniperShots = 0;
        counterFired = sawLockedAim = false;
        previewSamples = 0;
        const float mirror = strategy == 3 ? -1.0f : 1.0f;
        SpawnStageEnemyForProbe(-4.6f * mirror, -0.6f, 44, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 3, 1);
        SpawnStageEnemyForProbe(-2.0f * mirror, 0.5f, 44, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 3, 1);
        SpawnStageEnemyForProbe(0.6f * mirror, -0.6f, 44, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 3, 1);
        SpawnStageEnemyForProbe(5.3f * mirror, 2.0f, 48, Enemy::Behavior::Sniper, Enemy::EntryStyle::PopShooter, 6, 1.18f);
        log("STRATEGY_BEGIN case=" + std::to_string(strategy) + " real_input=1 hp=100 charge=0");
    };
    if (strategy > 0) {
        Enemy* sniper = nullptr;
        Enemy* center = nullptr;
        int smallCount = 0;
        for (const auto& enemy : stageEnemies_.Active()) {
            if (enemy->IsDead()) { continue; }
            if (enemy->IsSniper()) { sniper = enemy.get(); }
            else { if (++smallCount == 2) { center = enemy.get(); } }
        }
        if (sniper) {
            const auto& fire = sniper->GetFireControl();
            if (fire.ShotFlash() > 0.0f) { sniperShots = (std::max)(sniperShots, fire.ShotIndex() + 1); }
            if (fire.IsBraced() && !fire.IsTracking()) {
                if (!sawLockedAim) { lockedAim = fire.aimPoint; sawLockedAim = true; }
                require(std::abs(lockedAim.x - fire.aimPoint.x) < 0.001f &&
                    std::abs(lockedAim.y - fire.aimPoint.y) < 0.001f, "locked sniper followed the dodge");
            }
        }
        bool incoming = false;
        const auto playerPosition = player_->GetTranslate();
        for (const auto& bullet : projectiles_.EnemyPool().Active()) {
            const auto p = bullet->GetTranslate();
            incoming |= !bullet->IsDead() && p.z > playerPosition.z && p.z - playerPosition.z < 4.0f &&
                std::abs(p.x - playerPosition.x) < 1.4f && std::abs(p.y - playerPosition.y) < 1.4f;
        }
        unsigned int sample = 0;
        if (strategy == 1 && preview && playerControl_.ShotsFired() == 2 && !projectiles_.PlayerPool().Active().empty() &&
            !(previewSamples & 32)) { sample = 32; }
        if (strategy == 2 && preview) {
            if (sawLockedAim && !(previewSamples & 1)) { sample = 1; }
            else if (incoming && !(previewSamples & 2)) { sample = 2; }
            else if (rewards_.JustDodgeCount() > 0 && !(previewSamples & 4)) { sample = 4; }
            else if (sniper && sniper->IsChargeCounterOpen() && !(previewSamples & 8)) { sample = 8; }
            else if (combatResolver_.Stats().defeated == 4 && !(previewSamples & 16)) { sample = 16; }
        }
        if (sample != 0) {
            phantomPreviewPaused_ = true;
            ImGui::SetNextWindowPos({ 20, 165 }, ImGuiCond_Always);
            ImGui::Begin("Sniper encounter fixture", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
            ImGui::Text("TEST ONLY: %s   sample=%u", sample == 32 ? "normal blue shot" : "lock / dodge / counter", sample);
            ImGui::Text("Frame: %d  Kills: %d  Shots: %d  Dodge: %d", strategyFrame,
                combatResolver_.Stats().defeated, playerControl_.ShotsFired(), rewards_.JustDodgeCount());
            const bool next = ImGui::Button("NEXT / F8", { 220, 36 }) || ImGui::IsKeyPressed(ImGuiKey_F8, false);
            ImGui::End();
            if (!next) { input_->SetTestFrame(keys, mouse); return false; }
            previewSamples |= sample;
            phantomPreviewPaused_ = false;
        }
        ++strategyFrame;
        require(strategyFrame < 520 && !isGameOver_ && stageEnemies_.EscapedCount() == 0, "encounter strategy timed out or lost a ship");
        Enemy* target = strategy == 1 && sniper ? sniper : (smallCount > 0 ? center : sniper);
        if (target && target->IsTargetable()) {
            Math::Vector2 screen{};
            require(TryProjectToScreen(target->GetAimPosition(), screen), "strategy target offscreen");
            const auto origin = ImGui::GetMainViewport()->Pos;
            mouse = { screen.x - origin.x, screen.y - origin.y };
            const bool aimed = playerControl_.Aim().Target() == target && playerControl_.Aim().IsReticleOnTarget();
            if (strategy == 1 && sniper) {
                if (aimed && (playerControl_.ShotsFired() > 0 || playerControl_.Shooting().IsChargeReady())) { keys[DIK_SPACE] = 0x80; }
            } else if (smallCount > 0) {
                if (aimed && playerControl_.Shooting().IsChargeReady()) { keys[DIK_SPACE] = 0x80; }
            } else if (sniper && aimed && !counterFired && rewards_.JustDodgeCount() > 0 &&
                playerControl_.Shooting().IsChargeReady() && sniper->IsChargeCounterOpen()) {
                keys[DIK_SPACE] = 0x80;
                counterFired = true;
                log("COUNTER_FIRED recovery=" + std::to_string(sniper->GetFireControl().RecoveryElapsed()));
            }
        }
        if (incoming && !player_->IsDodging()) {
            keys[strategy == 3 ? DIK_A : DIK_D] = keys[DIK_LSHIFT] = 0x80;
        }
        if (strategy == 1 && !sniper) { require(sniperShots == 0, "priority fire did not stop sniper's first volley"); }
        if (strategy >= 2 && combatResolver_.Stats().defeated >= 3) {
            require(smallCount == 0 && playerControl_.ShotsFired() <= 2, "center charge failed to clear exactly three small ships");
        }
        if (combatResolver_.Stats().defeated == 4) {
            require(combatResolver_.Stats().playerDamage == 0 && player_->GetHp() == 100, "strategy took damage");
            require(projectiles_.PlayerPool().MissCount() == 0 && projectiles_.EnemyPool().MissCount() == 0 && combatEffects_.MissCount() == 0,
                "strategy exhausted fixed pools");
            if (strategy >= 2) {
                require(sniperShots == 2 && sawLockedAim && rewards_.JustDodgeCount() > 0 && counterFired &&
                    playerControl_.ShotsFired() == 2 && combatResolver_.Stats().directHits == 2, "dodge counter did not kill sniper in one real charged hit");
            }
            log("STRATEGY_PASS case=" + std::to_string(strategy) + " kills=4 hp=100 sniper_shots=" +
                std::to_string(sniperShots) + " player_shots=" + std::to_string(playerControl_.ShotsFired()) +
                " dodge=" + std::to_string(rewards_.JustDodgeCount()) + " frames=" + std::to_string(strategyFrame));
            if (strategy == 3) { input_->SetTestFrame({}, mouse); log("PASS sniper_priority_and_dodge_counter=1 mirrored_encounter=1"); return true; }
            beginEncounter(strategy + 1);
            keys = {};
        }
        input_->SetTestFrame(keys, mouse);
        return false;
    }
    if (frame > 102 && combatResolver_.Stats().defeated == 3 && impactFrame < 0) { impactFrame = frame; }
    if (preview && (frame == 100 || frame == 105 || frame == 161 || frame == 166 || frame == 174 || (impactFrame >= 0 &&
        (frame == impactFrame || frame == impactFrame + 5 || frame == impactFrame + 12)))) {
        phantomPreviewPaused_ = true;
        ImGui::SetNextWindowPos({ 20.0f, 165.0f }, ImGuiCond_Always);
        ImGui::Begin("Charge visual fixture", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted(frame == 100 ? "TEST ONLY: six enemy projectile styles" :
            frame >= 160 ? "TEST ONLY: fever burst / high chain" :
            "TEST ONLY: charge shot / three small ships + sniper");
        ImGui::Text("Frame: %d   Defeated: %d", frame, combatResolver_.Stats().defeated);
        const bool next = ImGui::Button("NEXT / F8", { 220.0f, 36.0f }) || ImGui::IsKeyPressed(ImGuiKey_F8, false);
        ImGui::End();
        if (!next) { input_->SetTestFrame(keys, mouse); return false; }
        phantomPreviewPaused_ = false;
    }
    ++frame;
    if (frame == 1) {
        CheckProjectileVisualProjection();
        CheckGameplayHudRendering(BuildHudFrame());
        log("HUD_OK hit=16,20,24,30 damage=26 direction=1 charge=1 reset=1 layouts=48 finite_vertices=1 draw_clocks_unchanged=1");
        CheckPlayerFlightEffects(playerFlightEffects_, player_->GetTranslate());
        UpdatePlayerFlightEffects();
        log("FLIGHT_EFFECTS_OK objects=24 trail_capacity=16 charge=2 fever_charge=0 lifetime=0.5,1,4 reset=1 draw_state_assertions=1");
        log("PROJECTILE_VISUAL_OK body_follows_velocity=1 projected_endpoints_match=1 head_on_round=1 near_clip_bounded=1");
        const auto reset = [&]() {
            DebugJumpToStagePhase(0);
            stageFlight_.DisableSpawnsForProbe();
            combatEffects_.Reset();
            combatProgress_.SetScoreForDebug(0);
            rewards_.Fever().SetGauge(0);
            rewards_.Fever().SetRemainingFramesForDebug(0);
            playerCombatFeedback_.ClearImpactSlowForDebug();
            gameplayCamera_.ClearShake();
        };
        // 編隊同時撃破＋着弾＋連射を重ねても、固定枠だけで全演出が出て返却されるか。
        for (const bool fever : { false, true }) {
            reset();
            combatProgress_.SetChainForDebug(12);
            const size_t capacity = combatEffects_.AvailableCount();
            for (int index = 0; index < 7; ++index) {
                const Math::Vector3 p{ static_cast<float>(index - 3), 1.0f, stageFlight_.Distance() + 32 };
                AddEnemyHitEffect(p, fever ? 1.08f : 1.32f, fever);
                combatEffects_.EnemyImpact(p, fever ? 1.38f : 1.42f, fever);
            }
            for (int index = 0; index < 12; ++index) {
                combatEffects_.PlayerShot(player_->GetTranslate(), true, rewards_.Fever().IsActive());
            }
            require(combatEffects_.MissCount() == 0, "Simultaneous combat burst exhausted fixed effect pool");
            for (int step = 0; step < 120; ++step) { UpdateHitEffects(); }
            require((combatEffects_.ActiveCount() == 0) && combatEffects_.AvailableCount() == capacity,
                "Combat burst retained or returned an effect object twice");
        }
        reset();
        AddEnemyHitEffect(player_->GetTranslate(), 1.0f);
        const float baseStrength = combatEffects_.LatestStrength();
        const size_t baseVisuals = combatEffects_.LatestVisualCount();
        reset();
        combatProgress_.SetChainForDebug(12);
        AddEnemyHitEffect(player_->GetTranslate(), 1.0f);
        require(combatEffects_.LatestStrength() > baseStrength && combatEffects_.LatestVisualCount() > baseVisuals,
            "Defeat chain failed to strengthen combat visuals");
        reset();
        log("COMBAT_BURST_OK simultaneous_kills=7 overlapping_impacts=7 muzzles=12 pools=0 fully_recycled=1 chain_escalates=1");
        const auto spawn = [&](float x, float y, int hp = 3, Enemy::Behavior behavior = Enemy::Behavior::Formation) {
            SpawnStageEnemyForProbe(x, y, 44.0f, behavior, Enemy::EntryStyle::TightFormation, hp, 1.0f);
            auto* enemy = stageEnemies_.Active().back().get();
            for (int step = 0; step < 120; ++step) { enemy->Update(stageFlight_.Distance()); }
            require(enemy->IsTargetable(), "fixture not targetable");
            return enemy;
        };
        const auto hit = [&](Enemy& target, bool charged, bool fever = false) {
            auto bullet = projectiles_.AcquirePlayerForDebug();
            require(bullet != nullptr, "fixture bullet pool exhausted");
            bullet->Initialize(object3dCommon_.get(), projectiles_.BodyModelForDebug(), target.GetAimPosition(), {},
                { 1, 1, 1, 1 }, 30, { 0.5f, 0.5f, 1 }, charged ? 1.0f : 0.42f, fever ? 3 : 1);
            bullet->SetFeverShot(fever);
            playerControl_.RecordShotForProbe();
            combatResolver_.ResolvePlayerShots(BuildCombatContext());
        };
        for (const bool charged : { false, true }) {
            reset();
            auto* target = spawn(0, 0, 1);
            hit(*target, charged);
            const int expected = charged ? 250 : 200;
            require(target->IsDead() && combatProgress_.Score() == expected, "defeat did not directly award former pickup bonus");
            combatResolver_.ResolvePlayerShots(BuildCombatContext());
            require(combatProgress_.Score() == expected, "defeat score awarded twice");
        }
        reset();
        auto* feverTarget = spawn(0, 0, 1);
        rewards_.Fever().SetRemainingFramesForDebug(600);
        hit(*feverTarget, true, true);
        require(combatProgress_.Score() == 750, "fever multiplier did not apply to direct defeat score");
        log("DIRECT_SCORE_OK normal=200 charged=250 fever=750 once=1 no_pickup_delay=1");

        reset();
        auto* center = spawn(0, 0);
        auto* neighbor = spawn(2.8f, 0);
        playerSkill_.SetRechargeForDebug(360.0f);
        hit(*center, false);
        require(center->GetHp() == 2 && neighbor->GetHp() == 3 && combatResolver_.Stats().directHits == 1 && rewards_.Fever().Gauge() == 0,
            "normal shot damage or no-splash rule changed");
        require(!playerCombatFeedback_.IsImpactSlowActive() && GetCinematicWorldTimeScale() == 1.0f && gameplayCamera_.ShakeFrames() == 0,
            "Ordinary hit interrupted continuous flight");
        log("NORMAL_OK direct=1 no_splash=1");
        require(playerSkill_.CooldownFrames() == 358.5f, "normal direct hit recovery changed");

        reset();
        center = spawn(0, 0, 6);
        neighbor = spawn(2.8f, 0);
        playerSkill_.SetRechargeForDebug(360.0f);
        hit(*center, true);
        require(center->GetHp() == 2 && neighbor->IsDead() && combatResolver_.Stats().defeated == 1 && combatResolver_.Stats().directHits == 1,
            "charge direct damage stacked, splash failed, or accuracy inflated");
        require(!playerCombatFeedback_.IsImpactSlowActive() && GetCinematicWorldTimeScale() == 1.0f,
            "Charged hit or splash kill slowed continuous flight");
        log("FLIGHT_TEMPO_OK normal_hit_no_slow_or_shake=1 charged_hit_and_splash_kill_no_slow=1");
        const int score = combatProgress_.Score(), gauge = rewards_.Fever().Gauge();
        require(gauge == 4, "splash defeat exceeded the new fever reward budget");
        combatResolver_.ResolvePlayerShots(BuildCombatContext());
        require(center->GetHp() == 2 && combatResolver_.Stats().defeated == 1 && combatProgress_.Score() == score && rewards_.Fever().Gauge() == gauge,
            "spent charge rewarded/damaged twice");
        log("SINGLE_HIT_OK direct=4 splash=3 reward_once=1 accuracy_once=1");
        require(playerSkill_.CooldownFrames() == 354.0f, "charge splash kill also recharged skill");

        reset();
        center = spawn(0, 0);
        auto* inside = spawn(3.39f, 0);
        auto* outside = spawn(-3.41f, 0);
        hit(*center, true);
        require(inside->IsDead() && outside->GetHp() == 3, "charge radius boundary wrong");
        log("BOUNDARY_OK radius=3.4 inside=3.39 outside=3.41");

        reset();
        center = spawn(-2.6f, 0);
        neighbor = spawn(0, 0);
        auto* opposite = spawn(2.6f, 0);
        hit(*center, true);
        require(center->IsDead() && neighbor->IsDead() && opposite->GetHp() == 3 && combatResolver_.Stats().defeated == 2,
            "edge hit chained across formation");
        log("NO_CHAIN_OK edge_hit_kills=2 opposite_ship_alive=1");

        reset();
        center = spawn(0, 0, 6);
        neighbor = spawn(4.0f, 0, 6); // 貫通弾そのものには触れない隣機。
        playerSkill_.SetRechargeForDebug(360.0f);
        hit(*center, true, true);
        combatResolver_.ResolvePlayerShots(BuildCombatContext());
        require(center->GetHp() == 4 && neighbor->GetHp() == 6 && !projectiles_.PlayerPool().Active().front()->IsDead() &&
            combatResolver_.Stats().directHits == 1 && combatEffects_.ActiveCount() == 1,
            "fever damage, piercing or duplicate-hit prevention changed");
        log("FEVER_OK direct=2 piercing_retained=1 no_splash=1 no_repeat=1");
        require(playerSkill_.CooldownFrames() == 360.0f, "fever piercing fire recharged skill");
        log("SKILL_RECOVERY_BUDGET_OK normal=1.5 charge_direct=6 splash=0 fever=0");

        reset();
        center = spawn(0, 0, 10, Enemy::Behavior::Shield);
        hit(*center, true);
        require(center->GetShieldHp() == 0 && center->GetHp() == 9, "charge failed to consume shield before hull damage");
        reset();
        center = spawn(0, 0, 52, Enemy::Behavior::Boss);
        hit(*center, true);
        require(center->GetHp() == 46, "boss direct damage doubled by own splash");
        bossAttack_.OpenCounterForDebug(30);
        hit(*center, true);
        require(center->GetHp() == 34, "boss counter damage changed");
        log("DEFENSE_OK shield_absorbs=3 overflow_to_hull=1 boss_direct=6 boss_counter=12");

        // 狙撃の反撃倍率は、指定の隙へ当てた通常チャージ直撃だけ。
        for (int mode = 0; mode < 6; ++mode) {
            reset();
            auto* sniper = spawn(0, 0, 6, Enemy::Behavior::Sniper);
            auto& fire = sniper->GetFireControl();
            const EnemyFireControl::Pattern pattern{ 60, 18, 128, 2, 22 };
            fire.Advance(1, true, pattern);
            const int elapsed = mode == 0 ? 85 : (mode == 5 ? 144 : 86);
            for (int step = 0; step < elapsed; ++step) { fire.Advance(1, true, pattern); }
            require(sniper->IsChargeCounterOpen() == (mode >= 1 && mode <= 4), "sniper counter window boundary incorrect");
            if (mode == 3) { combatResolver_.ApplyShotDamage(BuildCombatContext(), *sniper, sniper->GetAimPosition(), true, false, true); }
            else { hit(*sniper, mode != 2, mode == 4); }
            require(sniper->GetHp() == (mode == 1 ? 0 : mode == 2 ? 5 : mode == 4 ? 4 : mode == 3 ? 3 : 2),
                "sniper multiplier leaked to normal shots/fever/splash/outside window");
        }
        log("SNIPER_COUNTER_RULE_OK open=[8,66) charge_direct=8 normal=1 fever=2 splash=3 outside=4");

        reset();
        center = spawn(0, 0);
        neighbor = spawn(0, 0, 6, Enemy::Behavior::Sniper);
        const auto a = center->GetAimPosition(), b = neighbor->GetAimPosition();
        require(std::abs(a.z - b.z) > 3.4f, "depth fixture too close");
        hit(*center, true);
        require(neighbor->GetHp() == 6, "blast hit a distant enemy overlapping on screen");
        log("DEPTH_OK distant_ship_undamaged=1");

        reset();
        center = spawn(0, 0);
        neighbor = spawn(2.8f, 0);
        neighbor->Kill();
        const auto p = center->GetAimPosition();
        SpawnStageEnemyForProbe(p.x, p.y, p.z - stageFlight_.Distance(), Enemy::Behavior::Formation,
            Enemy::EntryStyle::Direct, 3, 1.0f);
        auto* entering = stageEnemies_.Active().back().get();
        require(!entering->IsTargetable(), "entry fixture already targetable");
        hit(*center, true);
        require(entering->GetHp() == 3 && combatResolver_.Stats().defeated == 1, "blast hit entry/dead enemy");
        log("LIFETIME_OK entry_ignored=1 destroyed_ignored=1");

        reset();
        // ここからは手動の命中配置ではなく、本編と同じ動く編隊へ実際に一発撃つ。
        SpawnStageEnemyForProbe(-4.6f, -0.6f, 44.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemyForProbe(-2.0f, 0.5f, 44.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemyForProbe(0.6f, -0.6f, 44.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemyForProbe(5.3f, 2.0f, 48.0f, Enemy::Behavior::Sniper, Enemy::EntryStyle::PopShooter, 6, 1.18f);
        log("LIVE_FORMATION_BEGIN normal_input=1");
    }
    if (preview && frame == 99) {
        // 映像確認だけで全種類の本物の敵弾を並べる。次の射撃試験へ危険弾を持ち越さない。
        const std::array<EnemyBulletStyle, 6> styles{ EnemyBulletStyle::Standard, EnemyBulletStyle::Crossfire,
            EnemyBulletStyle::Sniper, EnemyBulletStyle::ShieldOrb, EnemyBulletStyle::BossCannon, EnemyBulletStyle::BossCharge };
        for (size_t i = 0; i < styles.size(); ++i) {
            FireEnemyBullet({ -6.0f + static_cast<float>(i) * 2.4f, 4.0f, stageFlight_.Distance() + 26.0f }, styles[i]);
        }
        log("ENEMY_PROJECTILE_PREVIEW real_styles=6 cleared_before_charge_test=1");
    }
    if (preview && frame == 101) {
        for (auto& bullet : projectiles_.EnemyPool().Active()) { bullet->Kill(); }
    }
    if (preview && frame == 160) {
        const int chain = combatProgress_.ChainCount();
        combatProgress_.SetChainForDebug(12);
        AddEnemyHitEffect({ 2.2f, 2.0f, stageFlight_.Distance() + 26.0f }, 1.08f, true);
        combatProgress_.SetChainForDebug(chain);
        log("FEVER_BURST_PREVIEW real_effect=1 high_chain=12 gameplay_unchanged=1");
    }
    if (frame == 101 || frame == 102) {
        Math::Vector2 screen{};
        require(TryProjectToScreen((*std::next(stageEnemies_.Active().begin()))->GetAimPosition(), screen), "center offscreen");
        const ImVec2 origin = ImGui::GetMainViewport()->Pos;
        mouse = { screen.x - origin.x, screen.y - origin.y };
    }
    if (frame == 102) {
        require(playerControl_.Shooting().IsChargeReady(), "normal waiting did not charge shot");
        require(playerControl_.Aim().Target() == (*std::next(stageEnemies_.Active().begin())).get() && playerControl_.Aim().IsReticleOnTarget(),
            "fixture did not aim at formation center");
        keys[DIK_SPACE] = 0x80;
    }
    if (frame == 190) {
        log("LIVE_RESULT kills=" + std::to_string(combatResolver_.Stats().defeated) + " shots=" +
            std::to_string(playerControl_.ShotsFired()) + " hits=" + std::to_string(combatResolver_.Stats().directHits));
        require(combatResolver_.Stats().defeated == 3 && playerControl_.ShotsFired() == 1 && combatResolver_.Stats().directHits == 1,
            "real charged projectile failed to clear moving three-ship formation");
        require(stageEnemies_.Active().size() == 1 && stageEnemies_.Active().front()->IsSniper() && stageEnemies_.Active().front()->GetHp() == 6,
            "real splash reached distant sniper");
        require(projectiles_.PlayerPool().MissCount() == 0 && combatEffects_.MissCount() == 0, "effect/bullet pool exhausted");
        log("LIVE_FORMATION_PASS real_projectile_kills=3 sniper_hp=6 shots=1 hits=1 pool_misses=0");
        beginEncounter(1);
        input_->SetTestFrame(keys, mouse);
        return false;
    }
    input_->SetTestFrame(keys, mouse);
    return false;
}
// リングを通過したときの判定、得点、加速、チャージ補充を検証する。
bool GameRuntime::RunRiftProbe(const std::string& logPath, bool preview)
{
    static int frame = 0;
    static int phase = 0;
    static int phaseFrame = 0;
    static int previewStep = 0;
    static bool fired = false;
    static bool liveGateSeen = false;
    static bool firstPass = false;
    static int gaugeBeforePass = 0;
    static int feverBeforePass = 0;
    const auto log = [&](const std::string& message) {
        std::ofstream file(logPath, std::ios::app);
        file << "RIFT_TEST " << message << '\n';
    };
    const auto require = [&](bool ok, const char* message) {
        if (!ok) { log(std::string("FAIL ") + message); throw std::runtime_error(message); }
    };
    std::array<BYTE, 256> keys{};
    Math::Vector2 mouse = input_->GetMousePosition();
    const auto reset = [&]() {
        DebugJumpToStagePhase(0);
        stageFlight_.DisableSpawnsForProbe();
        combatEffects_.Reset();
        combatProgress_.Reset(); // フェーズ移動は本来得点を保持する。独立した報酬試験では毎回初期化する。
        rewards_.Fever().SetGauge(0);
        rewards_.Fever().SetRemainingFramesForDebug(0);
        playerCombatFeedback_.ClearImpactSlowForDebug();
        gameplayCamera_.ClearShake();
        playerControl_.Shooting().Reset();
        playerControl_.Shooting().FillCharge();
    };
    const auto activeGate = [&]() -> const RingGates::Gate* {
        for (auto& rift : ringGates_.Gates()) { if (rift.active) { return &rift; } }
        return nullptr;
    };
    if (preview) {
        const RingGates::Gate* gate = activeGate();
        const bool stop = (previewStep == 0 && phaseFrame == 2) ||
            (previewStep == 1 && gate && gate->age >= 14.0f) ||
            (previewStep == 2 && gate && gate->position.z - stageFlight_.Distance() < 12.0f) ||
            (previewStep == 3 && firstPass && ringGates_.BoostAge() >= 8.0f) ||
            (previewStep == 4 && phase == 1 && ringGates_.PassCount() == 1);
        if (stop) {
            phantomPreviewPaused_ = true;
            ImGui::SetNextWindowPos({ 20.0f, 165.0f }, ImGuiCond_Always);
            ImGui::Begin("Rift visual fixture", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextUnformatted("TEST ONLY: course gate -> pass -> boost");
            ImGui::Text("Step %d  Passes %d  Gauge %d  Fever %d", previewStep,
                ringGates_.PassCount(), rewards_.Fever().Gauge(), rewards_.Fever().RemainingFrames());
            const bool next = ImGui::Button("NEXT / F8", { 220.0f, 36.0f }) || ImGui::IsKeyPressed(ImGuiKey_F8, false);
            ImGui::End();
            if (!next) { input_->SetTestFrame(keys, mouse); return false; }
            ++previewStep;
            phantomPreviewPaused_ = false;
        }
    }
    ++frame;
    ++phaseFrame;
    // 固定配置の射撃・通過試験に、本編の増援やボスを混ぜない。
    stageFlight_.HoldProgressForProbe(0.0f);
    if (frame == 1) {
        reset();
        const float z = stageFlight_.Distance() + 30.0f;
        const Math::Vector3 location{ 2.5f, 1.6f, z };
        SpawnRift(location);
        require(!bossPresentation_.HasRadio(), "rift displayed routine combat radio");
        SpawnStageEnemyForProbe(3, 1, 44, Enemy::Behavior::Sniper, Enemy::EntryStyle::Direct, 3, 1);
        require(!bossPresentation_.HasRadio(), "sniper displayed routine combat radio");
        rewards_.Fever().SetRemainingFramesForDebug(420);
        SpawnRift(location);
        require(!bossPresentation_.HasRadio() && !bossPresentation_.WasRadioRequestedForProbe(),
            "fever rift displayed routine combat radio");
        RequestBossRadio();
        UpdateBossRadio(0.05f);
        const float radioRemaining = bossPresentation_.RadioRemaining();
        gameplayMenu_.SetPausedForDebug(true);
        UpdateBossRadio(0.1f);
        require(bossPresentation_.RadioRemaining() == radioRemaining, "paused radio advanced");
        gameplayMenu_.SetPausedForDebug(false);
        RequestBossRadio();
        require(bossPresentation_.RadioRemaining() == radioRemaining, "repeated boss arrival restarted radio");
        for (int step = 0; step < 40; ++step) { UpdateBossRadio(0.1f); }
        require(!bossPresentation_.HasRadio(), "radio failed to expire");
        RequestBossRadio();
        require(!bossPresentation_.HasRadio(), "boss arrival repeated after expiry");
        reset();
        require(!bossPresentation_.WasRadioRequestedForProbe() && bossPresentation_.RadioRemaining() == 0,
            "retry retained radio history");
        playMode_ = PlayMode::Tutorial;
        RequestBossRadio();
        require(!bossPresentation_.HasRadio(), "combat radio leaked into training");
        playMode_ = PlayMode::Game;
        RequestBossRadio();
        isGameClear_ = true;
        UpdateBossRadio(0.1f);
        require(!bossPresentation_.HasRadio(), "radio remained on results");
        isGameClear_ = false;
        log("RADIO_CONTEXT_OK normal_rift_silent=1 fever_rift_silent=1 sniper_silent=1");
        log("RADIO_OK boss_arrival_only=1 once=1 pause=1 retry=1 tutorial=1 results=1");
        reset();
        const auto cross = [&](float x, float y) {
            UpdateRifts({ x, y, z - 2.0f }, { x, y, z + 2.0f });
        };
        stageFlight_.ResetScheduleForProbe(30.0f);
        const float startDistance = stageFlight_.Distance();
        const float startProgress = stageFlight_.Schedule().Progress();
        PrepareStageFrame();
        require(stageFlight_.Distance() == startDistance && stageFlight_.Schedule().Progress() == startProgress,
            "stage preparation advanced rail or encounter progress");
        require(ringGates_.SpawnCount() == 1 && activeGate() &&
            activeGate()->position.x == MainStage::GateSpawns().front().x &&
            activeGate()->position.z - startDistance >= 68.0f, "course gate did not appear ahead of combat");
        PrepareStageFrame();
        require(ringGates_.SpawnCount() == 1, "course gate spawned twice");
        SpawnStageEnemyForProbe(0.6f, 0.5f, 44, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 3, 1);
        Enemy* fixtureEnemy = stageEnemies_.Active().back().get();
        for (int step = 0; step < 120; ++step) { fixtureEnemy->Update(stageFlight_.Distance()); }
        fixtureEnemy->Kill();
        combatResolver_.RegisterEnemyDefeat(BuildCombatContext(), *fixtureEnemy, true, false);
        require(ringGates_.SpawnCount() == 1, "enemy kill created an extra gate");
        SpawnStageEnemyForProbe(3, 1, 44, Enemy::Behavior::Sniper, Enemy::EntryStyle::Direct, 3, 1);
        Enemy* ordinary = stageEnemies_.Active().back().get();
        ordinary->Kill();
        combatResolver_.RegisterEnemyDefeat(BuildCombatContext(), *ordinary, false, false);
        require(ringGates_.SpawnCount() == 1, "ordinary kill spawned a rift");
        log("COURSE_GATE_OK visible_before_kill=1 ordered_once=1 enemy_kill_independent=1");

        reset();
        SpawnRift(location);
        const float age = activeGate()->age;
        gameplayMenu_.SetPausedForDebug(true);
        cross(location.x, location.y);
        require(activeGate() && activeGate()->age == age && ringGates_.PassCount() == 0, "paused gate advanced");
        gameplayMenu_.SetPausedForDebug(false);
        cross(location.x + RingGates::kRadiusX * 0.8f, location.y + RingGates::kRadiusY * 0.8f);
        require(!activeGate() && ringGates_.PassCount() == 0 && rewards_.Fever().Gauge() == 0, "ellipse corner miss rewarded");
        SpawnRift(location);
        playerControl_.Shooting().SetChargeForDebug(0);
        playerControl_.Shooting().SetCooldownForDebug(20);
        combatProgress_.SetChainForDebug(2, 1);
        UpdateRifts({ location.x - 4.0f, location.y, z - 2.0f },
            { location.x + 4.0f, location.y, z + 2.0f });
        require(ringGates_.PassCount() == 1 && rewards_.Fever().Gauge() == CombatRewards::kRingGaugeReward && playerControl_.Shooting().ChargeFrames() == 100 &&
            playerControl_.Shooting().CooldownFrames() == 4 && combatProgress_.ChainRemainingFrames() > 200 &&
            combatProgress_.Score() == kRiftPassScore, "swept pass failed to grant score/charge/gauge/chain");
        require(audio_.SoundForProbe() && audio_.SoundForProbe()->GetVoiceCount() == 29 && audio_.SoundForProbe()->GetPlayCount() > 0,
            "dedicated gate air rush did not play while legacy effects were muted");
        log("GATE_AUDIO_OK dedicated_voice=1 legacy_effects_muted=1");
        cross(location.x, location.y);
        require(ringGates_.PassCount() == 1 && rewards_.Fever().Gauge() == CombatRewards::kRingGaugeReward && combatProgress_.Score() == kRiftPassScore,
            "rift rewarded twice");
        SpawnRift(location);
        cross(location.x + RingGates::kRadiusX * 1.01f, location.y);
        require(ringGates_.PassCount() == 1, "outside aperture rewarded");
        SpawnRift(location);
        cross(location.x + RingGates::kRadiusX * 0.99f, location.y);
        require(ringGates_.PassCount() == 2, "inside aperture missed");
        log("CROSSING_OK swept_xy=1 ellipse=1 boundary=1 reward_once=1 pause_freezes=1");

        reset();
        SpawnRift(location);
        SpawnRift({ location.x, location.y, z + 1.0f });
        UpdateRifts({ location.x, location.y, z - 2.0f }, { location.x, location.y, z + 2.0f });
        require(ringGates_.PassCount() == 2 && rewards_.Fever().Gauge() == CombatRewards::kRingGaugeReward * 2 &&
            combatProgress_.Score() == kRiftPassScore * 2 && playerControl_.Shooting().ChargeFrames() == 100,
            "multiple passes in one update lost rewards");
        log("MULTI_PASS_OK crossings=2 reward_per_gate=1");

        reset();
        const auto still = player_->GetTranslate();
        PrepareStageFrame();
        const float cruise = stageFlight_.TargetSpeedForProbe();
        ringGates_.StartBoost();
        float peak = 0.0f;
        for (int step = 0; step < 80; ++step) {
            peak = (std::max)(peak, ringGates_.BoostStrength());
            PrepareStageFrame();
            require(stageFlight_.TargetSpeedForProbe() <= cruise * 1.651f, "gate acceleration stacks without a cap");
            ringGates_.Update(still, still, 1.0f);
        }
        PrepareStageFrame();
        require(peak > 0.99f && ringGates_.BoostStrength() == 0.0f && std::abs(stageFlight_.TargetSpeedForProbe() - cruise) < 0.001f,
            "gate boost failed to peak and return to cruise");
        ringGates_.StartBoost();
        ringGates_.Update(still, still, 10.0f);
        ringGates_.StartBoost();
        require(ringGates_.BoostAge() == 0.0f && ringGates_.BoostStrength() <= 1.0f, "second pass stacked boost speed");
        log("BOOST_ENVELOPE_OK peak=1 duration=66 speed_cap=1.65 smooth_release=1 retrigger_no_stack=1");

        reset();
        rewards_.Fever().SetRemainingFramesForDebug(kFeverDurationFrames - 90);
        SpawnRift(location);
        cross(location.x, location.y);
        require(rewards_.Fever().RemainingFrames() == kFeverDurationFrames && rewards_.Fever().Gauge() == 0 && rewards_.RingRecoveredFrames() == 90, "fever recovery wrong");
        rewards_.Fever().SetRemainingFramesForDebug(kFeverDurationFrames - 30);
        SpawnRift(location);
        cross(location.x, location.y);
        require(rewards_.Fever().RemainingFrames() == kFeverDurationFrames && rewards_.RingRecoveredFrames() == 30, "fever duration cap failed");
        log("FEVER_OK recovery=90 cap=360 actual_recovery_display=1");

        reset();
        for (size_t index = 0; index < ringGates_.Gates().size() + 1; ++index) { SpawnRift(location); }
        require(ringGates_.SpawnCount() == static_cast<int>(ringGates_.Gates().size()), "gate pool overflow replaced a live gate");
        for (int step = 0; step < 480; ++step) {
            ringGates_.Update({ 0, 0, z - 10 }, { 0, 0, z - 9 }, 1.0f);
        }
        require(!ringGates_.Gates().front().active && rewards_.Fever().Gauge() == 0, "expired gate rewarded");
        reset();
        require(!activeGate() && ringGates_.PassCount() == 0 && rewards_.RingNoticeFrames() == 0 &&
            ringGates_.BoostStrength() == 0.0f && ringGates_.BoostAge() < 0.0f, "retry retained gates/rewards/boost");
        playMode_ = PlayMode::Tutorial;
        SpawnRift(location);
        require(!activeGate(), "main-only rift leaked into tutorial");
        playMode_ = PlayMode::Game;
        SpawnRift(location);
        isGameOver_ = true;
        cross(location.x, location.y);
        require(!activeGate() && rewards_.Fever().Gauge() == 0, "game-over gate rewarded");
        isGameOver_ = false;
        log("LIFETIME_OK fixed_pool=8 expired_no_reward=1 retry_resets=1 tutorial_unchanged=1 game_over=1");

        reset();
        Math::Vector3 movedMuzzle{ 6.0f, 3.0f, stageFlight_.Distance() + 1.75f };
        const Math::Vector3 aimPoint{ -1.0f, 2.0f, movedMuzzle.z + 45.0f };
        Math::Vector2 aimScreen{};
        require(TryProjectToScreen(aimPoint, aimScreen), "aim fixture offscreen");
        playerControl_.Aim().SetReticleForProbe(aimScreen);
        const auto direction = playerControl_.AimDirection(movedMuzzle, GetPlayerAimView());
        const float distance = (aimPoint.z - movedMuzzle.z) / direction.z;
        require(std::abs(movedMuzzle.x + direction.x * distance - aimPoint.x) < 0.02f &&
            std::abs(movedMuzzle.y + direction.y * distance - aimPoint.y) < 0.02f,
            "unassisted shot did not converge from displaced muzzle");
        log("AIM_OK displaced_muzzle_x=6 cursor_convergence=1");

        SpawnStageEnemyForProbe(4, 1, 44, Enemy::Behavior::Formation, Enemy::EntryStyle::Direct, 4, 1);
        auto* unselected = stageEnemies_.Active().back().get();
        unselected->SetTrainingTarget(true);
        for (int step = 0; step < 120; ++step) { unselected->Update(stageFlight_.Distance()); }
        rewards_.Fever().SetRemainingFramesForDebug(kFeverDurationFrames);
        playerControl_.Aim().ClearTarget();
        keys[DIK_SPACE] = 0x80;
        input_->SetTestFrame(keys, mouse);
        UpdatePlayerShooting();
        require(projectiles_.PlayerPool().Active().size() == 1 &&
            !projectiles_.PlayerPool().Active().back()->CanHome(), "Unaimed fever shot failed or acquired an enemy automatically");
        log("FEVER_AIM_OK unselected_enemy_not_acquired=1");
        reset();

        // 外れ弾が寿命まで残る最悪条件で、10秒以上の実際の射撃・弾更新を検証する。
        rewards_.Fever().SetRemainingFramesForDebug(600);
        for (int step = 0; step < 620; ++step) {
            playerControl_.Shooting().FillCharge();
            UpdatePlayerShooting();
            projectiles_.UpdatePlayerBullets();
            UpdateHitEffects();
        }
        require(playerControl_.ShotsFired() >= 68 && projectiles_.PlayerPool().MissCount() == 0 && combatEffects_.MissCount() == 0,
            "rapid fever fire exhausted fixed pools");
        log("RAPID_FIRE_OK shots=" + std::to_string(playerControl_.ShotsFired()) + " pool_misses=0");
        keys.fill(0);
        reset();
        SpawnRift({ 4.0f, 1.4f, stageFlight_.Distance() + 68.0f });
        SpawnStageEnemyForProbe(-3.0f, 1.4f, 44, Enemy::Behavior::Formation, Enemy::EntryStyle::Direct, 3, 1);
        fixtureEnemy = stageEnemies_.Active().back().get();
        fixtureEnemy->SetTrainingTarget(true);
        for (int step = 0; step < 120; ++step) { fixtureEnemy->Update(stageFlight_.Distance()); }
        log("LIVE_NORMAL_BEGIN gate_before_kill=1 real_projectile=1 real_player_movement=1");
    }
    if (!fired && !stageEnemies_.Active().empty()) {
        Math::Vector2 screen{};
        require(TryProjectToScreen(stageEnemies_.Active().front()->GetAimPosition(), screen), "live enemy offscreen");
        const auto origin = ImGui::GetMainViewport()->Pos;
        mouse = { screen.x - origin.x, screen.y - origin.y };
        if (phaseFrame >= 4) {
            require(playerControl_.Aim().IsReticleOnTarget(), "live fixture failed to lock target");
            keys[DIK_SPACE] = 0x80;
            fired = true;
        }
    }
    if (const RingGates::Gate* gate = activeGate()) {
        liveGateSeen = true;
        gaugeBeforePass = rewards_.Fever().Gauge();
        const auto position = player_->GetTranslate();
        const float x = gate->position.x - position.x;
        const float y = gate->position.y - position.y;
        if (std::abs(x) > 0.15f) { keys[x > 0 ? DIK_D : DIK_A] = 0x80; }
        if (std::abs(y) > 0.15f) { keys[y > 0 ? DIK_W : DIK_S] = 0x80; }
        feverBeforePass = rewards_.Fever().RemainingFrames();
    }
    if (phase == 0 && ringGates_.PassCount() == 1) {
        require(liveGateSeen && playerControl_.ShotsFired() == 1 && combatResolver_.Stats().defeated == 1 &&
            rewards_.Fever().Gauge() == gaugeBeforePass + CombatRewards::kRingGaugeReward && playerControl_.Shooting().ChargeFrames() == 100,
            "real kill/steering pass failed normal rewards");
        if (!firstPass) { firstPass = true; log("LIVE_NORMAL_OK real_kill=1 steered_pass=1 gauge_plus=14 charge_ready=1"); }
        if ((!preview && ringGates_.BoostAge() >= 8.0f) || previewStep > 3) {
            reset();
            ActivateFever();
            rewards_.Fever().SetRemainingFramesForDebug(300); // 上限とは別に1.5秒の実回復を検証する。
            phase = 1;
            phaseFrame = 0;
            fired = false;
            liveGateSeen = false;
            SpawnRift({ -2.0f, 2.0f, stageFlight_.Distance() + (std::max)(80.0f, stageFlight_.Speed() * 110.0f) });
            SpawnStageEnemyForProbe(3.0f, 2.0f, 44, Enemy::Behavior::Formation, Enemy::EntryStyle::Direct, 2, 1);
            auto* enemy = stageEnemies_.Active().back().get();
            enemy->SetTrainingTarget(true);
            for (int step = 0; step < 120; ++step) { enemy->Update(stageFlight_.Distance()); }
            log("LIVE_FEVER_BEGIN remaining_frames=300");
        }
    }
    if (phase == 1 && ringGates_.PassCount() == 1 && (!preview || previewStep > 4)) {
        log("LIVE_FEVER_RESULT before=" + std::to_string(feverBeforePass) + " after=" +
            std::to_string(rewards_.Fever().RemainingFrames()) + " recovered=" + std::to_string(rewards_.RingRecoveredFrames()) +
            " shots=" + std::to_string(playerControl_.ShotsFired()) + " kills=" + std::to_string(combatResolver_.Stats().defeated));
        require(liveGateSeen && rewards_.Fever().RemainingFrames() == feverBeforePass - 1 + CombatRewards::kRingFeverRecoveryFrames &&
            playerControl_.ShotsFired() == 1 && combatResolver_.Stats().defeated == 1 && rewards_.RingRecoveredFrames() == 90,
            "real fever kill/steering pass failed duration recovery");
        require(projectiles_.PlayerPool().MissCount() == 0 && combatEffects_.MissCount() == 0, "live pool exhaustion");
        log("PASS normal_and_fever_real_shot_pass=1 fever_plus=90 pools=0");
        input_->SetTestFrame({}, mouse);
        return true;
    }
    require(frame < 700, "live rift steering timed out or missed gate");
    input_->SetTestFrame(keys, mouse);
    return false;
}
#endif
