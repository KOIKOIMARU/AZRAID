#pragma once

#include "engine/3d/Object3d.h"

#include <cstddef>
#include <memory>
#include <vector>

class Camera;
class Model;
class Object3dCommon;

// 背景物体の配置、可視判定、透明部分の描画順を管理する。
// 戦闘やシーンの切り替えには依存しない。
class RailScenery {
public:
    struct Object {
        std::unique_ptr<Object3d> object;
        Math::Vector3 anchor{};
        Math::Vector3 scale{ 1.0f, 1.0f, 1.0f };
        Math::Vector3 rotate{};
        Math::Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        float loopLength = 120.0f;
        float speedMultiplier = 1.0f;
        float lateralDrift = 0.0f;
        float verticalDrift = 0.0f;
        float driftSpeed = 0.02f;
        float rollSpeed = 0.0f;
        float curveInfluence = 0.0f;
        float phase = 0.0f;
        float currentLocalZ = 0.0f;
        float drawFarLocalZ = 300.0f;
        bool isVisible = true;
        bool isInView = true; // 色描画だけのカメラ判定。画面外の施設が落とす影は別に残す。
        bool castsShadow = false; // 不透明な構造物だけを投影し、路面と別モデルの窓は除外する。
        Math::Vector3 boundsMin{};
        Math::Vector3 boundsMax{};
        bool billboard = false;
        bool isBuilding = false; // 街区ごとに幅と高さを変える対象。元のanchor/scaleは保持する。
        bool isRoad = false;
        bool isBackRow = false; // 奥の建物は道路沿いより外側へ置き、空の輪郭を作る。
        bool isLandmark = false; // 一度だけ通過する施設。道路のようにループ再配置しない。
        bool isTrackside = false; // 飛行域の外側に置く低い設備。近景の流れで速度を伝える。
        bool isTerrain = false; // 敷地や道路の隙間から空を見せない連続した不透明地表。
        bool isDefenseDistrict = false; // 中盤の固定間隔の敷地。再配置は視界の外だけで行う。
        bool isBossApron = false; // 終盤の広い航空エプロン。中央の飛行域に構造物を置かない。
        bool isRoadDetail = false; // 車道用の排水溝・マンホール。航空エプロンには持ち込まない。
        float halfDepth = 0.0f; // 大きな施設の前後端まで含めた可視・影の判定範囲。
    };

    void SetDistricts(bool tutorial, float defenseStartZ, float plazaStartZ);
    bool Prepare(Object3dCommon* common, Camera* camera, size_t first, size_t count);
    void Clear(); // 所有する背景を先に破棄する。借りたカメラ・共通描画設定の寿命はGameRuntimeが管理する。
    void Update(float railDistance);
    void Draw(ModelDrawPass drawPass);

    Math::Vector2 DistrictWeights(float worldZ) const;
    float BossApronStartZ() const;
    float DefenseStartZ() const { return defenseStartZ_; }
    float PlazaStartZ() const { return plazaStartZ_; }
    size_t VisibleCount() const { return visibleCount_; }
    // 影の描画と検証で参照する。配置の変更はPrepare/Updateに集約する。
    const std::vector<Object>& Objects() const { return objects_; }

private:
    struct BoundsCache {
        Model* model;
        Math::Vector3 scale, rotate, min, max;
        float halfDepth;
    };

    Object3dCommon* common_ = nullptr;
    Camera* camera_ = nullptr;
    bool tutorial_ = false;
    float defenseStartZ_ = -1.0f;
    float plazaStartZ_ = -1.0f;
    size_t visibleCount_ = 0;
    std::vector<Object> objects_;
    std::vector<const Object*> transparentOrder_;
    std::vector<BoundsCache> boundsCache_;
};
