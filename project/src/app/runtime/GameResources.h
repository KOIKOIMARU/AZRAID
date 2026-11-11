#pragma once

class DirectXCommon;
class SrvManager;

namespace GameResources {
inline constexpr const char* kGameEnvironmentTexturePath =
    "resources/skybox/kloofendal_48d_partly_cloudy_puresky_4k_cube.dds";
inline constexpr const char* kFreePlayerModelPath = "free_models/player_candidates/Omen.gltf";
inline constexpr const char* kFreeEnemyModelPath =
    "free_models/quaternius_sci_fi_essentials/Enemy_EyeDrone_Static.gltf";
inline constexpr const char* kEnemyFormationModelPath =
    "free_models/Spitfire-20260707T202640Z-3-001/Spitfire/glTF/Spitfire.gltf";
inline constexpr const char* kEnemyFormationTexturePath =
    "resources/free_models/Spitfire-20260707T202640Z-3-001/Spitfire/Textures/Spitfire_Red.dds";
inline constexpr const char* kEnemySwoopModelPath =
    "free_models/Striker-20260707T202647Z-3-001/Striker/glTF/Striker.gltf";
inline constexpr const char* kEnemySwoopTexturePath =
    "resources/free_models/Striker-20260707T202647Z-3-001/Striker/Textures/Striker_Purple.dds";
inline constexpr const char* kEnemyShooterModelPath =
    "free_models/Pancake-20260707T202659Z-3-001/Pancake/glTF/Pancake.gltf";
inline constexpr const char* kEnemyShooterTexturePath =
    "resources/free_models/Pancake-20260707T202659Z-3-001/Pancake/Textures/Pancake_Orange.dds";
inline constexpr const char* kEnemyHeavyModelPath =
    "free_models/Challenger-20260707T202702Z-3-001/Challenger/glTF/Challenger.gltf";
inline constexpr const char* kEnemyHeavyTexturePath =
    "resources/free_models/Challenger-20260707T202702Z-3-001/Challenger/Textures/Challenger_Blue.dds";
inline constexpr const char* kBossModelPath =
    "free_models/Imperial-20260707T180457Z-3-001/Imperial/glTF/Imperial.gltf";
inline constexpr const char* kBossTexturePath =
    "resources/free_models/Imperial-20260707T180457Z-3-001/Imperial/Textures/Imperial_Purple.dds";

// タイトルと本編で同じ素材を使うため、読み込みの進捗はアプリ全体で共有する。
bool PreloadStep(DirectXCommon* dxCommon, SrvManager* srvManager);
bool IsPreloaded();
int GetStep();
int GetStepCount();
const char* GetLabel();
float GetLastStepMs();
float GetTotalMs();
}
