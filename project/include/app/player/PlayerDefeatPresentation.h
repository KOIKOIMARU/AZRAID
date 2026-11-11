#pragma once

#include "app/presentation/GameplayCamera.h"
#include "engine/3d/Object3d.h"

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

class Camera;
class Model;
class Player;

// 失速から破片の接地までを再生する。敵や弾の退場、リザルトへの遷移は呼び出し側が進める。
class PlayerDefeatPresentation {
public:
    static constexpr size_t kDebrisCount = 4;

    struct Models {
        const Model* ship;
        Model* spark;
        Model* smoke;
        Model* fire;
        Model* glow;
    };
    struct Frame {
        GameplayCamera::Pose camera;
        // 爆発したフレームだけ、共通の戦闘エフェクトと効果音を呼び出し側へ通知する。
        std::optional<Math::Vector3> burstPosition;
    };

    void Initialize(Object3dCommon& common, Camera& camera, const Models& models, float shipScale);
    void Reset() { playerDefeat_ = {}; }
    void Clear();
    void Start(const Player& player, const Camera& camera, float fov, float railSpeed,
        const Math::Vector3& incomingVelocity);
    Frame Update(Player& player, float deltaSeconds);

    bool IsActive() const { return playerDefeat_.elapsed >= 0.0f; }
    bool HasExploded() const { return playerDefeat_.exploded; }
    bool IsResultReady() const;
    void DrawDebris() const;
    void DrawEffects(Object3dCommon& common, const Camera& camera, const Player& player);
    void DrawOverlay(const Math::Vector2& min, const Math::Vector2& size) const;
    size_t CopyShadowCasters(std::array<Object3d*, kDebrisCount>& output) const;

#ifdef _DEBUG
    float ElapsedForProbe() const { return playerDefeat_.elapsed; }
    bool IsResetForProbe() const { return !IsActive() && !HasExploded() && playerDefeat_.fuelStarted < 0.0f; }
    bool HasFuelStainForProbe() const { return defeatFuelStain_ != nullptr; }
    bool HasLandedFuelForProbe() const;
    bool HasDebrisForProbe(bool active) const;
    bool IsDebrisAboveRoadForProbe() const;
    bool AreMotesInactiveForProbe() const;
#endif

private:
    void InitializeDebris(Object3dCommon& common, Camera& camera, const Model* ship, float shipScale);
    Math::Vector3 GetDefeatFireOrigin(size_t piece, size_t site) const;
    void EmitDefeatMote(const Math::Vector3& position, bool spark, bool burst, float strength = 1.0f);

    struct PlayerDefeat {
        float elapsed = -1.0f;
        float smokeClock = 0.0f;
        float emberClock = 0.0f;
        float forwardSpeed = 0.0f;
        float side = 1.0f;
        float startFov = 0.8f;
        float burstTime = 1.25f;
        float lateralTravel = 2.0f;
        float roll = 1.5f;
        float pitch = 0.45f;
        float yaw = 0.30f;
        float travelScale = 0.37f;
        float variation = 0.0f;
        float fuelStarted = -1.0f;
        float smolderClock = 0.0f;
        Math::Vector3 fuelOrigin{};
        bool exploded = false;
        unsigned int emission = 0;
        Math::Vector3 origin{}, rotation{}, cameraPosition{}, cameraRotation{};
        Math::Vector3 wreckPosition{};
    } playerDefeat_;
    struct DefeatMote {
        std::unique_ptr<Object3d> object;
        Math::Vector3 position{}, velocity{};
        float age = -1.0f;
        float lifetime = 1.0f;
        float size = 1.0f;
        float roll = 0.0f;
        float strength = 1.0f;
        bool spark = false;
        bool ember = false;
    };
    std::array<DefeatMote, 48> defeatMotes_{}; // 起動時に確保。失速・爆発・残煙で共用する。
    std::array<std::unique_ptr<Object3d>, 12> defeatFire_{}; // 機体内部2、燃料の火8、翼の残り火2。
    std::array<std::unique_ptr<Object3d>, 3> defeatFireGlow_{};
    std::unique_ptr<Object3d> defeatFuelStain_;
    struct DefeatDebris {
        std::unique_ptr<Object3d> object;
        Math::Vector3 localOffset{};
        std::array<Math::Vector3, 8> fireAnchors{}; // 上下面それぞれの実メッシュ上の点。
        std::vector<Math::Vector3> contactVertices; // 接地判定用。生成は初期化時だけ。
        Math::Vector3 position{}, velocity{}, rotation{}, spin{};
        float restitution = 0.22f;
        float drag = 6.0f;
        float restPitch = 0.0f;
        float restRoll = 0.0f;
        bool active = false;
    };
    std::array<DefeatDebris, kDebrisCount> defeatDebris_{}; // 胴体・機首・左右の翼。生成は初期化時だけ。
};
