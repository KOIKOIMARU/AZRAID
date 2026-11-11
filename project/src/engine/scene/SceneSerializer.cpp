// ===== SceneSerializer.cpp =====
// シーンに配置したオブジェクトの情報をJSONに保存し、読み込み時に復元する。

#include "engine/scene/SceneSerializer.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

namespace {

// 指定キーの3要素配列をXYZとして読む。見つからなければfalseを返す。
bool ExtractJsonVector3(
    const std::string& source,
    const char* key,
    Math::Vector3& out)
{
    const std::regex pattern(
        std::string("\"") + key +
        "\"\\s*:\\s*\\[\\s*([-+0-9.eE]+)\\s*,\\s*([-+0-9.eE]+)\\s*,\\s*([-+0-9.eE]+)\\s*\\]"
    );
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }

    out.x = std::stof(match[1].str());
    out.y = std::stof(match[2].str());
    out.z = std::stof(match[3].str());
    return true;
}

// 指定キーの4要素配列をXYZWとして読む。見つからなければfalseを返す。
bool ExtractJsonVector4(
    const std::string& source,
    const char* key,
    Math::Vector4& out)
{
    const std::regex pattern(
        std::string("\"") + key +
        "\"\\s*:\\s*\\[\\s*([-+0-9.eE]+)\\s*,\\s*([-+0-9.eE]+)\\s*,\\s*([-+0-9.eE]+)\\s*,\\s*([-+0-9.eE]+)\\s*\\]"
    );
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }

    out.x = std::stof(match[1].str());
    out.y = std::stof(match[2].str());
    out.z = std::stof(match[3].str());
    out.w = std::stof(match[4].str());
    return true;
}

// 指定キーから小数を読み取る。見つからなければ出力値を保つ。
bool ExtractJsonFloat(const std::string& source, const char* key, float& out)
{
    const std::regex pattern(
        std::string("\"") + key + "\"\\s*:\\s*([-+0-9.eE]+)"
    );
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }

    out = std::stof(match[1].str());
    return true;
}

// 指定キーから真偽値を読み取る。見つからなければ出力値を保つ。
bool ExtractJsonBool(const std::string& source, const char* key, bool& out)
{
    const std::regex pattern(
        std::string("\"") + key + "\"\\s*:\\s*(true|false)"
    );
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }

    out = match[1].str() == "true";
    return true;
}

// 指定キーから整数を読み取る。見つからなければ出力値を保つ。
bool ExtractJsonInt(const std::string& source, const char* key, int& out)
{
    const std::regex pattern(
        std::string("\"") + key + "\"\\s*:\\s*(-?[0-9]+)"
    );
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }

    out = std::stoi(match[1].str());
    return true;
}

// 指定キーの引用符で囲まれた文字列を読み取る。
bool ExtractJsonString(
    const std::string& source,
    const char* key,
    std::string& out)
{
    const std::regex pattern(
        std::string("\"") + key + "\"\\s*:\\s*\"([^\"]*)\""
    );
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }

    out = match[1].str();
    return true;
}

// 指定キーのオブジェクトから、波括弧の内側を設定の読取用に取り出す。
bool ExtractJsonBlock(
    const std::string& source,
    const char* key,
    std::string& out)
{
    const std::regex pattern(
        std::string("\"") + key + "\"\\s*:\\s*\\{([^{}]*)\\}"
    );
    std::smatch match;
    if (!std::regex_search(source, match, pattern)) {
        return false;
    }

    out = match[1].str();
    return true;
}

} // namespace

bool SceneSerializer::SaveObjects(
    const char* path,
    const std::vector<ObjectRecord>& records)
{
    SceneSettings settings{};
    return SaveScene(path, records, settings);
}

// 配置された物体のモデル名、位置、回転、拡大率などを書き出す。
bool SceneSerializer::SaveScene(
    const char* path,
    const std::vector<ObjectRecord>& records,
    const SceneSettings& settings)
{
    if (!path) {
        return false;
    }

    std::filesystem::path filePath(path);
    if (filePath.has_parent_path()) {
        std::filesystem::create_directories(filePath.parent_path());
    }

    std::ofstream file(filePath);
    if (!file) {
        return false;
    }

    file << std::fixed << std::setprecision(6);
    file << "{\n";
    if (settings.hasCamera) {
        file << "  \"camera\": {\n";
        file << "    \"translate\": [" << settings.cameraTranslate.x << ", "
            << settings.cameraTranslate.y << ", "
            << settings.cameraTranslate.z << "],\n";
        file << "    \"rotate\": [" << settings.cameraRotate.x << ", "
            << settings.cameraRotate.y << ", "
            << settings.cameraRotate.z << "]\n";
        file << "  },\n";
    }
    if (settings.hasLighting) {
        file << "  \"lighting\": {\n";
        file << "    \"direction\": [" << settings.lightDirection.x << ", "
            << settings.lightDirection.y << ", "
            << settings.lightDirection.z << "],\n";
        file << "    \"intensity\": " << settings.lightIntensity << ",\n";
        file << "    \"pointPosition\": [" << settings.pointLightPosition.x << ", "
            << settings.pointLightPosition.y << ", "
            << settings.pointLightPosition.z << "],\n";
        file << "    \"pointIntensity\": " << settings.pointLightIntensity << ",\n";
        file << "    \"spotPosition\": [" << settings.spotLightPosition.x << ", "
            << settings.spotLightPosition.y << ", "
            << settings.spotLightPosition.z << "],\n";
        file << "    \"spotDirection\": [" << settings.spotLightDirection.x << ", "
            << settings.spotLightDirection.y << ", "
            << settings.spotLightDirection.z << "],\n";
        file << "    \"spotIntensity\": " << settings.spotLightIntensity << "\n";
        file << "  },\n";
    }
    file << "  \"objects\": [\n";
    for (size_t index = 0; index < records.size(); ++index) {
        const ObjectRecord& record = records[index];

        file << "    {\n";
        file << "      \"name\": \"" << record.name << "\",\n";
        file << "      \"primitive\": "
            << (record.primitive ? "true" : "false") << ",\n";
        file << "      \"modelIndex\": " << record.modelIndex << ",\n";
        file << "      \"translate\": [" << record.translate.x << ", "
            << record.translate.y << ", " << record.translate.z << "],\n";
        file << "      \"rotate\": [" << record.rotate.x << ", "
            << record.rotate.y << ", " << record.rotate.z << "],\n";
        file << "      \"scale\": [" << record.scale.x << ", "
            << record.scale.y << ", " << record.scale.z << "],\n";
        file << "      \"color\": [" << record.color.x << ", "
            << record.color.y << ", " << record.color.z << ", "
            << record.color.w << "],\n";
        file << "      \"alphaReference\": " << record.alphaReference << ",\n";
        file << "      \"lightingMode\": " << record.lightingMode << ",\n";
        file << "      \"texture\": \"" << record.textureFilePath << "\"\n";
        file << "    }" << (index + 1 < records.size() ? "," : "") << "\n";
    }
    file << "  ]\n";
    file << "}\n";

    return true;
}

bool SceneSerializer::LoadObjects(
    const char* path,
    std::vector<ObjectRecord>& records)
{
    SceneSettings settings{};
    return LoadScene(path, records, settings);
}

// 保存した配置データを読み、ゲームやエディタで使えるレコードに戻す。
bool SceneSerializer::LoadScene(
    const char* path,
    std::vector<ObjectRecord>& records,
    SceneSettings& settings)
{
    records.clear();
    settings = SceneSettings{};

    if (!path) {
        return false;
    }

    std::ifstream file(path);
    if (!file) {
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string json = buffer.str();

    std::string cameraJson;
    if (ExtractJsonBlock(json, "camera", cameraJson)) {
        settings.hasCamera =
            ExtractJsonVector3(cameraJson, "translate", settings.cameraTranslate) &&
            ExtractJsonVector3(cameraJson, "rotate", settings.cameraRotate);
    }

    std::string lightingJson;
    if (ExtractJsonBlock(json, "lighting", lightingJson)) {
        bool loadedLighting = true;
        loadedLighting &= ExtractJsonVector3(
            lightingJson,
            "direction",
            settings.lightDirection);
        loadedLighting &= ExtractJsonFloat(
            lightingJson,
            "intensity",
            settings.lightIntensity);
        loadedLighting &= ExtractJsonVector3(
            lightingJson,
            "pointPosition",
            settings.pointLightPosition);
        loadedLighting &= ExtractJsonFloat(
            lightingJson,
            "pointIntensity",
            settings.pointLightIntensity);
        loadedLighting &= ExtractJsonVector3(
            lightingJson,
            "spotPosition",
            settings.spotLightPosition);
        loadedLighting &= ExtractJsonVector3(
            lightingJson,
            "spotDirection",
            settings.spotLightDirection);
        loadedLighting &= ExtractJsonFloat(
            lightingJson,
            "spotIntensity",
            settings.spotLightIntensity);
        settings.hasLighting = loadedLighting;
    }

    const std::regex objectPattern("\\{[^{}]*\"name\"[^{}]*\\}");
    const auto begin =
        std::sregex_iterator(json.begin(), json.end(), objectPattern);
    const auto end = std::sregex_iterator();
    for (auto iterator = begin; iterator != end; ++iterator) {
        const std::string objectJson = iterator->str();
        ObjectRecord record{};

        if (!ExtractJsonString(objectJson, "name", record.name) ||
            !ExtractJsonVector3(objectJson, "translate", record.translate) ||
            !ExtractJsonVector3(objectJson, "rotate", record.rotate) ||
            !ExtractJsonVector3(objectJson, "scale", record.scale)) {
            continue;
        }

        ExtractJsonBool(objectJson, "primitive", record.primitive);
        ExtractJsonInt(objectJson, "modelIndex", record.modelIndex);
        ExtractJsonVector4(objectJson, "color", record.color);
        ExtractJsonFloat(objectJson, "alphaReference", record.alphaReference);
        ExtractJsonInt(objectJson, "lightingMode", record.lightingMode);
        ExtractJsonString(objectJson, "texture", record.textureFilePath);
        records.push_back(record);
    }

    return !records.empty() || settings.hasCamera || settings.hasLighting;
}

// 名前が一致する最初の配置データを探し、見つからなければnullptrを返す。
const SceneSerializer::ObjectRecord* SceneSerializer::FindObjectByName(
    const std::vector<ObjectRecord>& records,
    const char* name)
{
    if (!name) {
        return nullptr;
    }

    for (const ObjectRecord& record : records) {
        if (record.name == name) {
            return &record;
        }
    }

    return nullptr;
}
