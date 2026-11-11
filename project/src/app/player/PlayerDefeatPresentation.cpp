// ===== PlayerDefeatPresentation.cpp =====
// 自機が撃墜された後の墜落、破片、炎とカメラの動きを進める。

#include "app/player/PlayerDefeatPresentation.h"

#include "app/ui/CombatHud.h"
#include "app/runtime/GameRuntimeMath.h"
#include "app/player/Player.h"
#include "engine/3d/Camera.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Object3dCommon.h"

#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numbers>
#include <random>
#include <utility>

namespace {
using namespace GameRuntimeMath;
constexpr float kDefeatBurstTime = 1.25f;
constexpr float kDefeatMenuTime = 2.05f;
constexpr float kDebrisGroundY = -3.02f;
constexpr float kTwoPi = std::numbers::pi_v<float> * 2.0f;
constexpr std::array<size_t, 3> kBurningDebris{ 0, 2, 3 };
float Ease(float t) { t = std::clamp(t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }

// 時刻と乱数の種から風の揺らぎを求め、前後の値を補間して炎の変化をつなぐ。
float FireGust(float time, float seed)
{
    const float cell = std::floor(time + seed);
    const auto noise = [](float value) { const float n = std::sin(value * 12.9898f) * 43758.5453f; return n - std::floor(n); };
    return Lerp(noise(cell), noise(cell + 1.0f), Ease(time + seed - cell));
}

// 頂点の座標を走査し、破片の形を囲む最小・最大座標を返す。
std::pair<Math::Vector3, Math::Vector3> MeshBounds(const std::vector<VertexData>& vertices)
{
    const auto& first = vertices.front().position;
    Math::Vector3 lo{ first.x, first.y, first.z }, hi = lo;
    for (const auto& vertex : vertices) {
        const auto& p = vertex.position;
        lo = { (std::min)(lo.x, p.x), (std::min)(lo.y, p.y), (std::min)(lo.z, p.z) };
        hi = { (std::max)(hi.x, p.x), (std::max)(hi.y, p.y), (std::max)(hi.z, p.z) };
    }
    return { lo, hi };
}

float RandomRange(float low, float high)
{
    static std::mt19937 generator(static_cast<unsigned int>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    return std::uniform_real_distribution<float>(low, high)(generator);
}

// 破片を現在の姿勢へ回して最も低い頂点を求め、道路との接地に使う。
float DebrisBottom(const std::vector<Math::Vector3>& vertices, const Math::Vector3& rotation)
{
    const auto matrix = Math::Multiply(Math::MakeRotateXMatrix(rotation.x),
        Math::Multiply(Math::MakeRotateYMatrix(rotation.y), Math::MakeRotateZMatrix(rotation.z)));
    float bottom = 0.0f;
    for (const auto& vertex : vertices) {
        bottom = (std::min)(bottom, vertex.x * matrix.m[0][1] + vertex.y * matrix.m[1][1] + vertex.z * matrix.m[2][1]);
    }
    return bottom;
}
}

// 機体から飛び散る部品を描くためのモデルと描画オブジェクトを、撃墜前に準備する。
void PlayerDefeatPresentation::InitializeDebris(Object3dCommon& common, Camera& camera, const Model* ship, float shipScale)
{
    if (!ship || ship->GetVertices().empty()) { return; }
    const auto& vertices = ship->GetVertices();
    const auto& indices = ship->GetIndices();
    const auto [lo, hi] = MeshBounds(vertices);
    const Math::Vector3 center{ (lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f };
    std::array<ModelData, 4> parts;
    for (auto& part : parts) {
        part.materials = ship->GetMaterials();
        part.material.textureFilePath = ship->GetTextureFilePath();
    }
    // 同じ頂点・UV・材質を使い、三角形を翼と胴体へ振り分ける。
    const auto splitRange = [&](const ModelDrawRange& range) {
        std::array<uint32_t, 4> starts{};
        for (size_t p = 0; p < parts.size(); ++p) { starts[p] = static_cast<uint32_t>(parts[p].indices.size()); }
        const size_t end = (std::min)(indices.size(), static_cast<size_t>(range.indexOffset) + range.indexCount);
        for (size_t triangle = range.indexOffset; triangle + 2 < end; triangle += 3) {
            const auto& a = vertices[indices[triangle]].position;
            const auto& b = vertices[indices[triangle + 1]].position;
            const auto& c = vertices[indices[triangle + 2]].position;
            const float x = (a.x + b.x + c.x) / 3.0f - center.x;
            const float z = (a.z + b.z + c.z) / 3.0f - center.z;
            const size_t piece = x < -(hi.x - lo.x) * 0.22f ? 2 : x > (hi.x - lo.x) * 0.22f ? 3 :
                z > (hi.z - lo.z) * 0.12f ? 1 : 0;
            auto& part = parts[piece];
            for (size_t corner = 0; corner < 3; ++corner) {
                part.indices.push_back(static_cast<uint32_t>(part.vertices.size()));
                part.vertices.push_back(vertices[indices[triangle + corner]]);
            }
        }
        for (size_t p = 0; p < parts.size(); ++p) {
            const auto count = static_cast<uint32_t>(parts[p].indices.size()) - starts[p];
            if (count) { parts[p].drawRanges.push_back({ starts[p], count, range.materialIndex }); }
        }
    };
    if (ship->GetDrawRanges().empty()) { splitRange({ 0, static_cast<uint32_t>(indices.size()), 0 }); }
    else { for (const auto& range : ship->GetDrawRanges()) { splitRange(range); } }

    for (size_t p = 0; p < parts.size(); ++p) {
        auto& debris = defeatDebris_[p];
        debris.active = false;
        auto& part = parts[p];
        if (part.vertices.empty()) { continue; }
        const auto [partLo, partHi] = MeshBounds(part.vertices);
        const Math::Vector3 pivot{ (partLo.x + partHi.x) * 0.5f, (partLo.y + partHi.y) * 0.5f, (partLo.z + partHi.z) * 0.5f };
        const float scale = shipScale;
        debris.localOffset = { (pivot.x - center.x) * scale, (pivot.y - center.y) * scale, (pivot.z - center.z) * scale };
        for (auto& vertex : part.vertices) {
            vertex.position.x -= pivot.x; vertex.position.y -= pivot.y; vertex.position.z -= pivot.z;
        }
        debris.contactVertices.clear();
        debris.contactVertices.reserve(part.vertices.size());
        for (const auto& vertex : part.vertices) {
            debris.contactVertices.push_back({ vertex.position.x * scale, vertex.position.y * scale, vertex.position.z * scale });
        }
        // 箱の高さではなく、実際の三角形上に火元を取る。裏返った翼にも対応する。
        for (size_t site = 0; site < debris.fireAnchors.size(); ++site) {
            const Math::Vector3 target{ (site % 2 == 0 ? -0.20f : 0.20f) * (partHi.x - partLo.x),
                (site < 4 ? partHi.y : partLo.y) - pivot.y,
                (site % 4 < 2 ? -0.18f : 0.18f) * (partHi.z - partLo.z) };
            float closest = (std::numeric_limits<float>::max)();
            for (size_t triangle = 0; triangle + 2 < part.vertices.size(); triangle += 3) {
                Math::Vector3 point{};
                for (size_t corner = 0; corner < 3; ++corner) {
                    const auto& v = part.vertices[triangle + corner].position;
                    point.x += v.x / 3.0f; point.y += v.y / 3.0f; point.z += v.z / 3.0f;
                }
                const float distance = DistanceSquared(point, target);
                if (distance < closest) { closest = distance; debris.fireAnchors[site] = point * scale; }
            }
        }
        const auto name = std::string("player_defeat_part_") + std::to_string(p);
        auto* models = ModelManager::GetInstance();
        models->CreateMesh(name, std::move(part));
        debris.object = std::make_unique<Object3d>();
        debris.object->Initialize(&common);
        debris.object->SetCamera(&camera);
        debris.object->SetModel(models->FindModel(name));
        debris.object->SetScale({ scale, scale, scale });
        debris.object->SetLightingMode(1);
        debris.object->SetColor({ 0.40f, 0.40f, 0.41f, 1.0f });
        debris.object->SetEnvironmentCoefficient(0.025f);
        debris.object->SetRoughness(0.72f);
        debris.object->SetMetallic(0.20f);
    }
}

// 破片に設定した上下の燃焼位置を回転させ、機体の崩れに追従する炎の位置を得る。
Math::Vector3 PlayerDefeatPresentation::GetDefeatFireOrigin(size_t piece, size_t site) const
{
    const auto& debris = defeatDebris_[piece];
    const auto upper = RotateLocalOffset(debris.fireAnchors[site % 4], debris.rotation);
    const auto lower = RotateLocalOffset(debris.fireAnchors[site % 4 + 4], debris.rotation);
    const auto point = upper.y >= lower.y ? upper : lower;
    return { debris.position.x + point.x, debris.position.y + point.y, debris.position.z + point.z };
}

// 撃墜時に使う破片、火花、炎、燃料跡の描画オブジェクトをあらかじめ用意する。
void PlayerDefeatPresentation::Initialize(Object3dCommon& common, Camera& camera, const Models& models, float shipScale)
{
    playerDefeat_ = {};
    InitializeDebris(common, camera, models.ship, shipScale);
    for (size_t index = 0; index < defeatMotes_.size(); ++index) {
        auto& mote = defeatMotes_[index];
        mote = {};
        mote.spark = index % 4 == 0;
        mote.object = std::make_unique<Object3d>();
        mote.object->Initialize(&common);
        mote.object->SetCamera(&camera);
        mote.object->SetModel(mote.spark ? models.spark : models.smoke);
        mote.object->SetLightingMode(0);
        mote.object->SetEnvironmentCoefficient(0.0f);
        mote.object->SetAlphaReference(0.005f);
        mote.object->SetColor({ 0, 0, 0, 0 });
        if (!mote.spark) { mote.object->SetTextureFilePath("resources/effects/wreck_smoke_flipbook.png"); }
    }
    for (auto& fire : defeatFire_) {
        fire = std::make_unique<Object3d>();
        fire->Initialize(&common);
        fire->SetCamera(&camera);
        fire->SetModel(models.fire);
        fire->SetLightingMode(0);
        fire->SetEnvironmentCoefficient(0.0f);
        fire->SetAlphaReference(0.005f);
        // テクスチャは起動時に読み込み済み。死亡時の追加読み込みを避ける。
        fire->SetTextureFilePath("resources/effects/wreck_fire_flipbook.png");
    }
    for (auto& glow : defeatFireGlow_) {
        glow = std::make_unique<Object3d>();
        glow->Initialize(&common);
        glow->SetCamera(&camera);
        glow->SetModel(models.glow);
        glow->SetLightingMode(0);
        glow->SetEnvironmentCoefficient(0.0f);
        glow->SetAlphaReference(0.002f);
    }
    defeatFuelStain_ = std::make_unique<Object3d>();
    defeatFuelStain_->Initialize(&common);
    defeatFuelStain_->SetCamera(&camera);
    defeatFuelStain_->SetModel(models.fire);
    defeatFuelStain_->SetLightingMode(5);
    defeatFuelStain_->SetEnvironmentCoefficient(0.0f);
}

// 撃墜時の自機・カメラ・被弾方向を控え、墜落と破片の初期状態を決める。
void PlayerDefeatPresentation::Start(const Player& player, const Camera& camera,
    float fov, float railSpeed, const Math::Vector3& incomingVelocity)
{
    playerDefeat_ = {};
    for (auto& debris : defeatDebris_) { debris.active = false; }
    auto& defeat = playerDefeat_;
    defeat.elapsed = 0.0f;
    defeat.origin = player.GetTranslate();
    defeat.rotation = player.GetVisualRotate();
    defeat.cameraPosition = camera.GetTranslate();
    defeat.cameraRotation = camera.GetRotate();
    defeat.startFov = fov;
    defeat.forwardSpeed = std::clamp(railSpeed * 60.0f * RandomRange(0.88f, 1.12f), 15.0f, 46.0f);
    // 爆発の時刻と機体の崩れ方に幅を持たせ、墜落のたびに同じ動きになるのを避ける。
    defeat.burstTime = RandomRange(1.02f, kDefeatBurstTime);
    defeat.lateralTravel = RandomRange(1.1f, 3.2f);
    defeat.roll = RandomRange(0.85f, 1.65f);
    defeat.pitch = RandomRange(0.22f, 0.65f);
    defeat.yaw = RandomRange(0.12f, 0.46f);
    defeat.travelScale = RandomRange(0.30f, 0.43f);
    defeat.variation = RandomRange(0.0f, 100.0f);
    // 横から受けた弾はその方向へ崩す。正面弾は道路の中央へ流し、建物への食い込みを避ける。
    defeat.side = std::abs(incomingVelocity.x) > 0.05f && RandomRange(0.0f, 1.0f) < 0.80f ?
        (incomingVelocity.x > 0.0f ? 1.0f : -1.0f) : (RandomRange(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f);
    defeat.wreckPosition = defeat.origin;
    for (int index = 0; index < 6; ++index) { EmitDefeatMote(defeat.origin, true, true); }
}

// 空いている炎・煙・火花の枠を探し、位置や寿命を決めて発生させる。
void PlayerDefeatPresentation::EmitDefeatMote(const Math::Vector3& position, bool spark, bool burst, float strength)
{
    for (auto& mote : defeatMotes_) {
        if (!mote.object || mote.age >= 0.0f || mote.spark != spark) { continue; }
        const float seed = static_cast<float>(++playerDefeat_.emission) + playerDefeat_.variation;
        const float a = seed * 2.399963f;
        mote.age = 0.0f;
        mote.position = position;
        mote.ember = spark && !burst;
        mote.strength = strength;
        mote.lifetime = mote.ember ? 0.85f + std::fmod(seed * 0.173f, 0.45f) :
            spark ? 0.65f + std::fmod(seed * 0.173f, 0.65f) :
            burst ? 1.20f + std::fmod(seed * 0.317f, 0.40f) :
            strength < 0.6f ? 1.45f + std::fmod(seed * 0.317f, 0.40f) : 3.0f + std::fmod(seed * 0.317f, 0.55f);
        mote.size = mote.ember ? 0.045f + std::fmod(seed * 0.013f, 0.020f) :
            spark ? 0.09f : (burst ? 1.0f : strength < 0.6f ? 0.36f : 0.82f + std::fmod(seed * 0.073f, 0.22f));
        mote.roll = a;
        mote.velocity = mote.ember ? Math::Vector3{ std::cos(a) * 0.38f,
            1.0f + std::fmod(seed * 0.271f, 0.85f), std::sin(a) * 0.30f } :
            spark ? Math::Vector3{ std::cos(a) * 4.8f, 1.3f + std::sin(a) * 3.0f, std::sin(a * 1.7f) * 3.0f } :
            Math::Vector3{ 0.38f + std::cos(a) * (burst ? 0.65f : 0.12f),
                strength < 0.6f ? 0.55f : 1.30f + std::fmod(seed * 0.13f, 0.40f), 0.12f + std::sin(a) * 0.12f };
        return;
    }
}

// 実時間で墜落と破片の動きを進める。爆発した瞬間とカメラ姿勢を呼び出し元へ返す。
PlayerDefeatPresentation::Frame PlayerDefeatPresentation::Update(Player& player, float dt)
{
    Frame frame;
    auto& defeat = playerDefeat_;
    defeat.elapsed += dt;
    // 致命打だけ短くため、以後は実時間で失速させる。レール進行や敵の出現は進めない。
    const float t = std::clamp((defeat.elapsed - 0.09f) / (defeat.burstTime - 0.09f), 0.0f, 1.0f);
    const float travel = defeat.forwardSpeed * defeat.travelScale * (1.0f - std::exp(-2.7f * t));
    const float fall = t * t;
    if (!defeat.exploded) {
        const Math::Vector3 rotation{ defeat.rotation.x + defeat.pitch * fall,
            defeat.rotation.y + defeat.side * defeat.yaw * Ease(t),
            defeat.rotation.z - defeat.side * defeat.roll * (0.55f * t + 0.45f * t * t) };
        float contactHeight = kDebrisGroundY;
        for (const auto& debris : defeatDebris_) {
            const auto offset = RotateLocalOffset(debris.localOffset, rotation);
            contactHeight = (std::max)(contactHeight,
                kDebrisGroundY - offset.y - DebrisBottom(debris.contactVertices, rotation));
        }
        defeat.wreckPosition = { defeat.origin.x + defeat.side * defeat.lateralTravel * Ease(t),
            Lerp(defeat.origin.y, contactHeight, fall), defeat.origin.z + travel };
        player.SetCinematicPose(defeat.wreckPosition, rotation);
        if (defeat.elapsed >= defeat.burstTime) {
            defeat.exploded = true;
            for (size_t piece = 0; piece < defeatDebris_.size(); ++piece) {
                auto& debris = defeatDebris_[piece];
                if (!debris.object) { continue; }
                debris.active = true;
                const auto offset = RotateLocalOffset(debris.localOffset, rotation);
                debris.position = { defeat.wreckPosition.x + offset.x, defeat.wreckPosition.y + offset.y,
                    defeat.wreckPosition.z + offset.z };
                debris.rotation = rotation;
                const float angle = std::atan2(offset.x, offset.z) + RandomRange(-0.85f, 0.85f);
                const float impulse = piece == 0 ? RandomRange(0.7f, 1.7f) : RandomRange(2.2f, 5.4f);
                debris.velocity = { std::sin(angle) * impulse + defeat.side * RandomRange(0.1f, 0.8f),
                    piece == 0 ? RandomRange(0.3f, 1.1f) : RandomRange(1.0f, 3.4f),
                    defeat.forwardSpeed * RandomRange(0.08f, 0.22f) + std::cos(angle) * impulse };
                const float spin = piece == 0 ? 1.5f : 4.8f;
                debris.spin = { RandomRange(-spin, spin), RandomRange(-spin, spin), RandomRange(-spin, spin) };
                debris.restitution = RandomRange(0.12f, 0.32f);
                debris.drag = RandomRange(4.4f, 7.5f);
                debris.restPitch = RandomRange(-0.045f, 0.045f);
                debris.restRoll = piece >= 2 && RandomRange(0.0f, 1.0f) < 0.45f ? std::numbers::pi_v<float> : 0.0f;
            }
            frame.burstPosition = defeat.wreckPosition;
            for (int index = 0; index < 12; ++index) {
                if (index < 7) { EmitDefeatMote(defeat.wreckPosition, false, true); }
                EmitDefeatMote(defeat.wreckPosition, true, true);
            }
        }
    }
    if (defeat.exploded) {
        for (size_t piece = 0; piece < defeatDebris_.size(); ++piece) {
            auto& debris = defeatDebris_[piece];
            if (!debris.active) { continue; }
            debris.velocity.y -= 8.5f * dt;
            debris.position = { debris.position.x + debris.velocity.x * dt, debris.position.y + debris.velocity.y * dt,
                debris.position.z + debris.velocity.z * dt };
            debris.rotation = { debris.rotation.x + debris.spin.x * dt, debris.rotation.y + debris.spin.y * dt,
                debris.rotation.z + debris.spin.z * dt };
            const float floor = kDebrisGroundY - DebrisBottom(debris.contactVertices, debris.rotation);
            if (debris.position.y < floor) {
                debris.position.y = floor;
                if (piece == 0 && defeat.fuelStarted < 0.0f) {
                    defeat.fuelStarted = defeat.elapsed;
                    defeat.fuelOrigin = { debris.position.x, kDebrisGroundY + 0.026f, debris.position.z };
                }
                debris.velocity.y = debris.velocity.y < -0.9f ? -debris.velocity.y * debris.restitution : 0.0f;
                const float drag = std::exp(-debris.drag * dt);
                debris.velocity.x *= drag; debris.velocity.z *= drag;
                debris.spin = debris.spin * drag;
                // 接地後は平たい姿勢へ落ち着かせ、翼が縦に立ったままにならないようにする。
                const float settle = 1.0f - std::exp(-5.0f * dt);
                debris.rotation.x += std::remainder(debris.restPitch - debris.rotation.x, kTwoPi) * settle;
                debris.rotation.z += std::remainder(debris.restRoll - debris.rotation.z, kTwoPi) * settle;
                debris.position.y = kDebrisGroundY - DebrisBottom(debris.contactVertices, debris.rotation);
            }
            debris.object->SetTranslate(debris.position);
            debris.object->SetRotate(debris.rotation);
            auto lightPosition = defeatDebris_[0].position;
            lightPosition.y += 0.70f;
            debris.object->SetPointLightPosition(lightPosition);
            debris.object->SetPointLightColor({ 1.0f, 0.18f, 0.018f });
            debris.object->SetPointLightIntensity(0.78f + std::sin(defeat.elapsed * 4.7f) * 0.09f +
                std::sin(defeat.elapsed * 9.1f) * 0.05f);
            debris.object->Update();
        }
        if (defeatDebris_[0].active) { defeat.wreckPosition = defeatDebris_[0].position; }
    }
    const float follow = Ease(defeat.elapsed / 1.65f);
    const float impact = std::exp(-defeat.elapsed * 20.0f) * 0.045f;
    const float burst = defeat.exploded ? std::exp(-(defeat.elapsed - defeat.burstTime) * 12.0f) * 0.045f : 0.0f;
    auto& pose = frame.camera;
    pose.position = { defeat.cameraPosition.x + defeat.side * 0.55f * follow,
        defeat.cameraPosition.y - 0.30f * follow,
        defeat.cameraPosition.z + travel * 0.46f };
    pose.rotation = { defeat.cameraRotation.x + 0.08f * follow + std::sin(defeat.elapsed * 91.0f) * (impact + burst),
        defeat.cameraRotation.y + defeat.side * 0.035f * follow,
        defeat.cameraRotation.z + defeat.side * 0.025f * follow };
    pose.fov = defeat.startFov - 0.06f * follow;
    defeat.smokeClock += dt;
    const float fireAge = (std::max)(0.0f, defeat.elapsed - defeat.burstTime);
    const float interval = defeat.exploded ? (fireAge < 1.5f ? 0.12f : 0.16f) : 0.075f;
    if (defeat.smokeClock >= interval) {
        defeat.smokeClock -= interval;
        auto source = defeat.wreckPosition;
        if (defeat.exploded) {
            source = Lerp(GetDefeatFireOrigin(0, 0), GetDefeatFireOrigin(0, 3), 0.5f);
            source.y += 0.52f;
        }
        EmitDefeatMote(source, false, false);
    }
    if (defeat.exploded) {
        defeat.smolderClock += dt;
        if (defeat.smolderClock >= 0.48f && fireAge < 4.0f) {
            defeat.smolderClock -= 0.48f;
            const size_t piece = defeat.emission % 2 == 0 ? 2 : 3;
            EmitDefeatMote(GetDefeatFireOrigin(piece, 0), false, false, 0.35f);
        }
        defeat.emberClock += dt;
        if (defeat.emberClock >= 0.10f) {
            defeat.emberClock -= 0.10f;
            auto source = GetDefeatFireOrigin(0, defeat.emission % 4);
            source.y += 0.10f;
            EmitDefeatMote(source, true, false);
        }
    }
    for (auto& mote : defeatMotes_) {
        if (mote.age < 0.0f) { continue; }
        mote.age += dt;
        if (mote.age >= mote.lifetime) { mote.age = -1.0f; }
    }
    return frame;
}

// 爆発後に残っている部品だけを、現在の姿勢で描く。
void PlayerDefeatPresentation::DrawDebris() const
{
    if (!playerDefeat_.exploded) { return; }
    for (const auto& debris : defeatDebris_) {
        if (debris.active && debris.object) { debris.object->Draw(); }
    }
}

// 残った機体や破片の位置を基準に、炎・煙・火花を描く。
void PlayerDefeatPresentation::DrawEffects(Object3dCommon& common, const Camera& camera, const Player& player)
{
    if (!IsActive()) { return; }
    const auto oldBlend = common.GetBlendMode();
    const auto oldDepth = common.GetDepthDrawMode();
    common.SetDepthDrawMode(DepthDrawMode::ReadOnly);
    struct TransparentEffect { Object3d* object = nullptr; bool flame = false; };
    std::array<TransparentEffect, 60> translucent{};
    std::array<Object3d*, 48> sparks{};
    size_t translucentCount = 0, sparkCount = 0;
    for (auto& mote : defeatMotes_) {
        if (!mote.object || mote.age < 0.0f) { continue; }
        const bool spark = mote.spark;
        const float t = mote.age / mote.lifetime;
        const float fade = spark ? (1.0f - t) * (mote.ember ? Ease(t * 9.0f) : 1.0f) :
            Ease(t * 9.0f) * (1.0f - Ease((t - 0.45f) / 0.55f));
        const float size = mote.size * (spark ? 1.0f : 1.0f + mote.age * 0.65f);
        const float drift = mote.ember ? std::sin(mote.roll + mote.age * 5.0f) * mote.age * 0.12f :
            !spark ? std::sin(mote.roll + mote.age * 2.2f) * mote.age * 0.10f : 0.0f;
        mote.object->SetTranslate({ mote.position.x + mote.velocity.x * mote.age + drift,
            (std::max)(kDebrisGroundY + 0.06f, mote.position.y + mote.velocity.y * mote.age -
                (spark && !mote.ember ? 1.8f * mote.age * mote.age : 0.0f)),
            mote.position.z + mote.velocity.z * mote.age });
        auto rotation = camera.GetRotate();
        rotation.z += mote.roll + mote.age * (spark ? 1.6f : 0.09f);
        mote.object->SetRotate(rotation);
        mote.object->SetScale({ size, size * (mote.ember ? 1.35f : spark ? 5.0f : 1.25f), 1.0f });
        const float grey = mote.strength < 0.6f ? Lerp(0.10f, 0.18f, t) : Lerp(0.026f, 0.11f, t);
        mote.object->SetColor(mote.ember ? Math::Vector4{ Lerp(1.65f, 0.65f, t),
            Lerp(0.80f, 0.09f, t), Lerp(0.20f, 0.015f, t), fade } :
            spark ? Math::Vector4{ 1.40f, 0.58f, 0.16f, fade * 0.90f } :
            Math::Vector4{ grey, grey * 1.01f, grey * 1.03f, fade * mote.strength * 0.72f });
        if (!spark) {
            auto flow = Math::MakeIdentity4x4();
            flow.m[3][0] = mote.age;
            flow.m[3][1] = mote.roll;
            mote.object->SetUVTransform(flow);
            mote.object->SetLightingMode(4);
        }
        mote.object->Update();
        if (spark) { sparks[sparkCount++] = mote.object.get(); }
        else { translucent[translucentCount++] = { mote.object.get(), false }; }
    }
    const auto& defeat = playerDefeat_;
    const float fireAge = (std::max)(0.0f, defeat.elapsed - defeat.burstTime);
    const float heat = 0.48f + 0.52f * std::exp(-fireAge * 0.45f);
    const float fuelAge = defeat.fuelStarted < 0.0f ? -1.0f : defeat.elapsed - defeat.fuelStarted;
    const float spread = Ease(fuelAge / 1.1f);
    const auto& core = defeatDebris_[0];
    const auto fuelEnd = Math::Vector3{ core.position.x, kDebrisGroundY + 0.026f, core.position.z };
    const Math::Vector3 delta{ fuelEnd.x - defeat.fuelOrigin.x, 0.0f, fuelEnd.z - defeat.fuelOrigin.z };
    const float distance = std::sqrt(delta.x * delta.x + delta.z * delta.z);
    const float fuelYaw = distance > 0.10f ? std::atan2(delta.x, delta.z) : core.rotation.y;
    const Math::Vector3 fuelForward{ std::sin(fuelYaw), 0.0f, std::cos(fuelYaw) };
    const Math::Vector3 fuelSide{ std::cos(fuelYaw), 0.0f, -std::sin(fuelYaw) };
    const auto fuelCenter = Lerp(defeat.fuelOrigin, fuelEnd, 0.5f);
    const float fuelLength = (std::min)(4.8f, distance + 2.2f) * (0.45f + spread * 0.55f);
    const float fuelWidth = 1.05f + spread * 1.25f;
    const auto fuelPoint = [&](size_t index) {
        const float seed = static_cast<float>(index) * 2.399963f + defeat.variation;
        const float along = std::fmod(static_cast<float>(index) * 0.618034f, 1.0f) - 0.5f;
        const float across = std::sin(seed) * 0.32f * std::sqrt((std::max)(0.1f, 1.0f - along * along * 4.0f));
        auto point = fuelCenter;
        point += fuelForward * (along * fuelLength * 0.78f);
        point += fuelSide * (across * fuelWidth);
        return point;
    };

    // 接地した胴体から漏れた燃料だけを路面へ残す。飛んでいる部品には追従させない。
    if (defeatFuelStain_ && fuelAge >= 0.0f) {
        common.SetBlendMode(BlendMode::Normal);
        common.CommonDrawSetting();
        auto flow = Math::MakeIdentity4x4();
        flow.m[3][0] = fuelAge; flow.m[3][1] = defeat.variation;
        defeatFuelStain_->SetUVTransform(flow);
        defeatFuelStain_->SetTranslate(fuelCenter);
        defeatFuelStain_->SetRotate({ std::numbers::pi_v<float> * 0.5f, fuelYaw, 0.0f });
        defeatFuelStain_->SetScale({ fuelWidth, fuelLength, 1.0f });
        defeatFuelStain_->SetColor({ 1, 1, 1, Ease(fuelAge / 0.3f) * 0.72f });
        defeatFuelStain_->Update();
        defeatFuelStain_->Draw();
    }

    for (size_t index = 0; index < defeatFire_.size(); ++index) {
        auto& fire = defeatFire_[index];
        if (!fire) { continue; }
        const float phase = static_cast<float>(index) * 2.17f + defeat.variation * 0.31f;
        const float gust = FireGust(fireAge * 1.4f, phase);
        float width = 0.22f, height = 0.42f, strength = 0.65f;
        Math::Vector3 root{};
        float yaw = camera.GetRotate().y;
        if (!defeat.exploded) {
            if (index >= 2) { continue; }
            const float side = index == 0 ? -1.0f : 1.0f;
            root = defeat.wreckPosition;
            root += RotateLocalOffset({ side * 0.44f, -0.14f, -1.35f }, player.GetVisualRotate());
        } else if (index < 2) {
            if (!core.active) { continue; }
            // 破損部の内側から、重なった薄い炎が一続きに立ち上がる。
            root = Lerp(GetDefeatFireOrigin(0, 0), GetDefeatFireOrigin(0, 3), 0.5f);
            root.y -= 0.18f;
            root.z += index == 0 ? -0.15f : 0.20f;
            const float flare = std::exp(-fireAge * 3.6f) + std::pow((std::max)(0.0f, (gust - 0.58f) / 0.42f), 3.0f) * 0.60f;
            width = index == 0 ? 1.75f : 1.35f;
            height = (index == 0 ? 1.55f : 1.15f) * (heat + flare * 0.55f);
            strength = heat * 0.90f + flare * 0.15f;
            yaw += index == 0 ? -0.14f : 0.52f;
        } else if (index < 10) {
            if (fuelAge < 0.0f) { continue; }
            const size_t patch = index - 2;
            const float ignition = Ease((fuelAge - static_cast<float>(patch % 4) * 0.09f) / 0.70f);
            const float fuelHeat = 0.50f + 0.50f * std::exp(-fuelAge * 0.50f);
            root = fuelPoint(patch);
            width = 0.50f + gust * 0.24f;
            height = (0.25f + std::pow(gust, 1.6f) * 0.55f) * fuelHeat;
            strength = ignition * fuelHeat * (0.50f + gust * 0.45f);
            if (strength <= 0.01f) { continue; }
        } else {
            const size_t piece = index - 8;
            const float remaining = 1.0f - Ease((fireAge - 0.25f) / 1.65f);
            if (!defeatDebris_[piece].active || remaining <= 0.01f) { continue; }
            root = GetDefeatFireOrigin(piece, 0);
            width = 0.25f; height = 0.12f + 0.20f * remaining;
            strength = remaining * 0.28f;
        }
        fire->SetTranslate({ root.x, root.y + height * 0.5f - 0.055f, root.z });
        fire->SetRotate({ 0.0f, yaw, 0.0f });
        fire->SetScale({ width, height, 1.0f });
        fire->SetFlameAnimation(defeat.elapsed, phase * 3.1f);
        fire->SetColor({ 1.10f, 0.86f, 0.65f, strength });
        fire->Update();
        translucent[translucentCount++] = { fire.get(), true };
    }
    common.SetBlendMode(BlendMode::Add);
    common.CommonDrawSetting();
    if (playerDefeat_.exploded) {
        for (size_t index = 0; index < defeatFireGlow_.size(); ++index) {
            auto& glow = defeatFireGlow_[index];
            if (!glow || !core.active || (index > 0 && fuelAge < 0.0f)) { continue; }
            const auto at = index == 0 ? core.position : fuelPoint(index == 1 ? 1 : 5);
            const float contact = index == 0 ? std::clamp(1.0f - (core.position.y - kDebrisGroundY) / 2.2f, 0.0f, 1.0f) : spread;
            const float pulse = 0.88f + std::sin(playerDefeat_.elapsed * 13.0f + static_cast<float>(index)) * 0.12f;
            const float radius = index == 0 ? 1.45f : 0.65f;
            glow->SetTranslate({ at.x, kDebrisGroundY + 0.035f, at.z });
            glow->SetRotate({ std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f });
            glow->SetScale({ radius, radius * 0.80f, 1.0f });
            glow->SetColor({ 1.0f, 0.23f, 0.025f, contact * pulse * heat * (index == 0 ? 0.06f : 0.03f) });
            glow->Update();
            glow->Draw();
        }
    }
    const auto depth = [&](const Object3d* object) {
        const auto at = object->GetTranslate();
        const auto& view = camera.GetViewMatrix();
        return at.x * view.m[0][2] + at.y * view.m[1][2] + at.z * view.m[2][2];
    };
    // 炎と煙を一緒に奥から合成。煙が手前にあるときは炎も薄く隠れる。
    std::sort(translucent.begin(), translucent.begin() + translucentCount,
        [&](const auto& a, const auto& b) { return depth(a.object) > depth(b.object); });
    auto blend = BlendMode::Add;
    for (size_t index = 0; index < translucentCount; ++index) {
        const auto& effect = translucent[index];
        const auto nextBlend = effect.flame ? BlendMode::Add : BlendMode::Normal;
        if (blend != nextBlend) {
            blend = nextBlend;
            common.SetBlendMode(blend);
            common.CommonDrawSetting();
        }
        effect.object->Draw();
    }
    common.SetBlendMode(BlendMode::Add);
    common.CommonDrawSetting();
    for (size_t index = 0; index < sparkCount; ++index) { sparks[index]->Draw(); }
    common.SetBlendMode(oldBlend);
    common.SetDepthDrawMode(oldDepth);
    common.CommonDrawSetting();
}

// 墜落の進み具合に合わせて画面に暗さを加え、敗北画面へ視線をつなぐ。
void PlayerDefeatPresentation::DrawOverlay(const Math::Vector2& min, const Math::Vector2& size) const
{
    if (!IsActive()) { return; }
    auto* draw = ImGui::GetForegroundDrawList();
    const ImVec2 a{ min.x, min.y }, b{ min.x + size.x, min.y + size.y };
    const float dim = Ease((playerDefeat_.elapsed - 0.75f) / 1.35f);
    draw->AddRectFilled(a, b, CombatHud::SurfaceColor(IM_COL32(8, 14, 24, static_cast<int>(dim * 20.0f))));
    const float flash = 1.0f - std::clamp(playerDefeat_.elapsed / 0.13f, 0.0f, 1.0f);
    if (flash > 0.0f) { draw->AddRectFilled(a, b, IM_COL32(255, 190, 135, static_cast<int>(flash * 70.0f))); }
}

// 墜落演出用の描画オブジェクトを解放し、演出の進行も初期化する。
void PlayerDefeatPresentation::Clear()
{
    defeatMotes_ = {};
    defeatFire_ = {};
    defeatFireGlow_ = {};
    defeatFuelStain_.reset();
    defeatDebris_ = {};
    Reset();
}

// 墜落演出がリザルトを開ける時刻まで進んだかを返す。
bool PlayerDefeatPresentation::IsResultReady() const
{
    return playerDefeat_.elapsed >= kDefeatMenuTime;
}

// 表示中の破片を影描画用の一覧へ入れ、登録できた個数を返す。
size_t PlayerDefeatPresentation::CopyShadowCasters(std::array<Object3d*, kDebrisCount>& output) const
{
    size_t count = 0;
    for (const auto& debris : defeatDebris_) {
        if (debris.active && debris.object) {
            output[count++] = debris.object.get();
        }
    }
    return count;
}

#ifdef _DEBUG
bool PlayerDefeatPresentation::HasLandedFuelForProbe() const
{
    return playerDefeat_.fuelStarted >= 0.0f &&
        std::abs(playerDefeat_.fuelOrigin.y - (kDebrisGroundY + 0.026f)) < 0.001f;
}

bool PlayerDefeatPresentation::HasDebrisForProbe(bool active) const
{
    return std::all_of(defeatDebris_.begin(), defeatDebris_.end(), [active](const auto& debris) {
        return debris.object && debris.active == active &&
            (!active || std::isfinite(debris.position.x + debris.position.y + debris.position.z));
    });
}

bool PlayerDefeatPresentation::IsDebrisAboveRoadForProbe() const
{
    return std::all_of(defeatDebris_.begin(), defeatDebris_.end(), [](const auto& debris) {
        return debris.position.y + DebrisBottom(debris.contactVertices, debris.rotation) >= kDebrisGroundY - 0.02f;
    });
}

bool PlayerDefeatPresentation::AreMotesInactiveForProbe() const
{
    return std::all_of(defeatMotes_.begin(), defeatMotes_.end(), [](const auto& mote) {
        return mote.object && mote.age < 0.0f;
    });
}
#endif
