// 通常起動やReleaseには自動操作を含めない。明示したDebug試験だけで使用。
#ifdef _DEBUG
#include "app/GameRuntime.h"
#include "engine/io/Input.h"
#include "engine/audio/SoundManager.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <iterator>
#include <cstdlib>

namespace {
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
}

// 実際のPlayerと入力経路を使う回避回帰試験。描画しない独立自機なので本編へ影響しない。
void CheckDodgeControls(Object3dCommon* common, Model* model)
{
    const auto require = [](bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    };
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
                probe.Update(&input, slow, step);
                require(probe.IsInvincible(), "dodge lost protection before movement ended");
                if (frame == 0 && step == 1.0f) {
                    require(probe.GetTranslate().x > 0.50f, "dodge initial response too slow");
                }
            }
            require(!probe.IsDodging() && std::abs(probe.GetTranslate().x - 4.4f) < 0.001f,
                "dodge distance/duration depends on slow motion or frame rate");
            input.SetTestFrame({}, {});
            probe.Update(&input, slow, step);
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
            probe.Update(&input);
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
    for (int frame = 0; frame < 60; ++frame) { edge.Update(&input); }
    const float limit = edge.GetTranslate().x;
    for (int frame = 0; frame < 16; ++frame) {
        std::array<BYTE, 256> keys{};
        if (frame == 0) { keys[DIK_D] = keys[DIK_LSHIFT] = 0x80; }
        input.SetTestFrame(keys, {});
        edge.Update(&input);
        require(std::abs(edge.GetTranslate().x - limit) < 0.001f, "dodge crossed playfield boundary");
    }
    std::array<BYTE, 256> left{};
    left[DIK_A] = 0x80;
    input.SetTestFrame(left, {});
    edge.Update(&input, 0.08f);
    require(!edge.IsDodging() && edge.GetTranslate().x < limit - 0.15f,
        "movement did not resume promptly after edge dodge in slow motion");
}
} // namespace

bool GameRuntime::RunPlaythroughProbe(const std::string& logPath, bool tutorialPreview)
{
    static bool wasTutorial = false;
    if (IsTutorial()) { wasTutorial = true; return RunTutorialProbe(logPath, tutorialPreview); }
    if (wasTutorial) {
        if (player_->GetHp() != 100 || score_ != 0 || tutorial_.started ||
            feverActivationCount_ != 0 || isGameClear_ || isMainGameRequested_) {
            throw std::runtime_error("Main game inherited tutorial state");
        }
        std::ofstream file(logPath, std::ios::app);
        file << "TUTORIAL MAIN_ENTRY_OK fresh_hp_score_fever=1\n";
        wasTutorial = false;
    }
    static int phase = 0; // 0: 通常戦闘 1: クリア後の再挑戦待ち 2: 完了
    static int frame = 0;
    static int resultFrames = 0;
    static float longestEmptyGap = 0.0f;
    static int encounterGapCount = 0;
    static bool wasEmpty = false;
    static std::vector<float> pausedState;
    static int delayedEscapeEvents = 0;
    static unsigned int sceneryCoverage = 0;
    static unsigned int sceneryPreviewSeen = 0;
    static bool bossApronSeen = false;
    static bool flightReleaseSeen = false;
    static float maximumFlightSpeed = 0.0f;
    static float minimumCameraBank = 0.0f;
    static float maximumCameraBank = 0.0f;
    static float maximumFlightBlur = 0.0f;
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
        if (player_->GetHp() != 100 || score_ != 0 || feverGauge_ != 0 ||
            feverTimer_ != 0 || feverActivationCount_ != 0 ||
            playerShotsFired_ != 0 || !enemies_.empty() ||
            sceneryCanyonStartZ_ != 450.0f || sceneryPlazaStartZ_ != 866.0f ||
            flightReleaseKick_ != 0.0f || previousFlightTimeScale_ != 1.0f || sfxAccentUntil_ != 0.0f ||
            // 再入場後の最初の通常Updateは実行済み。初速への小さな反応だけを許容する。
            flightCameraMotion_.blurStrength > 0.02f || std::abs(flightCameraMotion_.acceleration) > 0.03f ||
            std::any_of(stageEnemyEventTriggered_.begin(), stageEnemyEventTriggered_.end(), [](bool v) { return v; })) {
            throw std::runtime_error("Retry did not reset gameplay state");
        }
        // 再入場の最初のUpdateは既に通っているので、新しい通常曲が一つだけ再生される。
        if (!sound_ || sound_->GetVoiceCount() != 28 || sound_->GetPlayCount() != 0 ||
            sound_->GetLoopCount() != 1 || musicTrack_ != 0) {
            throw std::runtime_error("Retry did not recreate the bounded audio bank");
        }
        log("RETRY_RESET_OK hp=100 score=0 fever=0 voices=28 music_loops=1 track=stage scenery=avenue camera_inertia_reset=1 sfx_priority_reset=1");
        phase = 2;
        input_->SetTestFrame(keys, mouse);
        return true;
    }
    if (phase == 2) {
        return true;
    }
    // 通常プレイを最後まで通し、街区の到達と、表示中の建物が変形しないことを検証する。
    const auto district = GetSceneryDistrictWeights(railDistance_ + 48.0f);
    const unsigned int districtBit = district.y > 0.98f ? 4u :
        (district.x > 0.98f && district.y < 0.02f ? 2u : (district.x < 0.02f ? 1u : 0u));
    if (districtBit != 0 && (sceneryCoverage & districtBit) == 0) {
        sceneryCoverage |= districtBit;
        log("SCENERY_DISTRICT bit=" + std::to_string(districtBit) +
            " stage=" + std::to_string(stageProgress_) + " rail=" + std::to_string(railDistance_));
    }
    if (previousScenery.empty()) { previousScenery.resize(railSceneryObjects_.size()); }
    if (previousScenery.size() != railSceneryObjects_.size()) {
        throw std::runtime_error("Scenery object count changed during play");
    }
    if (std::count_if(railSceneryObjects_.begin(), railSceneryObjects_.end(),
        [](const auto& object) { return object.isTrackside; }) != 18) {
        throw std::runtime_error("Flight trackside pool must contain exactly eighteen shared modules");
    }
    // 地表の実際の表示範囲を毎フレーム検証する。街区到達やビルド成功では穴を検出できない。
    constexpr float kTerrainSurfaceY = -3.5f;
    std::array<Math::Vector2, 4> terrainIntervals{};
    size_t terrainCount = 0;
    for (const auto& scenery : railSceneryObjects_) {
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
    float coveredUntil = railDistance_ - 40.0f;
    for (size_t index = 0; index < terrainCount; ++index) {
        const auto& interval = terrainIntervals[index];
        if (interval.y < coveredUntil) { continue; }
        if (interval.x > coveredUntil + 0.01f) {
            throw std::runtime_error("Visible ground has a gap between terrain tiles");
        }
        coveredUntil = (std::max)(coveredUntil, interval.y);
    }
    if (coveredUntil < railDistance_ + 500.0f) {
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
                std::any_of(railSceneryObjects_.begin(), railSceneryObjects_.end(), [&](const auto& scenery) {
                    if (!scenery.isTerrain || !scenery.object || !scenery.isVisible || !scenery.isInView) { return false; }
                    const float z = scenery.object->GetTranslate().z;
                    return hit.z >= z - scenery.halfDepth - 0.01f && hit.z <= z + scenery.halfDepth + 0.01f;
                });
            if (!groundCovered) {
                throw std::runtime_error("Camera saw sky through an uncovered ground ray");
            }
        }
    }
    maximumFlightSpeed = (std::max)(maximumFlightSpeed, railSpeed_);
    minimumCameraBank = (std::min)(minimumCameraBank, cameraRotate_.z);
    maximumCameraBank = (std::max)(maximumCameraBank, cameraRotate_.z);
    maximumFlightBlur = (std::max)(maximumFlightBlur, flightCameraMotion_.blurStrength);
    if (!std::isfinite(cameraRotate_.x) || !std::isfinite(cameraRotate_.y) || !std::isfinite(cameraRotate_.z) ||
        std::abs(cameraRotate_.z) > 0.18f || std::abs(cameraRotate_.x) > 0.25f ||
        !std::isfinite(flightCameraMotion_.blurStrength) || flightCameraMotion_.blurStrength < 0.0f ||
        flightCameraMotion_.blurStrength > 0.851f) {
        throw std::runtime_error("Flight inertia/blur left its readable envelope");
    }
    flightReleaseSeen |= flightReleaseKick_ > 0.5f;
    if (frame > 120 && (!std::isfinite(cameraFovY_) || cameraFovY_ < 0.65f || cameraFovY_ > 1.15f ||
        railDistance_ - cameraTranslate_.z < 8.0f || railDistance_ - cameraTranslate_.z > 20.5f)) {
        throw std::runtime_error("Flight camera left its readable FOV/distance envelope");
    }
    for (size_t i = 0; i < railSceneryObjects_.size(); ++i) {
        const auto& scenery = railSceneryObjects_[i];
        if (scenery.isTrackside && (scenery.halfDepth != 18.0f || scenery.loopLength != 648.0f)) {
            throw std::runtime_error("Trackside wrap does not include full module depth");
        }
        if (scenery.isLandmark && scenery.object) {
            const auto position = scenery.object->GetTranslate();
            if (std::abs(position.z - scenery.anchor.z) > 0.001f ||
                std::abs(scenery.currentLocalZ + railDistance_ - scenery.anchor.z) > 0.001f ||
                scenery.loopLength != 0.0f || scenery.halfDepth < 53.7f) {
                throw std::runtime_error("Landmark recycled or culled without its full bounds");
            }
        }
        if (!scenery.object) { continue; }
        const float bossApronStartZ = GetBossApronStartZ();
        if (scenery.isVisible && bossApronStartZ != -1.0f &&
            (scenery.isRoad || scenery.isTrackside || scenery.isRoadDetail) &&
            scenery.object->GetTranslate().z + (scenery.isRoad ? 9.0f : scenery.halfDepth) >= bossApronStartZ) {
            throw std::runtime_error("Car road or trackside equipment continued into boss apron");
        }
        if (scenery.isBossApron && scenery.isVisible &&
            std::abs(scenery.object->GetTranslate().z - railDistance_) < scenery.halfDepth) {
            bossApronSeen = true;
        }
        if (!scenery.isBuilding && !scenery.isRoad && !scenery.isDefenseDistrict &&
            !scenery.isBossApron && !scenery.isTerrain) { continue; }
        const ScenerySnapshot current{ scenery.object->GetTranslate(), scenery.object->GetScale(), scenery.isVisible };
        const auto& previous = previousScenery[i];
        if (frame > 1 && !previous.visible && current.visible &&
            current.position.z - scenery.halfDepth < railDistance_ + 500.0f &&
            current.position.z + scenery.halfDepth > railDistance_ - 40.0f) {
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
    const float defenseStartZ = sceneryCanyonStartZ_ >= 0.0f ?
        (std::max)(sceneryCanyonStartZ_, 450.0f) : sceneryCanyonStartZ_;
    const float bossApronStartZ = GetBossApronStartZ();
    unsigned int previewBit = sceneryPreviewMode == 3 ?
        (frame == 351 ? 1u : frame == 401 ? 2u : frame == 451 ? 4u :
            feverTimer_ > 0 && flightCameraMotion_.blurStrength > 0.32f ? 8u : 0u) : sceneryPreviewMode == 2 ?
        (railDistance_ >= 325.0f ? 8u : railDistance_ >= 245.0f ? 4u :
            railDistance_ >= 185.0f ? 2u : railDistance_ >= 100.0f ? 1u : 0u) : sceneryPreviewMode == 4 ?
        (sceneryPlazaStartZ_ != -1.0f && railDistance_ >= sceneryPlazaStartZ_ + 80.0f ? 4u :
            sceneryCanyonStartZ_ != -1.0f && railDistance_ >= defenseStartZ + 140.0f ? 2u :
            sceneryCanyonStartZ_ != -1.0f && railDistance_ >= defenseStartZ - 60.0f ? 1u : 0u) : sceneryPreviewMode == 5 ?
        (bossSpawned_ && bossIntroTimer_ <= 0 ? 4u :
            bossApronStartZ != -1.0f && railDistance_ >= bossApronStartZ + 90.0f ? 2u :
            bossApronStartZ != -1.0f && railDistance_ >= bossApronStartZ - 40.0f ? 1u : 0u) : districtBit;
    if (sceneryPreviewMode == 6) {
        // 全区間の細部を一度の通常プレイで確認。停止点は既存の画面確認専用の経路だけで使う。
        previewBit = 0;
        if (railDistance_ >= 180.0f && !(sceneryPreviewSeen & 1u)) { previewBit = 1u; }
        else if (sceneryCanyonStartZ_ != -1.0f && railDistance_ >= defenseStartZ + 40.0f &&
            !(sceneryPreviewSeen & 2u)) { previewBit = 2u; }
        else if (sceneryCanyonStartZ_ != -1.0f && railDistance_ >= defenseStartZ + 175.0f &&
            !(sceneryPreviewSeen & 4u)) { previewBit = 4u; }
        else if (bossApronStartZ != -1.0f && railDistance_ >= bossApronStartZ - 40.0f &&
            !(sceneryPreviewSeen & 8u)) { previewBit = 8u; }
        else if (bossSpawned_ && bossIntroTimer_ <= 0 && !(sceneryPreviewSeen & 16u)) { previewBit = 16u; }
        else if (bossApronStartZ != -1.0f && railDistance_ >= bossApronStartZ + 245.0f &&
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
            if (railDistance_ >= stops[index] && !(sceneryPreviewSeen & bit)) {
                previewBit = bit;
                break;
            }
        }
    }
    if (sceneryPreview && frame > 30 && previewBit != 0 && (sceneryPreviewSeen & previewBit) == 0) {
        sceneryPreviewSeen |= previewBit;
        log("SCENERY_PREVIEW rail=" + std::to_string(railDistance_));
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
        log("FAIL player_dead progress=" + std::to_string(stageProgress_));
        throw std::runtime_error("Playthrough pilot was defeated; no invulnerability or forced clear used");
    }
    if (frame == 0) {
        CheckEnemyFireControl();
        CheckSniperPosture(object3dCommon_.get(), enemyShooterModel_);
        log("SNIPER_POSTURE_OK brace_still=1 rail_relative=1 recoil_on_shot=1 recovery_open=1 death_cancels=1");
        log("ENEMY_FIRE_CONTROL_OK rail_intercept=1 fixed_aim=1 windup_burst_recovery=1 slow_steps=0.25,0.5,1");
        if (!sound_ || sound_->GetVoiceCount() != 28) {
            throw std::runtime_error("Combat audio bank failed to load all 28 voices");
        }
        log("START normal_damage=1 normal_collisions=1 audio_voices=28");
    }
    ++frame;
    // 実際のESC経路で停止し、各フレームの進行値と全アクター位置が不変であることを検証する。
    const auto snapshot = [&]() {
        std::vector<float> values{ gameplayElapsedSeconds_, stageProgress_, railDistance_, cameraTimer_,
            flightReleaseKick_, previousFlightTimeScale_, cameraFovY_, sfxAccentUntil_,
            flightCameraMotion_.fovVelocity, flightCameraMotion_.acceleration,
            flightCameraMotion_.previousSpeed, flightCameraMotion_.blurStrength,
            static_cast<float>(chargeTimer_), static_cast<float>(feverTimer_), static_cast<float>(feverGauge_), phantomCooldown_,
            static_cast<float>(score_), static_cast<float>(player_->GetHp()),
            static_cast<float>(enemies_.size()), static_cast<float>(playerBullets_.size()),
            static_cast<float>(enemyBullets_.size()), static_cast<float>(playerShotsFired_) };
        const auto position = [&](const Math::Vector3& p) { values.insert(values.end(), { p.x, p.y, p.z }); };
        position(player_->GetTranslate());
        position(cameraTranslate_);
        position(cameraRotate_);
        position(flightCameraMotion_.translationVelocity);
        position(flightCameraMotion_.rotationVelocity);
        for (const auto& enemy : enemies_) { position(enemy->GetTranslate()); }
        for (const auto& bullet : playerBullets_) { position(bullet->GetTranslate()); }
        for (const auto& bullet : enemyBullets_) { position(bullet->GetTranslate()); }
        return values;
    };
    if (frame == 721) {
        if (!isPaused_ || showControlsHelp_) { throw std::runtime_error("ESC did not open pause menu"); }
        pausedState = snapshot();
    }
    if (frame > 721 && frame <= 840 && (!isPaused_ || snapshot() != pausedState)) {
        throw std::runtime_error("Gameplay changed while pause menu was open");
    }
    if (frame == 841) {
        if (isPaused_) { throw std::runtime_error("ESC did not resume gameplay"); }
        if (delayedEscapeEvents != 2) { throw std::runtime_error("Delayed UI input fixture did not execute"); }
        log("PAUSE_OK frames=120 time_rail_actors_bullets_hp_charge_fever_skill_frozen=1 resume=1");
        log("MENU_INPUT_OK escape_hold=31_frames delayed_ui_duplicates_ignored=2 wasd_arrows_hold=1");
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
        if (pauseSelectedItem_ != expected) { throw std::runtime_error("Pause selection repeated during key hold or delayed UI event"); }
    }
    if (frame > 841 && frame <= 861 && isPaused_) { throw std::runtime_error("Held ESC reopened pause after resume"); }
    const bool empty = !bossSpawned_ && enemies_.empty() && defeatedEnemyCount_ > 0;
    if (empty && std::any_of(stageEnemyEventTriggered_.begin(), stageEnemyEventTriggered_.end(), [](bool v) { return !v; })) {
        longestEmptyGap = (std::max)(longestEmptyGap, stageEmptyFrames_);
        if (stageEmptyFrames_ > 62.0f) {
            throw std::runtime_error("Empty encounter gap exceeded one second: frames=" +
                std::to_string(stageEmptyFrames_) + " stage=" + std::to_string(stageProgress_) +
                " speed=" + std::to_string(stageTimelineSpeed_) + " slow=" +
                std::to_string(GetCinematicWorldTimeScale()) + " breather=" +
                std::to_string(stageEncounterBreatherTimer_));
        }
    }
    if (wasEmpty && !empty) { ++encounterGapCount; }
    wasEmpty = empty;
    if (frame == 61 && (!showControlsHelp_ || !isPaused_)) {
        throw std::runtime_error("H did not open controls help");
    }
    if (frame == 301 && (showControlsHelp_ || isPaused_)) {
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
        if (resultTransitionTimer_ <= 0) {
            constexpr BYTE directKeys[] = { DIK_RIGHT, DIK_LEFT, DIK_DOWN, DIK_UP, DIK_D, DIK_A, DIK_S, DIK_W };
            constexpr ImGuiKey uiKeys[] = { ImGuiKey_RightArrow, ImGuiKey_LeftArrow, ImGuiKey_DownArrow, ImGuiKey_UpArrow,
                ImGuiKey_D, ImGuiKey_A, ImGuiKey_S, ImGuiKey_W };
            const int action = resultFrames / 30;
            const int offset = resultFrames % 30;
            if (action < 8) {
                if (offset < 10) { keys[directKeys[action]] = 0x80; }
                if (offset == 3) { ImGui::GetIO().AddKeyEvent(uiKeys[action], true); }
                if (offset == 5) { ImGui::GetIO().AddKeyEvent(uiKeys[action], false); }
                if (offset > 0 && resultSelectedItem_ != (action + 1) % 2) {
                    throw std::runtime_error("Result selection repeated during key hold or delayed UI event");
                }
            }
            if (resultFrames == 240) { log("RESULT_KEYS_OK all_8_keys_hold=1 delayed_ui_duplicates_ignored=1"); }
        }
        if (!bossDefeated_ || !std::all_of(stageEnemyEventTriggered_.begin(), stageEnemyEventTriggered_.end(), [](bool v) { return v; })) {
            throw std::runtime_error("Clear without full schedule and boss defeat");
        }
        if (resultTransitionTimer_ <= 0 && ++resultFrames >= 260 && phase == 0) {
            if (bossShotsFired_ <= 0) {
                throw std::runtime_error("Boss was defeated without firing any attack");
            }
            if (feverActivationCount_ <= 0) {
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
            log("CLEAR hp=" + std::to_string(player_->GetHp()) +
                " defeated=" + std::to_string(defeatedEnemyCount_) +
                " escaped=" + std::to_string(escapedEnemyCount_) +
                " shots=" + std::to_string(playerShotsFired_) +
                " hits=" + std::to_string(playerHitCount_) +
                " fever=" + std::to_string(feverActivationCount_) +
                " phantom=" + std::to_string(phantomActivationCount_) +
                " phantom_kills=" + std::to_string(phantomDefeatCount_) +
                " boss_attacks_started=" + std::to_string(bossAttackSequence_) +
                " boss_shots=" + std::to_string(bossShotsFired_) +
                " boss_phase=" + std::to_string(bossPhase_) +
                " pool_miss_p=" + std::to_string(playerBulletPoolMisses_) +
                " pool_miss_e=" + std::to_string(enemyBulletPoolMisses_) +
                " pool_miss_fx=" + std::to_string(hitEffectObjectPoolMisses_) +
                " max_bullets_p=" + std::to_string(maxActivePlayerBullets_) +
                " max_bullets_e=" + std::to_string(maxActiveEnemyBullets_) +
                " sounds=" + std::to_string(sound_->GetPlayCount()) +
                " voices=" + std::to_string(sound_->GetVoiceCount()));
            keys[DIK_RETURN] = 0x80; // 選択中の「再挑戦」をEnterで決定する。
            phase = 1;
        }
        input_->SetTestFrame(keys, mouse);
        return false;
    }
    if (frame % 600 == 0) {
        log("PROGRESS stage=" + std::to_string(stageProgress_) +
            " rail=" + std::to_string(railDistance_) +
            " hp=" + std::to_string(player_->GetHp()) +
            " defeated=" + std::to_string(defeatedEnemyCount_));
    }
    const Enemy* target = nullptr;
    for (const auto& enemy : enemies_) {
        Math::Vector2 screen{};
        if (enemy && !enemy->IsDead() && enemy->IsTargetable() &&
            TryProjectToScreen(enemy->GetAimPosition(), screen)) {
            target = enemy.get();
            const ImVec2 origin = ImGui::GetMainViewport()->Pos;
            mouse = { screen.x - origin.x, screen.y - origin.y };
            break;
        }
    }
    if (target) {
        // 硬い敵へはチャージ、それ以外には連射。性能・弾・敵HPは通常値のまま。
        if (target->GetHp() < 7 || chargeTimer_ >= chargeShotThreshold_ || feverTimer_ > 0) {
            keys[DIK_SPACE] = 0x80;
        }
    }
    const auto position = player_->GetTranslate();
    const bool edgeScan = sceneryPreviewMode == 7 || sceneryPreviewMode == 8;
    const float targetX = (edgeScan ? 8.5f : 4.2f) * std::sin(static_cast<float>(frame) * 0.011f);
    const float targetY = 0.7f + (edgeScan ? 2.5f : 1.0f) * std::sin(static_cast<float>(frame) * 0.017f);
    if (std::abs(targetX - position.x) > 0.3f) {
        keys[targetX > position.x ? DIK_D : DIK_A] = 0x80;
    }
    if (std::abs(targetY - position.y) > 0.3f) {
        keys[targetY > position.y ? DIK_W : DIK_S] = 0x80;
    }
    for (const auto& bullet : enemyBullets_) {
        const auto bulletPosition = bullet->GetTranslate();
        const Math::Vector3 delta{ bulletPosition.x - position.x,
            bulletPosition.y - position.y, bulletPosition.z - position.z };
        if (!bullet->IsDead() && delta.z > 0.0f && delta.z < 12.0f &&
            std::abs(delta.x) < 3.0f && std::abs(delta.y) < 2.5f && frame % 2 == 0) {
            keys[DIK_LSHIFT] = 0x80;
            break;
        }
    }
    if (phantomReady_ && target && frame % 2 == 0) {
        keys[DIK_Q] = 0x80;
    }
    if (sceneryPreviewMode == 3 && frame >= 320 && frame <= 451) {
        keys[DIK_W] = keys[DIK_A] = keys[DIK_S] = keys[DIK_D] = keys[DIK_LSHIFT] = 0;
        if (frame <= 350) { keys[DIK_D] = keys[DIK_W] = 0x80; }
        else if (frame <= 400) { keys[DIK_A] = 0x80; }
        else if (frame <= 450) { keys[DIK_S] = 0x80; }
    }
    input_->SetTestFrame(keys, mouse);
    return false;
}

bool GameRuntime::RunPhantomProbe(const std::string& logPath, bool preview)
{
    static int frame = 0;
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
    if (preview && (frame == 0 || frame == 31 || frame == 38 || frame == 46 ||
        frame == 172 || frame == 184 || frame == 205 || frame == 230 || frame == 359 || frame == 374)) {
        phantomPreviewPaused_ = true;
        ImGui::SetNextWindowPos({ 20.0f, 165.0f }, ImGuiCond_Always);
        ImGui::Begin("Phantom visual fixture", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted("TEST ONLY: animation frozen for visual inspection");
        ImGui::Text("Sample: %d", frame);
        const bool next = ImGui::Button("NEXT / resume test", { 220.0f, 36.0f });
        ImGui::End();
        if (!next) {
            // 表示確認中も長押し試験の入力を保つ。ここで離すと再開時に
            // 新しいSHIFT押下が生まれ、通常プレイにはない再回避を起こしてしまう。
            if (frame >= 31 && frame <= 65) { keys[DIK_LSHIFT] = 0x80; }
            input_->SetTestFrame(keys, mouse);
            return false;
        }
        phantomPreviewPaused_ = false;
    }
    if (retry && !isGameClear_) {
        require(phantomReady_ && !IsPhantomRaidActive() && phantomActivationCount_ == 0 &&
            phantomDefeatCount_ == 0 && phantomCooldown_ == 0.0f, "retry skill state reset");
        require(sound_ && sound_->GetVoiceCount() == 28, "retry audio pool");
        log("PASS retry_reset voices=28");
        input_->SetTestFrame(keys, mouse);
        return true;
    }
    ++frame;
    require(!isGameOver_, "fixture player died");
    if (frame == 1) {
        CheckEnemyFireControl();
        log("ENEMY_FIRE_CONTROL_OK rail_intercept=1 fixed_aim=1 windup_burst_recovery=1");
        CheckDodgeControls(object3dCommon_.get(), playerModel_);
        log("DODGE_CONTROLS_OK distance=4.4 duration=16 buffer=5 slow_independent=1 steps=0.5,1,2 edge_ok=1");
        require(phantomReady_ && !IsPhantomRaidActive() && phantomCooldown_ == 0.0f,
            "startup must be ready without dodge, but must not auto activate");
        require(sound_ && sound_->GetVoiceCount() == 28, "all sounds loaded");
        if (!SoundManager::kSoundEffectsEnabled) {
            for (const char* key : { "shot", "charge", "skill_start", "hit", "destroy", "damage",
                "dodge", "fever", "clear", "fail", "skill_ready", "slash", "slash_finish" }) {
                require(!sound_->Play(key), "disabled sound effect was submitted");
            }
            require(sound_->GetPlayCount() == 0, "disabled sound effects changed playback count");
            require(sound_->PlayLoop("music_stage") && sound_->GetLoopCount() == 1,
                "sound effect disable also stopped BGM");
            log("SFX_DISABLED_OK all_one_shots_blocked=1 bgm_loop=1");
        }
        log(std::string("AUDIO_BANK mode=") + (phantomLocalAudio_ ? "local_cinematic" : "original_cc0") +
            " voices=28");
        std::fill(stageEnemyEventTriggered_.begin(), stageEnemyEventTriggered_.end(), true);
        log("BEGIN controlled_enemy_fixture=1 normal_damage_code=1");
        keys[DIK_Q] = 0x80;
    }
    if (frame == 2) {
        require(phantomActivationCount_ == 0 && phantomReady_ && phantomCooldown_ == 0.0f,
            "Q without target consumed startup ready");
        log("STARTUP_READY_WITHOUT_DODGE_OK empty_target_no_consumption=1");
    }
    if (frame == 30) {
        phantomReady_ = false;
        phantomCooldown_ = 300.0f; // 回避でスキルが再充填されないことを独立検証。
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
        require(justDodgeCount_ > 0 && !phantomReady_ && phantomCooldown_ > 290.0f,
            "just dodge unexpectedly recharged the independent skill");
        for (auto& bullet : enemyBullets_) { bullet->Kill(); }
        phantomCooldown_ = 0.0f;
        GrantPhantomRaid(); // 後続の連撃試験を初期の使用可能状態へ戻す。
        log("DODGE_INDEPENDENT_OK");
    }
    if (frame == 40) { keys[DIK_Q] = 0x80; }
    if (frame == 41) {
        require(phantomReady_ && !IsPhantomRaidActive(), "no target consumed ready");
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
            SpawnStageEnemy(x, 1.0f + static_cast<float>(index % 2) * 0.7f, 32.0f,
                Enemy::Behavior::Formation, Enemy::EntryStyle::Direct, 3, 1.0f);
        }
    };
    if (frame == 70) {
        // 本編「Sniper wing」と同じ位置・HP・編隊動作で、スキル対象の選択を検証する。
        SpawnStageEnemy(-4.6f, -0.6f, 44.0f, Enemy::Behavior::Formation,
            Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemy(-2.0f, 0.5f, 44.0f, Enemy::Behavior::Formation,
            Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemy(0.6f, -0.6f, 44.0f, Enemy::Behavior::Formation,
            Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemy(5.3f, 2.0f, 48.0f, Enemy::Behavior::Sniper,
            Enemy::EntryStyle::PopShooter, 6, 1.18f);
    }
    if (frame == 149) {
        // 狙撃機を指した場合も実際の選択処理を通す。時間は進めず、ダメージ前に試験状態を戻す。
        Math::Vector2 screen{};
        require(enemies_.size() == 4 && enemies_.back()->IsSniper() &&
            TryProjectToScreen(enemies_.back()->GetAimPosition(), screen), "sniper selection fixture missing");
        const auto savedReticle = reticleScreen_;
        reticleScreen_ = screen;
        require(TryActivatePhantomRaid() && phantomTargetCount_ == 3 &&
            phantomTargets_[0].enemy == enemies_.back().get(), "sniper could not be prioritized by aiming");
        ResetPhantomRaid();
        reticleScreen_ = savedReticle;
        log("SNIPER_CHOICE_OK aimed_sniper_selected_first=1");
    }
    if (frame == 150) {
        Math::Vector2 screen{};
        require(enemies_.size() == 4 && TryProjectToScreen((*std::next(enemies_.begin()))->GetAimPosition(), screen),
            "formation/sniper fixture missing");
        const ImVec2 origin = ImGui::GetMainViewport()->Pos;
        mouse = { screen.x - origin.x, screen.y - origin.y };
        // UpdateLockOnTargetはUpdateの末尾なので、照準を置く1フレームと発動を分ける。
        activationMouse = mouse;
    }
    if (frame == 151) {
        audioBefore = phantomAudioPlays_;
        // Input::Updateが毎フレームOSカーソルを取得するため、試験の照準を明示的に維持する。
        mouse = activationMouse;
        keys[DIK_Q] = 0x80;
        log("NORMAL_ACTIVATE");
    }
    if (frame == 152) {
        require(IsPhantomRaidActive() && phantomTargetCount_ == 3, "normal target selection");
        for (int index = 0; index < phantomTargetCount_; ++index) {
            require(phantomTargets_[index].enemy && !phantomTargets_[index].enemy->IsSniper(),
                "aiming at formation failed to prioritize its three targets");
        }
        require(phantomCooldown_ == kPhantomCooldownFrames, "activation must begin 8-second cooldown");
        const float before = phantomCooldown_;
        RecoverPhantomRaidOnHit(true, true);
        require(phantomCooldown_ == before, "active skill allowed cooldown recovery");
        log("COOLDOWN_8_SECONDS_OK no_self_recharge=1");
    }
    if (frame == 220) {
        require(phantomAudioPlays_[0] - audioBefore[0] == (SoundManager::kSoundEffectsEnabled ? 3u : 0u) &&
            phantomAudioPlays_[1] - audioBefore[1] == (SoundManager::kSoundEffectsEnabled ? 1u : 0u),
            "normal skill audio does not match sound effect enable state");
        log(SoundManager::kSoundEffectsEnabled ? "AUDIO_NORMAL_OK slashes=3 finish=1" :
            "AUDIO_NORMAL_OK muted=1 slashes=0 finish=0");
        require(!IsPhantomRaidActive() && phantomDefeatCount_ == 3 && defeatedEnemyCount_ == 3, "normal triple finish");
        require(enemies_.size() == 1 && enemies_.front()->IsSniper() && !enemies_.front()->IsDead(),
            "formation clear should leave the independently positioned sniper alive");
        log("FORMATION_CHOICE_OK triple_clear=1 sniper_remains=1");
        enemies_.front()->Kill(); // 次の独立ケースへ持ち越さない。報酬には加算しない。
        require(!phantomReady_ && phantomCooldown_ > 0.0f, "normal consumption or cooldown");
        GrantPhantomRaid();
        require(!phantomReady_, "cooldown allowed immediate recharge");
        const float before = phantomCooldown_;
        RecoverPhantomRaidOnHit(false, false);
        require(std::abs(phantomCooldown_ - (before - 12.0f)) < 0.001f, "normal hit cooldown reduction");
        RecoverPhantomRaidOnHit(true, true);
        require(std::abs(phantomCooldown_ - (before - 66.0f)) < 0.001f, "charged kill cooldown reduction");
        phantomCooldown_ = 6.0f;
        RecoverPhantomRaidOnHit(false, false);
        require(phantomReady_ && phantomCooldown_ == 0.0f, "hit did not complete recovery");
        const auto sounds = sound_->GetPlayCount();
        RecoverPhantomRaidOnHit(true, true);
        require(sound_->GetPlayCount() == sounds, "ready sound repeated on extra hit");
        log("NORMAL_3_KILLS_OK reward_once cooldown_ok");
        log("HIT_RECOVERY_OK normal=12 charged_kill=54 clamped=0 notification_once=1");
    }
    if (frame == 221) {
        phantomReady_ = false;
        phantomCooldown_ = 3.0f; // 最後の短区間を無入力で進め、満了時の自動回復を検証。
    }
    if (frame == 230) {
        require(phantomReady_ && phantomCooldown_ == 0.0f, "time expiry did not auto recharge skill");
        log("TIME_ONLY_RECOVERY_OK no_shooting_or_dodge=1");
    }
    if (frame == 232) {
        feverTimer_ = 0;
        feverGauge_ = 99;
        feverCountBeforeAutoTest = feverActivationCount_;
    }
    if (frame == 233) {
        require(feverTimer_ == 0 && feverActivationCount_ == feverCountBeforeAutoTest,
            "fever activated below full gauge");
        AddFeverGauge(1); // 本番のゲージ加算経路で満タンにする。Eキーは送らない。
    }
    if (frame == 234) {
        require(feverTimer_ > 0 && feverGauge_ == 0 &&
            feverActivationCount_ == feverCountBeforeAutoTest + 1,
            "full gauge did not auto activate fever exactly once");
        AddFeverGauge(100);
    }
    if (frame == 235) {
        require(feverGauge_ == 0 && feverActivationCount_ == feverCountBeforeAutoTest + 1,
            "active fever accumulated gauge or activated twice");
        log("FEVER_AUTO_OK threshold=100 gauge_consumed=1 no_key=1 no_double_activation=1");
    }
    if (frame == 240) {
        phantomCooldown_ = 0.0f; // 次の独立ケース用の試験リセット。
        GrantPhantomRaid();
        spawn(5);
    }
    if (frame == 330) {
        audioBefore = phantomAudioPlays_;
        keys[DIK_Q] = 0x80;
        log("OVERDRIVE_ACTIVATE");
    }
    if (frame == 331) { require(phantomEmpowered_ && phantomTargetCount_ == 5, "fever target selection"); }
    if (frame == 410) {
        require(phantomAudioPlays_[0] - audioBefore[0] == (SoundManager::kSoundEffectsEnabled ? 5u : 0u) &&
            phantomAudioPlays_[1] - audioBefore[1] == (SoundManager::kSoundEffectsEnabled ? 1u : 0u),
            "fever skill audio does not match sound effect enable state");
        log(SoundManager::kSoundEffectsEnabled ? "AUDIO_FEVER_OK slashes=5 finish=1" :
            "AUDIO_FEVER_OK muted=1 slashes=0 finish=0");
        require(!IsPhantomRaidActive() && phantomDefeatCount_ == 8 && defeatedEnemyCount_ == 8, "fever five finish");
        log("OVERDRIVE_5_KILLS_OK");
    }
    if (frame == 430) {
        feverTimer_ = 1; // 終了境界も通常のUpdateFever経路で検証する。
        phantomCooldown_ = 0.0f;
        GrantPhantomRaid();
        spawn(3);
    }
    if (frame == 432) {
        require(feverTimer_ == 0 && feverGauge_ == 0 &&
            feverActivationCount_ == feverCountBeforeAutoTest + 1,
            "fever reactivated after expiry without refilling");
        log("FEVER_AUTO_EXPIRY_OK no_retrigger=1");
    }
    if (frame == 520) { keys[DIK_Q] = 0x80; log("TARGET_REMOVAL_ACTIVATE"); }
    if (frame == 526) {
        for (auto& enemy : enemies_) { enemy->Kill(); } // 演出途中の外部消滅を模擬。
    }
    if (frame == 590) {
        require(!IsPhantomRaidActive() && phantomDefeatCount_ == 8, "removed targets double rewarded");
        log("REMOVED_TARGETS_SAFE_OK");
    }
    if (frame == 620) {
        DebugJumpToStagePhase(3);
        log("BOSS_FIXTURE_BEGIN");
    }
    if (frame == 820) {
        Enemy* boss = nullptr;
        for (auto& enemy : enemies_) { if (enemy->IsBoss()) { boss = enemy.get(); break; } }
        require(boss != nullptr, "boss fixture missing");
        boss->Damage((std::max)(0, boss->GetHp() - 7)); // クリア経路を調べる低HP試験配置。
        GrantPhantomRaid();
        ActivateFever();
    }
    if (frame == 850) { keys[DIK_Q] = 0x80; log("BOSS_FINISH_ACTIVATE"); }
    if (frame > 920 && isGameClear_ && resultTransitionTimer_ <= 0 && !retry) {
        require(bossDefeated_ && phantomDefeatCount_ == 1 &&
            defeatedEnemyCount_ == GetTotalEnemyTargetCount() + 1, "boss clear reward path");
        log("BOSS_SKILL_CLEAR_OK");
        keys[DIK_R] = 0x80;
        retry = true;
    }
    if (frame == 1100) { require(retry, "boss clear not reached"); }
    input_->SetTestFrame(keys, mouse);
    return false;
}
bool GameRuntime::RunBossProbe(const std::string& logPath, bool preview)
{
    static int frame = 0;
    const auto log = [&](const std::string& message) {
        std::ofstream file(logPath, std::ios::app);
        file << "BOSS_TEST " << message << '\n';
    };
    const auto require = [&](bool ok, const char* message) {
        if (!ok) { log(std::string("FAIL ") + message); throw std::runtime_error(message); }
    };
    if (preview && (frame == 30 || frame == 31 || frame == 138 || frame == 172 || frame == 366 || frame == 606 || frame == 773 ||
        frame == 880 || frame == 901 || frame == 931)) {
        phantomPreviewPaused_ = true;
        // 同じHUDを背後に残したポーズ・操作方法も、通常起動を変えずに実画面で確認する。
        isPaused_ = frame == 30 || frame == 31;
        showControlsHelp_ = frame == 31;
        ImGui::SetNextWindowPos({ 20.0f, 150.0f }, ImGuiCond_Always);
        ImGui::Begin("Boss visual fixture", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted("TEST ONLY: pause / controls / boss / results");
        ImGui::Text("Frame %d   Pattern %d   Counter %d", frame, bossAttackPattern_, bossCounterTimer_);
        // 全画面の操作説明ウィンドウがマウスを覆っていても、映像試験を進められる。
        const bool next = ImGui::Button("NEXT / F8", { 220.0f, 36.0f }) || ImGui::IsKeyPressed(ImGuiKey_F8, false);
        ImGui::End();
        if (!next) { input_->SetTestFrame({}, { 640, 320 }); return false; }
        phantomPreviewPaused_ = false;
        isPaused_ = showControlsHelp_ = false;
    }
    ++frame;
    if (frame == 1) {
        require(sound_ && sound_->GetVoiceCount() == 28, "BGM/SFX bank incomplete");
        for (const char* key : { "music_stage", "music_boss", "music_fever" }) {
            require(sound_->PlayLoop(key) && sound_->PlayLoop(key) && sound_->GetLoopCount() == 1,
                "loop not queued or duplicate loop created");
            sound_->Stop(key);
            require(sound_->GetLoopCount() == 0, "loop did not stop");
        }
        require(sound_->GetVoiceCount() == 28 && sound_->GetPlayCount() == 0, "loop changed SFX counter/pool");
        log("LOOPS_OK voices=28 duplicate_safe=1 stop_safe=1 sfx_count_unchanged=1");
        const auto reset = [&]() {
            isGameClear_ = false;
            isGameOver_ = false;
            resultSoundPlayed_ = false;
            score_ = 0;
            feverTimer_ = 0;
            feverGauge_ = 0;
            DebugJumpToStagePhase(3);
            for (auto& effect : hitEffects_) { RecycleHitEffectVisuals(effect); }
            hitEffects_.clear();
            SpawnBossEnemy();
            return enemies_.back().get();
        };
        for (int phase : { 1, 2 }) {
            for (int pattern = 0; pattern < 3; ++pattern) {
                auto* boss = reset();
                require(!boss->IsTargetable(), "boss targetable before arrival");
                for (int step = 0; step < 95; ++step) { boss->Update(railDistance_); }
                require(!boss->IsTargetable(), "boss became targetable before entry completed");
                for (int step = 95; step < 300; ++step) { boss->Update(railDistance_); }
                require(boss->CanShoot(), "boss fixture not ready");
                bossIntroTimer_ = 0;
                bossPhase_ = phase;
                bossAttackSequence_ = pattern;
                bossAttackCooldown_ = 0;
                UpdateBossActions();
                const int windup = bossAttackStepTimer_;
                require(windup >= 38 && enemyBullets_.empty(), "boss fired without a readable windup");
                while (bossAttackStepTimer_ > 14) { UpdateBossActions(); }
                bossAimPoint_.x = 3.75f; // 照準固定後に現在の自機座標で上書きされないことを確認。
                while (bossAttackStepTimer_ > 0) { UpdateBossActions(); }
                require(enemyBullets_.empty() && bossAimPoint_.x == 3.75f, "boss tracked after aim lock");
                UpdateBossActions();
                int guard = 0;
                while (bossAttackStep_ >= 0 && ++guard < 150) { UpdateBossActions(); }
                const size_t expected = pattern == 0 ? (phase == 2 ? 7u : 5u) :
                    pattern == 1 ? (phase == 2 ? 9u : 7u) : (phase == 2 ? 3u : 1u);
                require(bossAttackStep_ == -1 && enemyBullets_.size() == expected && bossAimPoint_.x == 3.75f,
                    "boss pattern count/fixed sweep aim incorrect");
                if (pattern == 2) {
                    const auto p = enemyBullets_.front()->GetTranslate();
                    const auto v = enemyBullets_.front()->GetVelocity();
                    const float t = (p.z - player_->GetTranslate().z - 0.18f) / (railSpeed_ - v.z);
                    require(t > 0 && std::abs(p.x + v.x * t - 3.75f) < 0.01f, "charge ignored fixed aim");
                }
                require(bossCounterTimer_ == (phase == 2 ? 108 : 120) &&
                    bossAttackCooldown_ > bossCounterTimer_, "counter opening too short or overlaps next attack");
                boss->SetBossRecoveryRate(0.5f);
                boss->Update(railDistance_);
                const float x = boss->GetTranslate().x;
                for (int step = 0; step < 40; ++step) { boss->Update(railDistance_); }
                require(std::abs(boss->GetTranslate().x - x) < 0.001f, "boss drifted during recovery");
                const size_t shotCount = enemyBullets_.size();
                for (int step = 0; step < bossCounterDuration_; ++step) { UpdateBossActions(); }
                require(bossCounterTimer_ == 0 && enemyBullets_.size() == shotCount,
                    "boss fired inside counter opening");
                log("PATTERN_OK phase=" + std::to_string(phase) + " pattern=" + std::to_string(pattern) +
                    " windup=" + std::to_string(windup) + " shots=" + std::to_string(shotCount));
            }
        }
        auto* boss = reset();
        for (int step = 0; step < 300; ++step) { boss->Update(railDistance_); }
        boss->Damage(boss->GetMaxHp() / 2);
        FireEnemyBullet(boss->GetAimPosition(), EnemyBulletStyle::BossCharge);
        UpdateBossActions();
        require(bossPhase_ == 2 && bossPhaseTransitionTimer_ > 0 && enemyBullets_.empty(),
            "phase transition retained dangerous bullets");
        log("PHASE_OK hp_half_transition=1 bullets_recycled=1");
        boss->Damage(999);
        OnEnemyDestroyed(*boss, true, false);
        require(isGameClear_ && bossDefeated_ && enemyBullets_.empty() && !resultSoundPlayed_,
            "boss defeat cleanup failed");
        for (int step = 0; step < 45; ++step) { UpdateResultAndSceneObjects(); }
        require(resultSoundPlayed_ && bossDefeatFlashTimer_ == 0, "defeat sequence/result cue not completed");
        require(enemyBulletPoolMisses_ == 0 && hitEffectObjectPoolMisses_ == 0, "boss exhausted fixed pools");
        log("DEFEAT_OK delayed_bursts=3 result_after_bursts=1 pool_misses=0");
        reset();
        cameraShakeTimer_ = 0;
        musicTrack_ = -1;
        musicLevels_.fill(0.0f);
        log("LIVE_VISUAL_BEGIN controlled_fixture=1");
    }
    if (frame == 500) { feverTimer_ = 90; }
    if (frame == 530) {
        require(musicTrack_ == 2 && sound_->GetLoopCount() == 1, "fever crossfade did not settle");
        log("FEVER_MUSIC_OK one_loop=1");
    }
    if (frame == 640) {
        require(musicTrack_ == 1 && sound_->GetLoopCount() == 1, "boss music did not resume");
        log("BOSS_MUSIC_OK one_loop=1");
    }
    if (frame == 750) {
        auto* boss = enemies_.front().get();
        boss->Damage(999); // 撃破映像の固定サンプル。通常通し試験ではこの操作を使わない。
        OnEnemyDestroyed(*boss, true, false);
    }
    if (frame == 840) {
        require(isGameClear_ && sound_->GetLoopCount() == 0 && resultSoundPlayed_, "music remains after clear");
        require(hitEffectObjectPoolMisses_ == 0, "live defeat effect pool exhausted");
        log("PASS clear_music_silent=1 voices=28 pool_misses=0");
        input_->SetTestFrame({}, { 640, 320 });
        if (!preview) { return true; }
    }
    // リザルトの分岐は映像検証時だけ切り替える。通常プレイ・通常の通し試験は実際の勝敗を使う。
    if (preview && frame == 900) { isGameClear_ = false; isGameOver_ = true; }
    if (preview && frame == 930) {
        playMode_ = PlayMode::Tutorial;
        isGameClear_ = true;
        isGameOver_ = bossSpawned_ = false;
        defeatedEnemyCount_ = GetTotalEnemyTargetCount();
    }
    if (preview && frame == 960) { log("RESULT_PREVIEW_COMPLETE"); return true; }
    input_->SetTestFrame({}, { 640, 320 });
    return false;
}

bool GameRuntime::RunChargeShotProbe(const std::string& logPath, bool preview)
{
    static int frame = 0;
    static int impactFrame = -1;
    const auto log = [&](const std::string& message) {
        std::ofstream file(logPath, std::ios::app);
        file << "CHARGE_TEST " << message << '\n';
    };
    const auto require = [&](bool ok, const char* message) {
        if (!ok) { log(std::string("FAIL ") + message); throw std::runtime_error(message); }
    };
    std::array<BYTE, 256> keys{};
    Math::Vector2 mouse = input_->GetMousePosition();
    if (frame > 102 && defeatedEnemyCount_ == 3 && impactFrame < 0) { impactFrame = frame; }
    if (preview && (frame == 100 || frame == 105 || frame == 161 || frame == 166 || frame == 174 || (impactFrame >= 0 &&
        (frame == impactFrame || frame == impactFrame + 5 || frame == impactFrame + 12)))) {
        phantomPreviewPaused_ = true;
        ImGui::SetNextWindowPos({ 20.0f, 165.0f }, ImGuiCond_Always);
        ImGui::Begin("Charge visual fixture", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted(frame == 100 ? "TEST ONLY: six enemy projectile styles" :
            frame >= 160 ? "TEST ONLY: fever burst / high chain" :
            "TEST ONLY: charge shot / three small ships + sniper");
        ImGui::Text("Frame: %d   Defeated: %d", frame, defeatedEnemyCount_);
        const bool next = ImGui::Button("NEXT / F8", { 220.0f, 36.0f }) || ImGui::IsKeyPressed(ImGuiKey_F8, false);
        ImGui::End();
        if (!next) { input_->SetTestFrame(keys, mouse); return false; }
        phantomPreviewPaused_ = false;
    }
    ++frame;
    if (frame == 1) {
        CheckProjectileVisualProjection();
        log("PROJECTILE_VISUAL_OK body_follows_velocity=1 projected_endpoints_match=1 head_on_round=1 near_clip_bounded=1");
        const auto reset = [&]() {
            DebugJumpToStagePhase(0);
            std::fill(stageEnemyEventTriggered_.begin(), stageEnemyEventTriggered_.end(), true);
            for (auto& effect : hitEffects_) { RecycleHitEffectVisuals(effect); }
            hitEffects_.clear();
            score_ = 0;
            feverGauge_ = 0;
            feverTimer_ = 0;
            playerImpactSlowTimer_ = 0;
            cameraShakeTimer_ = 0;
        };
        // 編隊同時撃破＋着弾＋連射を重ねても、固定枠だけで全演出が出て返却されるか。
        for (const bool fever : { false, true }) {
            reset();
            defeatChainCount_ = 12;
            const size_t capacity = hitEffectObjectPool_.size();
            for (int index = 0; index < 7; ++index) {
                const Math::Vector3 p{ static_cast<float>(index - 3), 1.0f, railDistance_ + 32 };
                if (fever) { AddFeverEnemyHitEffect(p, 1.08f); AddFeverEnemyImpactEffect(p, 1.38f); }
                else { AddEnemyHitEffect(p, 1.32f); AddEnemyImpactEffect(p, 1.42f); }
            }
            for (int index = 0; index < 12; ++index) { AddMuzzleFlashEffect(player_->GetTranslate(), true); }
            require(hitEffectObjectPoolMisses_ == 0, "Simultaneous combat burst exhausted fixed effect pool");
            for (int step = 0; step < 120; ++step) { UpdateHitEffects(); }
            require(hitEffects_.empty() && hitEffectObjectPool_.size() == capacity,
                "Combat burst retained or returned an effect object twice");
        }
        reset();
        AddEnemyHitEffect(player_->GetTranslate(), 1.0f);
        const float baseStrength = hitEffects_.back().strength;
        const size_t baseVisuals = hitEffects_.back().visualCount;
        reset();
        defeatChainCount_ = 12;
        AddEnemyHitEffect(player_->GetTranslate(), 1.0f);
        require(hitEffects_.back().strength > baseStrength && hitEffects_.back().visualCount > baseVisuals,
            "Defeat chain failed to strengthen combat visuals");
        reset();
        log("COMBAT_BURST_OK simultaneous_kills=7 overlapping_impacts=7 muzzles=12 pools=0 fully_recycled=1 chain_escalates=1");
        const auto spawn = [&](float x, float y, int hp = 3, Enemy::Behavior behavior = Enemy::Behavior::Formation) {
            SpawnStageEnemy(x, y, 44.0f, behavior, Enemy::EntryStyle::TightFormation, hp, 1.0f);
            auto* enemy = enemies_.back().get();
            for (int step = 0; step < 120; ++step) { enemy->Update(railDistance_); }
            require(enemy->IsTargetable(), "fixture not targetable");
            return enemy;
        };
        const auto hit = [&](Enemy& target, bool charged, bool fever = false) {
            auto bullet = AcquireBullet(playerBulletPool_);
            require(bullet != nullptr, "fixture bullet pool exhausted");
            bullet->Initialize(object3dCommon_.get(), bulletModel_, target.GetAimPosition(), {},
                { 1, 1, 1, 1 }, 30, { 0.5f, 0.5f, 1 }, charged ? 1.0f : 0.42f, fever ? 3 : 1);
            bullet->SetFeverShot(fever);
            playerBullets_.push_back(std::move(bullet));
            ++playerShotsFired_;
            CheckBulletEnemyCollisions();
        };
        reset();
        auto* center = spawn(0, 0);
        auto* neighbor = spawn(2.8f, 0);
        hit(*center, false);
        require(center->GetHp() == 2 && neighbor->GetHp() == 3 && playerHitCount_ == 1,
            "normal shot damage or no-splash rule changed");
        log("NORMAL_OK direct=1 no_splash=1");

        reset();
        center = spawn(0, 0, 6);
        neighbor = spawn(2.8f, 0);
        hit(*center, true);
        require(center->GetHp() == 3 && neighbor->IsDead() && defeatedEnemyCount_ == 1 && playerHitCount_ == 1,
            "charge direct damage stacked, splash failed, or accuracy inflated");
        const int score = score_, gauge = feverGauge_;
        CheckBulletEnemyCollisions();
        require(center->GetHp() == 3 && defeatedEnemyCount_ == 1 && score_ == score && feverGauge_ == gauge,
            "spent charge rewarded/damaged twice");
        log("SINGLE_HIT_OK direct=3 splash=3 reward_once=1 accuracy_once=1");

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
        require(center->IsDead() && neighbor->IsDead() && opposite->GetHp() == 3 && defeatedEnemyCount_ == 2,
            "edge hit chained across formation");
        log("NO_CHAIN_OK edge_hit_kills=2 opposite_ship_alive=1");

        reset();
        center = spawn(0, 0, 6);
        neighbor = spawn(4.0f, 0, 6); // 貫通弾そのものには触れない隣機。
        hit(*center, true, true);
        CheckBulletEnemyCollisions();
        require(center->GetHp() == 2 && neighbor->GetHp() == 6 && !playerBullets_.front()->IsDead() &&
            playerHitCount_ == 1 && hitEffects_.size() == 1,
            "fever damage, piercing or duplicate-hit prevention changed");
        log("FEVER_OK direct=4 piercing_retained=1 no_splash=1 no_repeat=1");

        reset();
        center = spawn(0, 0, 10, Enemy::Behavior::Shield);
        hit(*center, true);
        require(center->GetShieldHp() == 0 && center->GetHp() == 10, "charge bypassed shield");
        reset();
        center = spawn(0, 0, 52, Enemy::Behavior::Boss);
        hit(*center, true);
        require(center->GetHp() == 48, "boss direct damage doubled by own splash");
        bossCounterTimer_ = 30;
        hit(*center, true);
        require(center->GetHp() == 40, "boss counter damage changed");
        log("DEFENSE_OK shield_absorbs=3 boss_direct=4 boss_counter=8");

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
        SpawnStageEnemy(p.x, p.y, p.z - railDistance_, Enemy::Behavior::Formation,
            Enemy::EntryStyle::Direct, 3, 1.0f);
        auto* entering = enemies_.back().get();
        require(!entering->IsTargetable(), "entry fixture already targetable");
        hit(*center, true);
        require(entering->GetHp() == 3 && defeatedEnemyCount_ == 1, "blast hit entry/dead enemy");
        log("LIFETIME_OK entry_ignored=1 destroyed_ignored=1");

        reset();
        // ここからは手動の命中配置ではなく、本編と同じ動く編隊へ実際に一発撃つ。
        SpawnStageEnemy(-4.6f, -0.6f, 44.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemy(-2.0f, 0.5f, 44.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemy(0.6f, -0.6f, 44.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemy(5.3f, 2.0f, 48.0f, Enemy::Behavior::Sniper, Enemy::EntryStyle::PopShooter, 6, 1.18f);
        log("LIVE_FORMATION_BEGIN normal_input=1");
    }
    if (preview && frame == 99) {
        // 映像確認だけで全種類の本物の敵弾を並べる。次の射撃試験へ危険弾を持ち越さない。
        const std::array<EnemyBulletStyle, 6> styles{ EnemyBulletStyle::Standard, EnemyBulletStyle::Crossfire,
            EnemyBulletStyle::Sniper, EnemyBulletStyle::ShieldOrb, EnemyBulletStyle::BossCannon, EnemyBulletStyle::BossCharge };
        for (size_t i = 0; i < styles.size(); ++i) {
            FireEnemyBullet({ -6.0f + static_cast<float>(i) * 2.4f, 4.0f, railDistance_ + 26.0f }, styles[i]);
        }
        log("ENEMY_PROJECTILE_PREVIEW real_styles=6 cleared_before_charge_test=1");
    }
    if (preview && frame == 101) {
        for (auto& bullet : enemyBullets_) { bullet->Kill(); }
    }
    if (preview && frame == 160) {
        const int chain = defeatChainCount_;
        defeatChainCount_ = 12;
        AddFeverEnemyHitEffect({ 2.2f, 2.0f, railDistance_ + 26.0f }, 1.08f);
        defeatChainCount_ = chain;
        log("FEVER_BURST_PREVIEW real_effect=1 high_chain=12 gameplay_unchanged=1");
    }
    if (frame == 101 || frame == 102) {
        Math::Vector2 screen{};
        require(TryProjectToScreen((*std::next(enemies_.begin()))->GetAimPosition(), screen), "center offscreen");
        const ImVec2 origin = ImGui::GetMainViewport()->Pos;
        mouse = { screen.x - origin.x, screen.y - origin.y };
    }
    if (frame == 102) {
        require(chargeTimer_ >= chargeShotThreshold_, "normal waiting did not charge shot");
        require(lockedEnemy_ == (*std::next(enemies_.begin())).get() && isReticleOnTarget_,
            "fixture did not aim at formation center");
        keys[DIK_SPACE] = 0x80;
    }
    if (frame == 190) {
        log("LIVE_RESULT kills=" + std::to_string(defeatedEnemyCount_) + " shots=" +
            std::to_string(playerShotsFired_) + " hits=" + std::to_string(playerHitCount_));
        require(defeatedEnemyCount_ == 3 && playerShotsFired_ == 1 && playerHitCount_ == 1,
            "real charged projectile failed to clear moving three-ship formation");
        require(enemies_.size() == 1 && enemies_.front()->IsSniper() && enemies_.front()->GetHp() == 6,
            "real splash reached distant sniper");
        require(playerBulletPoolMisses_ == 0 && hitEffectObjectPoolMisses_ == 0, "effect/bullet pool exhausted");
        log("PASS real_projectile_kills=3 sniper_hp=6 shots=1 hits=1 pool_misses=0");
        input_->SetTestFrame(keys, mouse);
        return true;
    }
    input_->SetTestFrame(keys, mouse);
    return false;
}
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
        std::fill(stageEnemyEventTriggered_.begin(), stageEnemyEventTriggered_.end(), true);
        for (auto& effect : hitEffects_) { RecycleHitEffectVisuals(effect); }
        hitEffects_.clear();
        feverGauge_ = 0;
        feverTimer_ = 0;
        playerImpactSlowTimer_ = 0;
        cameraShakeTimer_ = 0;
        shootCooldown_ = 0;
        shootBufferTimer_ = 0;
        chargeTimer_ = 100;
    };
    const auto activeGate = [&]() -> RiftGate* {
        for (auto& rift : rifts_) { if (rift.active) { return &rift; } }
        return nullptr;
    };
    if (preview) {
        RiftGate* gate = activeGate();
        const bool stop = (previewStep == 0 && phaseFrame == 2) ||
            (previewStep == 1 && gate && gate->age >= 14.0f) ||
            (previewStep == 2 && gate && gate->position.z - railDistance_ < 12.0f) ||
            (previewStep == 3 && firstPass) ||
            (previewStep == 4 && phase == 1 && riftPassCount_ == 1);
        if (stop) {
            phantomPreviewPaused_ = true;
            ImGui::SetNextWindowPos({ 20.0f, 165.0f }, ImGuiCond_Always);
            ImGui::Begin("Rift visual fixture", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextUnformatted("TEST ONLY: real shot -> steer through kill location");
            ImGui::Text("Step %d  Passes %d  Gauge %d  Fever %d", previewStep,
                riftPassCount_, feverGauge_, feverTimer_);
            const bool next = ImGui::Button("NEXT / resume test", { 220.0f, 36.0f });
            ImGui::End();
            if (!next) { input_->SetTestFrame(keys, mouse); return false; }
            ++previewStep;
            phantomPreviewPaused_ = false;
        }
    }
    ++frame;
    ++phaseFrame;
    if (frame == 1) {
        reset();
        const float z = railDistance_ + 30.0f;
        const Math::Vector3 location{ 2.5f, 1.6f, z };
        const auto cross = [&](float x, float y) {
            UpdateRifts({ x, y, z - 2.0f }, { x, y, z + 2.0f });
        };
        SpawnStageEnemy(0.6f, 0.5f, 44, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 3, 1);
        Enemy* carrier = enemies_.back().get();
        require(carrier->IsRiftCarrier(), "formation carrier unmarked");
        for (int step = 0; step < 120; ++step) { carrier->Update(railDistance_); }
        const auto killPosition = carrier->GetAimPosition();
        carrier->Kill();
        OnEnemyDestroyed(*carrier, true, false);
        require(riftSpawnCount_ == 1 && activeGate() &&
            std::abs(activeGate()->position.x - killPosition.x) < 0.001f &&
            std::abs(activeGate()->position.y - killPosition.y) < 0.001f &&
            std::abs(activeGate()->position.z - killPosition.z) < 0.001f, "rift moved away from kill position");
        SpawnStageEnemy(3, 1, 44, Enemy::Behavior::Sniper, Enemy::EntryStyle::Direct, 3, 1);
        Enemy* ordinary = enemies_.back().get();
        require(!ordinary->IsRiftCarrier(), "ordinary enemy incorrectly marked");
        ordinary->Kill();
        OnEnemyDestroyed(*ordinary, false, false);
        require(riftSpawnCount_ == 1, "ordinary kill spawned a rift");
        log("CARRIER_OK marked_only=1 kill_position_preserved_xyz=1");

        reset();
        SpawnRift(location);
        const float age = activeGate()->age;
        isPaused_ = true;
        cross(location.x, location.y);
        require(activeGate() && activeGate()->age == age && riftPassCount_ == 0, "paused gate advanced");
        isPaused_ = false;
        cross(location.x + 1.4f, location.y + 1.1f);
        require(!activeGate() && riftPassCount_ == 0 && feverGauge_ == 0, "ellipse corner miss rewarded");
        SpawnRift(location);
        chargeTimer_ = 0;
        shootCooldown_ = 20;
        defeatChainCount_ = 2;
        defeatChainTimer_ = 1;
        UpdateRifts({ location.x - 4.0f, location.y, z - 2.0f },
            { location.x + 4.0f, location.y, z + 2.0f });
        require(riftPassCount_ == 1 && feverGauge_ == kRiftGaugeReward && chargeTimer_ == 100 &&
            shootCooldown_ == 4 && defeatChainTimer_ > 200, "swept pass failed to grant charge/gauge/chain");
        cross(location.x, location.y);
        require(riftPassCount_ == 1 && feverGauge_ == kRiftGaugeReward, "rift rewarded twice");
        SpawnRift(location);
        cross(location.x + kRiftRadiusX * 1.01f, location.y);
        require(riftPassCount_ == 1, "outside aperture rewarded");
        SpawnRift(location);
        cross(location.x + kRiftRadiusX * 0.99f, location.y);
        require(riftPassCount_ == 2, "inside aperture missed");
        log("CROSSING_OK swept_xy=1 ellipse=1 boundary=1 reward_once=1 pause_freezes=1");

        reset();
        feverTimer_ = 390;
        SpawnRift(location);
        cross(location.x, location.y);
        require(feverTimer_ == 480 && feverGauge_ == 0 && riftRecoveredFrames_ == 90, "fever recovery wrong");
        feverTimer_ = 550;
        SpawnRift(location);
        cross(location.x, location.y);
        require(feverTimer_ == 600 && riftRecoveredFrames_ == 50, "fever duration cap failed");
        log("FEVER_OK recovery=90 cap=600 actual_recovery_display=1");

        reset();
        for (size_t index = 0; index < rifts_.size() + 1; ++index) { SpawnRift(location); }
        require(riftSpawnCount_ == static_cast<int>(rifts_.size()), "gate pool overflow replaced a live gate");
        rifts_.front().age = rifts_.front().lifetime;
        UpdateRifts({ 0, 0, z - 10 }, { 0, 0, z - 9 });
        require(!rifts_.front().active && feverGauge_ == 0, "expired gate rewarded");
        reset();
        require(!activeGate() && riftPassCount_ == 0 && riftNoticeTimer_ == 0, "retry retained gates/rewards");
        playMode_ = PlayMode::Tutorial;
        SpawnRift(location);
        require(!activeGate(), "main-only rift leaked into tutorial");
        playMode_ = PlayMode::Game;
        SpawnRift(location);
        isGameOver_ = true;
        cross(location.x, location.y);
        require(!activeGate() && feverGauge_ == 0, "game-over gate rewarded");
        isGameOver_ = false;
        log("LIFETIME_OK fixed_pool=8 expired_no_reward=1 retry_resets=1 tutorial_unchanged=1 game_over=1");

        reset();
        Math::Vector3 movedMuzzle{ 6.0f, 3.0f, railDistance_ + 1.75f };
        const Math::Vector3 aimPoint{ -1.0f, 2.0f, movedMuzzle.z + 45.0f };
        require(TryProjectToScreen(aimPoint, reticleScreen_), "aim fixture offscreen");
        const auto direction = CalculateAimDirection(movedMuzzle);
        const float distance = (aimPoint.z - movedMuzzle.z) / direction.z;
        require(std::abs(movedMuzzle.x + direction.x * distance - aimPoint.x) < 0.02f &&
            std::abs(movedMuzzle.y + direction.y * distance - aimPoint.y) < 0.02f,
            "unassisted shot did not converge from displaced muzzle");
        log("AIM_OK displaced_muzzle_x=6 cursor_convergence=1");

        // 外れ弾が寿命まで残る最悪条件で、10秒以上の実際の射撃・弾更新を検証する。
        keys[DIK_SPACE] = 0x80;
        input_->SetTestFrame(keys, mouse);
        feverTimer_ = 600;
        for (int step = 0; step < 620; ++step) {
            chargeTimer_ = 100;
            UpdatePlayerShooting();
            UpdatePlayerBullets();
            UpdateHitEffects();
        }
        require(playerShotsFired_ >= 75 && playerBulletPoolMisses_ == 0 && hitEffectObjectPoolMisses_ == 0,
            "rapid fever fire exhausted fixed pools");
        log("RAPID_FIRE_OK shots=" + std::to_string(playerShotsFired_) + " pool_misses=0");
        keys.fill(0);
        reset();
        SpawnStageEnemy(4.0f, 1.4f, 48, Enemy::Behavior::Formation, Enemy::EntryStyle::Direct, 3, 1);
        carrier = enemies_.back().get();
        carrier->SetTrainingTarget(true);
        for (int step = 0; step < 120; ++step) { carrier->Update(railDistance_); }
        log("LIVE_NORMAL_BEGIN real_projectile=1 real_player_movement=1");
    }
    if (!fired && !enemies_.empty()) {
        Math::Vector2 screen{};
        require(TryProjectToScreen(enemies_.front()->GetAimPosition(), screen), "live enemy offscreen");
        const auto origin = ImGui::GetMainViewport()->Pos;
        mouse = { screen.x - origin.x, screen.y - origin.y };
        if (phaseFrame >= 4) {
            require(isReticleOnTarget_, "live fixture failed to lock target");
            keys[DIK_SPACE] = 0x80;
            fired = true;
        }
    }
    if (RiftGate* gate = activeGate()) {
        if (!liveGateSeen) { liveGateSeen = true; gaugeBeforePass = feverGauge_; }
        const auto position = player_->GetTranslate();
        const float x = gate->position.x - position.x;
        const float y = gate->position.y - position.y;
        if (std::abs(x) > 0.15f) { keys[x > 0 ? DIK_D : DIK_A] = 0x80; }
        if (std::abs(y) > 0.15f) { keys[y > 0 ? DIK_W : DIK_S] = 0x80; }
        feverBeforePass = feverTimer_;
    }
    if (phase == 0 && riftPassCount_ == 1) {
        require(liveGateSeen && playerShotsFired_ == 1 && defeatedEnemyCount_ == 1 &&
            feverGauge_ == gaugeBeforePass + kRiftGaugeReward && chargeTimer_ == 100,
            "real kill/steering pass failed normal rewards");
        if (!firstPass) { firstPass = true; log("LIVE_NORMAL_OK real_kill=1 steered_pass=1 gauge_plus=22 charge_ready=1"); }
        if (!preview || previewStep > 3) {
            reset();
            ActivateFever();
            feverTimer_ = 420; // 発動3秒後を配置し、上限とは別に1.5秒の実回復を検証する。
            phase = 1;
            phaseFrame = 0;
            fired = false;
            liveGateSeen = false;
            SpawnStageEnemy(2.0f, 2.0f, 48, Enemy::Behavior::Formation, Enemy::EntryStyle::Direct, 4, 1);
            auto* enemy = enemies_.back().get();
            enemy->SetTrainingTarget(true);
            for (int step = 0; step < 120; ++step) { enemy->Update(railDistance_); }
            log("LIVE_FEVER_BEGIN remaining_frames=420");
        }
    }
    if (phase == 1 && riftPassCount_ == 1 && (!preview || previewStep > 4)) {
        log("LIVE_FEVER_RESULT before=" + std::to_string(feverBeforePass) + " after=" +
            std::to_string(feverTimer_) + " recovered=" + std::to_string(riftRecoveredFrames_) +
            " shots=" + std::to_string(playerShotsFired_) + " kills=" + std::to_string(defeatedEnemyCount_));
        require(liveGateSeen && feverTimer_ == feverBeforePass - 1 + kRiftFeverRecoveryFrames &&
            playerShotsFired_ == 1 && defeatedEnemyCount_ == 1 && riftRecoveredFrames_ == 90,
            "real fever kill/steering pass failed duration recovery");
        require(playerBulletPoolMisses_ == 0 && hitEffectObjectPoolMisses_ == 0, "live pool exhaustion");
        log("PASS normal_and_fever_real_shot_pass=1 fever_plus=90 pools=0");
        input_->SetTestFrame({}, mouse);
        return true;
    }
    require(frame < 700, "live rift steering timed out or missed gate");
    input_->SetTestFrame(keys, mouse);
    return false;
}
#endif
