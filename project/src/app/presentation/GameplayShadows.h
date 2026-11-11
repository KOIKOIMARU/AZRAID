// 上から見た影用の深度画像と、機体の近くに描く接地影を管理する。

#pragma once

#include "engine/3d/Object3d.h"

#include <array>
#include <list>
#include <memory>
#include <span>
#include <vector>

class Enemy;
class Player;
class RailScenery;

class GameplayShadows {
public:
    struct Scene {
        const std::vector<std::unique_ptr<Object3d>>& objects;
        const RailScenery& scenery;
        const std::list<std::unique_ptr<Enemy>>& enemies;
        Player* player;
        std::span<Object3d* const> wreckage;
        float railDistance;
    };

    void PrepareContacts(Object3dCommon& common, Model* model);
    void Clear();
    void RenderMap(Object3dCommon& common, const Scene& scene) const;
    void DrawContacts(Object3dCommon& common, const Scene& scene, float cameraClock);

private:
    std::array<std::unique_ptr<Object3d>, 32> contacts_{};
};
