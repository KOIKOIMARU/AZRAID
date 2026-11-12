// シーンの種類から実体を作るための共通インターフェース

#pragma once
#include <memory>
#include "engine/scene/SceneType.h"

class BaseScene;

class AbstractSceneFactory {
public:
    virtual ~AbstractSceneFactory() = default;
    virtual std::unique_ptr<BaseScene> CreateScene(SceneType sceneType) = 0;
};
