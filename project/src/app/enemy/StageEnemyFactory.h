// 機種や入場経路を指定して敵を作る　機体モデルとテクスチャの選択もここで行う

#pragma once

#include "app/enemy/EnemyTypes.h"
#include "engine/base/Math.h"

#include <memory>

class Enemy;
class Model;
class ModelManager;
class Object3dCommon;

class StageEnemyFactory {
public:
    void Prepare(ModelManager& models);
    // 機種に対応するモデルとテクスチャを選び、指定位置・HP・スケールで敵を初期化する　モデルがなければnullptrを返す
    std::unique_ptr<Enemy> Create(Object3dCommon& common, const Math::Vector3& position,
        EnemyBehavior behavior, EnemyEntryStyle entryStyle, int maxHp, float scale) const;

#ifdef _DEBUG
    Model* ShooterModelForProbe() const { return shooter_.model; }
#endif

private:
    struct Appearance {
        Model* model = nullptr;
        const char* texture = nullptr;
    };
    Appearance SelectAppearance(EnemyBehavior behavior, int maxHp) const;

    Appearance formation_;
    Appearance swoop_;
    Appearance shooter_;
    Appearance heavy_;
    Appearance boss_;
    Appearance fallback_;
};
