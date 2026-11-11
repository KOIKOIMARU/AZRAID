#include "engine/scene/SceneFactory.h"
#include "engine/scene/TitleScene.h"
#include "engine/scene/GameScene.h"
#include "engine/scene/EditorScene.h"

// 画面の種類から対応するシーンを作り、必要な共有データを渡す。
std::unique_ptr<BaseScene> SceneFactory::CreateScene(SceneType sceneType) {
    switch (sceneType) {
    case SceneType::Title:
        return std::make_unique<TitleScene>(TitleScene::Screen::Title, records_);

    case SceneType::StageSelect:
        return std::make_unique<TitleScene>(TitleScene::Screen::StageSelection, records_);

    case SceneType::Departure:
        return std::make_unique<TitleScene>(TitleScene::Screen::StageSelection, records_, true);

    case SceneType::Game:
        return std::make_unique<GameScene>(GameRuntime::PlayMode::Game, records_);

    case SceneType::Tutorial:
        return std::make_unique<GameScene>(GameRuntime::PlayMode::Tutorial, records_);

    case SceneType::Editor:
        return std::make_unique<EditorScene>();

    default:
        return nullptr;
    }
}
