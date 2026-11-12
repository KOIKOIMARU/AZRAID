#pragma once
#include "engine/scene/AbstractSceneFactory.h"

class StageRecords;

class SceneFactory : public AbstractSceneFactory {
public:
    explicit SceneFactory(StageRecords* records = nullptr) : records_(records) {}
    ~SceneFactory() override = default;
    std::unique_ptr<BaseScene> CreateScene(SceneType sceneType) override;

private:
    StageRecords* records_; // MyGameが所有　シーン生成時に同じ記録を渡す
};
