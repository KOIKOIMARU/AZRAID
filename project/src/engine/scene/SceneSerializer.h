// シーンに置いたオブジェクトのモデル名や姿勢をファイルへ保存し、読み戻す。

#pragma once
#include <string>
#include <vector>
#include "engine/base/Math.h"

namespace SceneSerializer {

struct ObjectRecord {
    std::string name;
    bool primitive = false;
    int modelIndex = -1;
    Math::Vector3 translate{};
    Math::Vector3 rotate{};
    Math::Vector3 scale{ 1.0f, 1.0f, 1.0f };
    Math::Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    float alphaReference = 0.0f;
    int lightingMode = 2;
    std::string textureFilePath;
};

struct SceneSettings {
    bool hasCamera = false;
    Math::Vector3 cameraTranslate{};
    Math::Vector3 cameraRotate{};
    bool hasLighting = false;
    Math::Vector3 lightDirection{ 0.0f, -1.0f, 0.0f };
    float lightIntensity = 1.0f;
    Math::Vector3 pointLightPosition{ 0.0f, 2.0f, 0.0f };
    float pointLightIntensity = 1.0f;
    Math::Vector3 spotLightPosition{ 2.0f, 1.25f, 0.0f };
    Math::Vector3 spotLightDirection{ -1.0f, 1.0f, 0.0f };
    float spotLightIntensity = 4.0f;
};

bool SaveObjects(
    const char* path,
    const std::vector<ObjectRecord>& records);
bool LoadObjects(
    const char* path,
    std::vector<ObjectRecord>& records);
bool SaveScene(
    const char* path,
    const std::vector<ObjectRecord>& records,
    const SceneSettings& settings);
bool LoadScene(
    const char* path,
    std::vector<ObjectRecord>& records,
    SceneSettings& settings);
const ObjectRecord* FindObjectByName(
    const std::vector<ObjectRecord>& records,
    const char* name);

} // namespace SceneSerializer
