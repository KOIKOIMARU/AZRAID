// シーンごとに必要なリソースを段階的に準備する　進捗と完了状態はこのクラスから取得する

#pragma once

#include <cstddef>
#include <memory>
#include <string>

class Camera;
class ClearResultPresentation;
class CombatEffects;
class DirectXCommon;
class FlightSpeedEffects;
class GameplayAudio;
class GameplayCamera;
class GameplayShadows;
class Model;
class Object3dCommon;
class Player;
class PlayerControl;
class PlayerDefeatPresentation;
class PlayerFlightEffects;
class PlayerSkill;
class PlayerSkillPresentation;
class ProjectileSystem;
class RailScenery;
class RingGates;
class RuntimeSceneObjects;
class Skybox;
class SrvManager;
class StageEnemies;

// ステージの準備を複数フレームに分ける　準備先の各クラスは呼び出し中だけ借りる
class GameplaySceneSetup {
public:
    struct Targets {
        DirectXCommon* dx;
        SrvManager* srv;
        std::unique_ptr<Object3dCommon>& common;
        std::unique_ptr<Camera>& camera;
        std::unique_ptr<Skybox>& skybox;
        std::unique_ptr<Player>& player;
        GameplayCamera& cameraMotion;
        StageEnemies& enemies;
        PlayerControl& controls;
        ProjectileSystem& projectiles;
        CombatEffects& combatEffects;
        RailScenery& scenery;
        FlightSpeedEffects& speedEffects;
        RuntimeSceneObjects& sceneObjects;
        PlayerFlightEffects& playerEffects;
        GameplayShadows& shadows;
        PlayerSkill& skill;
        PlayerSkillPresentation& skillPresentation;
        RingGates& rings;
        ClearResultPresentation& clearPresentation;
        PlayerDefeatPresentation& defeatPresentation;
        GameplayAudio& audio;
        std::string& sceneObjectStatus;
    };
    struct Timing {
        const char* label = "None";
        float workMs = 0.0f;
    };

    void Reset();
    bool Advance(const Targets& targets, bool tutorial, float railDistance, float enemyBulletSpeed);
    bool IsReady() const { return step_ == Step::Complete; }
    float Progress(const ProjectileSystem& projectiles, const CombatEffects& effects, const GameplayAudio& audio) const;
    void Release(const Targets& targets);
    static bool LoadSceneObjects(const Targets& targets, const char* path, bool tutorial);
    static void ResetTiming();
    static Timing GetTiming();

#ifdef _DEBUG
    Model* PlayerModelForProbe() const { return playerModel_; }
#endif

private:
    enum class Step {
        Renderer, Bullets, CombatEffects, Scenery, DepthCues, SceneObjects, SceneReady,
        Afterimages, Exhaust, ChargeLights, ContactShadows, Skill, Rings, Results, Audio, FinalSetup, Complete
    };

    bool PrepareRenderer(const Targets& targets, bool tutorial, float railDistance);
    Step step_ = Step::Renderer;
    std::size_t sceneryItem_ = 0;
    Model* playerModel_ = nullptr; // ModelManagerが保持するモデル
};
