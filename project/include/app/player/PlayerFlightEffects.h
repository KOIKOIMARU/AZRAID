#pragma once

#include "engine/3d/Object3d.h"
#include <array>
#include <cstddef>
#include <memory>

class Camera;
class Model;
class Object3dCommon;

// 自機の噴射・チャージの光・回避の残像。入力、回避の成否、報酬は扱わない。
class PlayerFlightEffects {
public:
    // モデル、共通描画設定、カメラは借りる。描画物体24個だけをこのクラスが所有する。
    struct Assets {
        Model* glow = nullptr;
        Model* trail = nullptr;
        Model* core = nullptr;
        Model* enginePlume = nullptr;
        Model* chargeArc = nullptr;
        Model* chargeStar = nullptr;
        Model* hull = nullptr;
    };
    struct FlightFrame {
        Math::Vector3 position{}, rotation{};
        float deltaSeconds = 1.0f / 60.0f;
        float clockFrames = 0.0f;
        float speed = 0.0f;
        float acceleration = 0.0f;
        float fever = 0.0f;
        float boost = 0.0f;
        float gateBoost = 0.0f;
        float justDodge = 0.0f;
        float releaseKick = 0.0f;
        float charge = 0.0f;
        bool dodging = false;
        bool feverActive = false;
    };
    struct DodgeFrame {
        Math::Vector3 position{};
        Math::Vector3 visualPosition{}, rotation{};
        float scale = 1.26f;
        float deltaSeconds = 1.0f / 60.0f;
        int direction = 1;
        bool alive = false;
        bool dodging = false;
        bool justDodge = false;
    };

    void Initialize(Object3dCommon* common, Camera* camera, const Assets& assets);
    void PrepareAfterimages(); // 起動時に固定数を準備。プレイ中には確保しない。
    void PrepareExhaust();
    void PrepareCharge();
    void Clear();              // 描画物体を先に破棄し、借りた参照を解除する。
    void Reset();              // 準備済みの物体を保持して、残像と噴射の状態だけを初期化。
    void SetCinematicThrust(float thrust) { thrust_ = thrust; }

    bool UpdateAfterimages(const DodgeFrame& frame); // 回避開始の瞬間だけtrue。音や揺れは呼び出し側へ返す。
    void FlashDodgeTrail(Math::Vector3 position, int direction);
    void FlashJustDodge(const DodgeFrame& frame);
    void UpdateFlight(const FlightFrame& frame);
    void DrawFlight();         // 時間を進めない。ポーズ中も同じ状態を描く。
    void DrawAfterimages();

#ifdef _DEBUG
    struct Snapshot {
        size_t objects = 0, afterimages = 0, plumes = 0, chargeLights = 0;
        float thrust = 0.0f;
        float afterimageAge = 0.0f;
        bool operator==(const Snapshot&) const = default;
    };
    Snapshot Inspect() const;
#endif

private:
    struct Afterimage {
        std::unique_ptr<Object3d> object;
        Math::Vector3 position{};
        Math::Vector3 rotation{};
        float scale = 1.26f;
        bool precise = false;
        int direction = 1;
        float age = 0.0f; // 60fps換算。成功時は姿勢を写した短い機体の残像にする。
        bool active = false;
    };
    struct Plume {
        std::unique_ptr<Object3d> object;
        Model* model = nullptr;
        Math::Vector4 color{};
        float pulseOffset = 0.0f;
        bool visible = false;
    };
    void SpawnAfterimage(Math::Vector3 position, int direction);
    void SpawnHullAfterimage(const DodgeFrame& frame);

    Object3dCommon* common_ = nullptr;
    Camera* camera_ = nullptr;
    Assets assets_{};
    std::array<Afterimage, 16> afterimages_{};
    std::array<Plume, 6> plumes_{};
    std::array<std::unique_ptr<Object3d>, 2> chargeLights_{};
    size_t nextAfterimage_ = 0;
    int afterimageTimer_ = 0;
    bool wasDodging_ = false;
    bool chargeVisible_ = false;
    float thrust_ = 0.0f;
};
