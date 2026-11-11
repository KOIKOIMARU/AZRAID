// エディタで配置したオブジェクトの読込・保存・描画を担当する。

#pragma once

#include "engine/3d/Object3d.h"
#include "engine/scene/SceneSerializer.h"

#include <memory>
#include <string>
#include <vector>

class RuntimeSceneObjects {
public:
    static constexpr const char* kDefaultPath = "resources/game_scene.json";

    bool Load(const char* path, Object3dCommon& common, SceneSerializer::SceneSettings& settings);
    bool Save(const char* path, const SceneSerializer::SceneSettings& settings);
    void Update();
    void Draw() const;
    void Clear();
    const auto& Objects() const { return objects_; }
    size_t Count() const { return objects_.size(); }
    const std::string& Path() const { return path_; }

private:
    std::vector<SceneSerializer::ObjectRecord> BuildRecords() const;

    std::vector<std::unique_ptr<Object3d>> objects_;
    std::vector<SceneSerializer::ObjectRecord> records_;
    std::string path_{ kDefaultPath };
};
