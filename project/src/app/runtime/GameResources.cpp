#include "app/runtime/GameResources.h"
#include "app/stage/SceneryAssets.h"

#include "engine/3d/ModelManager.h"
#include "engine/3d/TextureManager.h"
#include "engine/base/DirectXCommon.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

namespace GameResources {
namespace {
using namespace SceneryAssets;

constexpr float kTwoPi = 6.28318530718f;

// チャージ弾の光に使う細長い形状を、頂点データから生成する。
ModelData CreateChargeLanceMesh()
{
    ModelData data;
    data.material.textureFilePath = "resources/human/white.png";
    data.vertices.reserve(48);
    const auto vertex = [](Math::Vector3 p) {
        return VertexData{{ p.x, p.y, p.z, 1.0f }, { 0.5f, 0.5f }, { 0, 0, -1 }};
    };
    // 丸い通常弾と輪郭を変える。進行方向へ尖った八面の槍を一度だけ作る。
    for (int side = 0; side < 8; ++side) {
        const float a = kTwoPi * static_cast<float>(side) / 8.0f;
        const float b = kTwoPi * static_cast<float>(side + 1) / 8.0f;
        const auto left = vertex({ std::cos(a) * 0.38f, std::sin(a) * 0.38f, -0.10f });
        const auto right = vertex({ std::cos(b) * 0.38f, std::sin(b) * 0.38f, -0.10f });
        data.vertices.insert(data.vertices.end(), {
            vertex({ 0, 0, 1.50f }), right, left,
            vertex({ 0, 0, -1.0f }), left, right });
    }
    return data;
}

// スキルの斬撃を描く刃の形状を、頂点データから生成する。
ModelData CreatePhantomBladeMesh()
{
    ModelData data;
    data.material.textureFilePath = "resources/human/white.png";
    data.vertices.reserve(240);
    const auto point = [](float t, bool inner) {
        const float x = t * 2.0f - 1.0f;
        const float taper = std::pow((std::max)(std::sin(t * 3.14159265f), 0.0f), 0.75f);
        const float y = (1.0f - x * x) * 0.28f - (inner ? 0.24f * taper : 0.0f);
        return VertexData{{ x, y, 0, 1 }, { t, inner ? 1.0f : 0.0f }, { 0, 0, -1 }};
    };
    for (int segment = 0; segment < 40; ++segment) {
        const float t = static_cast<float>(segment) / 40.0f;
        const float next = static_cast<float>(segment + 1) / 40.0f;
        data.vertices.insert(data.vertices.end(), {
            point(t, false), point(next, false), point(t, true),
            point(t, true), point(next, false), point(next, true) });
    }
    return data;
}

// リングの枠・内側の光・流れる光に使う形状を種類ごとに作る。
ModelData CreateGateMesh(int kind)
{
    ModelData data;
    data.material.textureFilePath = "resources/human/white.png";
    data.vertices.reserve(1800);
    const auto point = [](float angle, float radius, float z) {
        return Math::Vector3{ std::cos(angle) * radius, std::sin(angle) * radius, z };
    };
    const auto quad = [&](Math::Vector3 a, Math::Vector3 b, Math::Vector3 c, Math::Vector3 d) {
        const Math::Vector3 u{b.x-a.x,b.y-a.y,b.z-a.z}, v{c.x-a.x,c.y-a.y,c.z-a.z};
        const auto normal = Math::Normalize({u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x});
        const auto vertex = [&](Math::Vector3 p) { return VertexData{{p.x,p.y,p.z,1.0f},{0.5f,0.5f},normal}; };
        data.vertices.insert(data.vertices.end(), {vertex(a),vertex(b),vertex(c),vertex(c),vertex(b),vertex(d)});
    };
    if (kind == 3) {
        // 中央の視界を空け、外周の短い矢印だけを奥へ流す。
        for (int side = 0; side < 4; ++side) {
            const float angle = static_cast<float>(side) * kTwoPi / 4.0f;
            for (int mark = 0; mark < 3; ++mark) {
                const float z = static_cast<float>(mark) * 0.62f;
                for (float sign : {-1.0f,1.0f}) {
                    quad(point(angle+sign*0.10f,0.97f,z),point(angle+sign*0.10f,0.99f,z),
                        point(angle,0.97f,z+0.30f),point(angle,0.99f,z+0.30f));
                }
            }
        }
    } else {
        const int quarters = kind == 2 ? 4 : 1;
        for (int quarter = 0; quarter < quarters; ++quarter) {
            const float offset = static_cast<float>(quarter) * kTwoPi / 4.0f;
            constexpr std::array<Math::Vector2,6> profile{{{1.02f,-0.18f},{1.045f,-0.28f},{1.145f,-0.28f},
                {1.18f,-0.18f},{1.18f,0.28f},{1.02f,0.28f}}};
            for (int segment = 0; segment < 12; ++segment) {
                const float a = offset + (9.0f+static_cast<float>(segment)*6.0f)*kTwoPi/360.0f;
                const float b = a + 6.0f*kTwoPi/360.0f;
                if (kind != 0) {
                    quad(point(a,1.045f,0),point(a,1.065f,0),point(b,1.045f,0),point(b,1.065f,0));
                    continue;
                }
                for (size_t face = 0; face < profile.size(); ++face) {
                    const auto p = profile[face], q = profile[(face+1)%profile.size()];
                    quad(point(a,p.x,p.y),point(a,q.x,q.y),point(b,p.x,p.y),point(b,q.x,q.y));
                }
                if (segment == 0 || segment == 11) {
                    const float end = segment == 0 ? a : b;
                    quad(point(end,1.02f,-0.18f),point(end,1.18f,-0.18f),point(end,1.02f,0.28f),point(end,1.18f,0.28f));
                }
            }
        }
    }
    return data;
}
constexpr int kSharedResourcePreloadStepCount = 86;
int gSharedResourcePreloadStep = 0;
bool gSharedResourcesPreloaded = false;
const char* gSharedResourcePreloadLabel = "Waiting";
float gSharedResourceLastStepMs = 0.0f;
float gSharedResourceTotalMs = 0.0f;
} // namespace

// タイトルと本編で共有する素材を順番に準備する。ロード画面の進捗には、この準備単位を使う。
bool PreloadStep(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    if (gSharedResourcesPreloaded) { return true; }
    if (!dxCommon || !srvManager) { return false; }
    ModelManager::GetInstance()->Initialize(dxCommon, srvManager);
    struct Job { const char* label; bool (*load)(DirectXCommon*, ModelManager*); };
    // タイトルの環境・自機を優先。1フレームに1素材、CPU解析の完了を待たず戻る。
    static const Job jobs[] = {
        { "Renderer resources", [](DirectXCommon*, ModelManager* modelManager) { modelManager->SetEnvironmentTexturePath(kGameEnvironmentTexturePath); return true; } },
        { "Environment texture", [](DirectXCommon*, ModelManager*) { return TextureManager::GetInstance()->PreloadTextureStep(kGameEnvironmentTexturePath); } },
        { "Gameplay shaders", [](DirectXCommon* dxCommon, ModelManager*) { dxCommon->CompileShader(L"shaders/Object3D.VS.hlsl", L"vs_6_0"); return true; } },
        { "Gameplay shaders", [](DirectXCommon* dxCommon, ModelManager*) { dxCommon->CompileShader(L"shaders/Object3D.PS.hlsl", L"ps_6_0"); return true; } },
        { "Gameplay shaders", [](DirectXCommon* dxCommon, ModelManager*) { dxCommon->CompileShader(L"shaders/Object3DShadow.VS.hlsl", L"vs_6_0"); return true; } },
        { "Gameplay shaders", [](DirectXCommon* dxCommon, ModelManager*) { dxCommon->CompileShader(L"shaders/Skybox.VS.hlsl", L"vs_6_0"); return true; } },
        { "Gameplay shaders", [](DirectXCommon* dxCommon, ModelManager*) { dxCommon->CompileShader(L"shaders/Skybox.PS.hlsl", L"ps_6_0"); return true; } },
        { "Title exhaust image", [](DirectXCommon*, ModelManager*) { return TextureManager::GetInstance()->PreloadTextureStep("resources/effects/title_engine_plume.png"); } },
        { "Title exhaust core image", [](DirectXCommon*, ModelManager*) { return TextureManager::GetInstance()->PreloadTextureStep("resources/effects/rail_player_bolt_core.png"); } },
        { "Player exhaust mesh", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreatePlane("effect_player_engine_plume", 1.0f, 1.0f, "resources/effects/title_engine_plume.png"); return true; } },
        { "Projectile effect meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreatePlane("effect_player_bullet_core", 1.0f, 1.0f, "resources/effects/rail_player_bolt_core.png"); return true; } },
        { "Player ship model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kFreePlayerModelPath); } },
        { "Core game meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateBox("game_player", 1.0f, 0.45f, 1.3f, "resources/uvChecker.png"); return true; } },
        { "Core game meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateSphere("game_bullet", 12, 24, 0.35f, "resources/human/white.png"); return true; } },
        { "Core game meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateSphere("game_enemy", 16, 32, 0.9f, "resources/uvChecker.png"); return true; } },
        { "Core game meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreatePlane("primitive_plane", 8.0f, 8.0f, "resources/checkerBoard.png"); return true; } },
        { "Core game meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateTriangle("primitive_triangle", 1.6f, 1.6f, "resources/uvChecker.png"); return true; } },
        { "Core game meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateCircle("primitive_circle", 32, 0.9f, "resources/uvChecker.png"); return true; } },
        { "Reward and glow meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateHeart("reward_heart", 48, 1.0f, "resources/human/white.png"); return true; } },
        { "Reward and glow meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateSphere("reward_energy_shard", 2, 4, 0.5f, "resources/human/white.png"); return true; } },
        { "Reward and glow meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateCircle("effect_glow_core", 48, 1.0f, "resources/effects/glow_core.png"); return true; } },
        { "Reward and glow meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateCircle("effect_glow_ring", 64, 1.0f, "resources/effects/glow_ring.png"); return true; } },
        { "Reward and glow meshes", [](DirectXCommon* dxCommon, ModelManager* modelManager) {
            modelManager->CreateMesh("boost_gate_frame", CreateGateMesh(0));
            modelManager->CreateMesh("boost_gate_light", CreateGateMesh(1));
            modelManager->CreateMesh("boost_gate_rear", CreateGateMesh(2));
            modelManager->CreateMesh("boost_gate_flow", CreateGateMesh(3));
            return true;
        } },
        { "Reward and glow meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateCircle("effect_spark_star", 48, 1.0f, "resources/effects/pal_star_spark.png"); return true; } },
        { "Reward and glow meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateCircle("effect_bullet_glow", 48, 1.0f, "resources/effects/glow_core.png"); return true; } },
        { "Reward and glow meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateCircle("effect_contact_shadow", 64, 1.0f, "resources/effects/glow_core.png"); return true; } },
        { "Projectile effect meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreatePlane("effect_bullet_trail", 1.0f, 1.0f, "resources/effects/bullet_trail.png"); return true; } },
        { "Projectile effect meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreatePlane("effect_player_bullet_trail", 1.0f, 1.0f, "resources/effects/rail_player_bolt_trail.png"); return true; } },
        { "Projectile effect meshes", [](DirectXCommon* dxCommon, ModelManager* modelManager) {
            modelManager->CreatePlane("effect_player_charge_core", 1.0f, 1.0f, "resources/effects/rail_charge_lance_core.png");
            modelManager->CreateMesh("game_charge_lance", CreateChargeLanceMesh());
            return true;
        } },
        { "Projectile effect meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreatePlane("effect_player_charge_trail", 1.0f, 1.0f, "resources/effects/rail_charge_lance_trail.png"); return true; } },
        { "Projectile effect meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreatePlane("effect_enemy_bullet_core", 1.0f, 1.0f, "resources/effects/azraid_enemy_core.png"); return true; } },
        { "Projectile effect meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateRing("effect_combat_shock", 64, 0.5f, 0.492f, "resources/human/white.png"); return true; } },
        { "Projectile effect meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateTriangle("effect_combat_blade", 1.0f, 1.0f, "resources/human/white.png"); return true; } },
        { "Projectile effect meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateCircle("effect_combat_disc", 48, 0.5f, "resources/human/white.png"); return true; } },
        { "Projectile effect meshes", [](DirectXCommon* dxCommon, ModelManager* modelManager) {
            modelManager->CreateArc("effect_combat_arc", 40, 0.5f, 0.038f, 3.2f, "resources/human/white.png");
            modelManager->CreateMesh("phantom_slash_blade", CreatePhantomBladeMesh());
            return true;
        } },
        { "Projectile effect meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreatePlane("effect_enemy_bullet_tail", 1.0f, 1.0f, "resources/effects/azraid_enemy_wake.png"); return true; } },
        { "Projectile effect meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreatePlane("effect_impact_burst", 1.0f, 1.0f, "resources/effects/pal_impact_burst.png"); return true; } },
        { "Projectile effect meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreatePlane("effect_magic_shard", 1.0f, 1.0f, "resources/effects/pal_magic_shard.png"); return true; } },
        { "Projectile effect meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreatePlane("effect_explosion_fireball", 1.0f, 1.0f, "resources/effects/rail_explosion_fireball.png"); return true; } },
        { "Projectile effect meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreatePlane("effect_explosion_smoke", 1.0f, 1.0f, "resources/effects/rail_explosion_smoke.png"); return true; } },
        { "Projectile effect meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreatePlane("effect_explosion_sparks", 1.0f, 1.0f, "resources/effects/rail_explosion_sparks.png"); return true; } },
        { "Primitive scene meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateRing("primitive_ring", 32, 2.0f, 1.0f, "resources/gradationLine.png"); return true; } },
        { "Primitive scene meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateSphere("primitive_sphere", 16, 32, 1.0f, "resources/uvChecker.png"); return true; } },
        { "Primitive scene meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateTorus("primitive_torus", 32, 16, 0.8f, 0.3f, "resources/uvChecker.png"); return true; } },
        { "Primitive scene meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateCylinder("primitive_cylinder", 32, 1.2f, 1.2f, 2.5f, "resources/gradationLine.png"); return true; } },
        { "Primitive scene meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateCone("primitive_cone", 32, 0.8f, 1.6f, "resources/uvChecker.png"); return true; } },
        { "Primitive scene meshes", [](DirectXCommon*, ModelManager* modelManager) { modelManager->CreateBox("primitive_box", 1.5f, 1.5f, 1.5f, "resources/uvChecker.png"); return true; } },
        { "Enemy ship models", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kEnemyFormationModelPath); } },
        { "Enemy ship models", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kEnemySwoopModelPath); } },
        { "Enemy ship models", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kEnemyShooterModelPath); } },
        { "Enemy ship models", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kEnemyHeavyModelPath); } },
        { "Enemy ship models", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kFreeEnemyModelPath); } },
        { "Enemy ship models", [](DirectXCommon*, ModelManager*) { return TextureManager::GetInstance()->PreloadTextureStep(kEnemyFormationTexturePath); } },
        { "Enemy ship models", [](DirectXCommon*, ModelManager*) { return TextureManager::GetInstance()->PreloadTextureStep(kEnemySwoopTexturePath); } },
        { "Enemy ship models", [](DirectXCommon*, ModelManager*) { return TextureManager::GetInstance()->PreloadTextureStep(kEnemyShooterTexturePath); } },
        { "Enemy ship models", [](DirectXCommon*, ModelManager*) { return TextureManager::GetInstance()->PreloadTextureStep(kEnemyHeavyTexturePath); } },
        { "Enemy ship models", [](DirectXCommon*, ModelManager*) { return TextureManager::GetInstance()->PreloadTextureStep(kBossTexturePath); } },
        { "City road model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kCityStreet4LaneModelPath); } },
        { "City street detail models", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kCityManholeCoverModelPath); } },
        { "City street detail models", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kCityDrainModelPath); } },
        { "City building small model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kCityBuildingSmallModelPath); } },
        { "City building medium model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kCityBuildingMediumModelPath); } },
        { "City building large model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kCityBuildingLargeModelPath); } },
        { "Boss spaceship model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kBossModelPath); } },
        { "Transit facility model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kTransitLandmarkModelPath); } },
        { "Trackside model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kTracksideModelPath); } },
        { "Aviation district model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationDistrictModelPath); } },
        { "Aviation district model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationGlazingModelPath); } },
        { "Aviation district model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationFlightwayModelPath); } },
        { "Defense district model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationDefenseModelPath); } },
        { "Defense district model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationDefenseGlazingModelPath); } },
        { "Defense district model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationDefenseFlightwayModelPath); } },
        { "Boss apron model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationBossApronModelPath); } },
        { "Boss apron model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationBossApronGlazingModelPath); } },
        { "Boss apron model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationBossApronSurfaceModelPath); } },
        { "Communications court model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationDefenseRelayModelPath); } },
        { "Communications court model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationDefenseRelayGlazingModelPath); } },
        { "Freight apron model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationBossApronFreightModelPath); } },
        { "Freight apron model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationBossApronFreightGlazingModelPath); } },
        { "Continuous terrain model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationTerrainModelPath); } },
        { "Campus ground model", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationCampusGroundModelPath); } },
        { "Outer support building", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationBuildingSupportModelPath); } },
        { "Outer operations building", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationBuildingOperationsModelPath); } },
        { "Outer maintenance hangar", [](DirectXCommon*, ModelManager* modelManager) { return modelManager->PreloadModelStep(kAviationBuildingHangarModelPath); } },
        { "Wreck fire animation", [](DirectXCommon*, ModelManager*) { return TextureManager::GetInstance()->PreloadTextureStep("resources/effects/wreck_fire_flipbook.png"); } },
        { "Wreck smoke animation", [](DirectXCommon*, ModelManager*) { return TextureManager::GetInstance()->PreloadTextureStep("resources/effects/wreck_smoke_flipbook.png"); } },
    };
    static_assert(std::size(jobs) == kSharedResourcePreloadStepCount);
    const auto& job = jobs[gSharedResourcePreloadStep];
    gSharedResourcePreloadLabel = job.label;
    const auto begin = std::chrono::steady_clock::now();
    dxCommon->BeginTextureUploadBatch();
    const bool ready = job.load(dxCommon, ModelManager::GetInstance());
    dxCommon->EndTextureUploadBatch();
    gSharedResourceLastStepMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - begin).count();
    gSharedResourceTotalMs += gSharedResourceLastStepMs;
    if (ready && ++gSharedResourcePreloadStep == kSharedResourcePreloadStepCount) {
        gSharedResourcesPreloaded = true;
    }
    return gSharedResourcesPreloaded;
}

bool IsPreloaded()
{
    return gSharedResourcesPreloaded;
}

int GetStep()
{
    return std::clamp(
        gSharedResourcePreloadStep,
        0,
        kSharedResourcePreloadStepCount);
}

int GetStepCount()
{
    return kSharedResourcePreloadStepCount;
}

const char* GetLabel()
{
    return gSharedResourcePreloadLabel;
}

float GetLastStepMs()
{
    return gSharedResourceLastStepMs;
}

float GetTotalMs()
{
    return gSharedResourceTotalMs;
}
} // namespace GameResources
