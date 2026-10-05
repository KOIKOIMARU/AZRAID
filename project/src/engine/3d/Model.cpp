#include "engine/3d/Model.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <cassert>
#include <filesystem>
#include <functional>
#include <numbers>

#include <assimp/Importer.hpp>
#include <assimp/GltfMaterial.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "engine/3d/ModelCommon.h"
#include "engine/base/DirectXCommon.h"
#include "engine/3d/TextureManager.h"
#include "engine/base/SrvManager.h"

namespace {

constexpr const char* kDefaultTextureFilePath = "resources/uvChecker.png";
constexpr const char* kDefaultNormalTextureFilePath =
    "resources/default_normal.png";

Matrix4x4 ConvertAiMatrixToMatrix4x4(aiMatrix4x4 matrix)
{
    matrix.Transpose();

    Matrix4x4 result{};
    result.m[0][0] = matrix.a1;
    result.m[0][1] = matrix.a2;
    result.m[0][2] = matrix.a3;
    result.m[0][3] = matrix.a4;
    result.m[1][0] = matrix.b1;
    result.m[1][1] = matrix.b2;
    result.m[1][2] = matrix.b3;
    result.m[1][3] = matrix.b4;
    result.m[2][0] = matrix.c1;
    result.m[2][1] = matrix.c2;
    result.m[2][2] = matrix.c3;
    result.m[2][3] = matrix.c4;
    result.m[3][0] = matrix.d1;
    result.m[3][1] = matrix.d2;
    result.m[3][2] = matrix.d3;
    result.m[3][3] = matrix.d4;
    return result;
}

Vector4 TransformPosition(const Vector4& position, const Matrix4x4& matrix)
{
    return {
        position.x * matrix.m[0][0] +
            position.y * matrix.m[1][0] +
            position.z * matrix.m[2][0] +
            position.w * matrix.m[3][0],
        position.x * matrix.m[0][1] +
            position.y * matrix.m[1][1] +
            position.z * matrix.m[2][1] +
            position.w * matrix.m[3][1],
        position.x * matrix.m[0][2] +
            position.y * matrix.m[1][2] +
            position.z * matrix.m[2][2] +
            position.w * matrix.m[3][2],
        position.x * matrix.m[0][3] +
            position.y * matrix.m[1][3] +
            position.z * matrix.m[2][3] +
            position.w * matrix.m[3][3],
    };
}

Vector3 TransformNormal(const Vector3& normal, const Matrix4x4& matrix)
{
    Vector4 transformed =
        TransformPosition({ normal.x, normal.y, normal.z, 0.0f }, matrix);
    return Normalize({ transformed.x, transformed.y, transformed.z });
}

VertexData MakeDefaultVertexData()
{
    return {
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0, 0, 0, 0 }
    };
}

void GenerateFallbackObjTexcoords(std::vector<VertexData>& vertices)
{
    if (vertices.empty()) {
        return;
    }

    Vector4 minPosition = vertices.front().position;
    Vector4 maxPosition = vertices.front().position;
    for (const VertexData& vertex : vertices) {
        minPosition.x = (std::min)(minPosition.x, vertex.position.x);
        minPosition.y = (std::min)(minPosition.y, vertex.position.y);
        minPosition.z = (std::min)(minPosition.z, vertex.position.z);
        maxPosition.x = (std::max)(maxPosition.x, vertex.position.x);
        maxPosition.y = (std::max)(maxPosition.y, vertex.position.y);
        maxPosition.z = (std::max)(maxPosition.z, vertex.position.z);
    }

    const float width = (std::max)(maxPosition.x - minPosition.x, 0.001f);
    const float height = (std::max)(maxPosition.y - minPosition.y, 0.001f);
    const float depth = (std::max)(maxPosition.z - minPosition.z, 0.001f);

    for (VertexData& vertex : vertices) {
        const float normalX = std::abs(vertex.normal.x);
        const float normalY = std::abs(vertex.normal.y);
        const float normalZ = std::abs(vertex.normal.z);

        if (normalY > normalX && normalY > normalZ) {
            vertex.texcoord = {
                (vertex.position.x - minPosition.x) / width,
                (vertex.position.z - minPosition.z) / depth
            };
        } else if (normalX > normalZ) {
            vertex.texcoord = {
                (vertex.position.z - minPosition.z) / depth,
                1.0f - (vertex.position.y - minPosition.y) / height
            };
        } else {
            vertex.texcoord = {
                (vertex.position.x - minPosition.x) / width,
                1.0f - (vertex.position.y - minPosition.y) / height
            };
        }
    }
}

void SetVertexInfluence(
    VertexData& vertex,
    uint32_t jointIndex,
    float weight)
{
    for (uint32_t influenceIndex = 0; influenceIndex < kNumMaxInfluence; ++influenceIndex) {
        if (vertex.weight.x == 0.0f && influenceIndex == 0) {
            vertex.weight.x = weight;
            vertex.jointIndices[0] = jointIndex;
            return;
        }
        if (vertex.weight.y == 0.0f && influenceIndex == 1) {
            vertex.weight.y = weight;
            vertex.jointIndices[1] = jointIndex;
            return;
        }
        if (vertex.weight.z == 0.0f && influenceIndex == 2) {
            vertex.weight.z = weight;
            vertex.jointIndices[2] = jointIndex;
            return;
        }
        if (vertex.weight.w == 0.0f && influenceIndex == 3) {
            vertex.weight.w = weight;
            vertex.jointIndices[3] = jointIndex;
            return;
        }
    }

    float* weights[] = {
        &vertex.weight.x,
        &vertex.weight.y,
        &vertex.weight.z,
        &vertex.weight.w,
    };

    uint32_t minIndex = 0;
    for (uint32_t i = 1; i < kNumMaxInfluence; ++i) {
        if (*weights[i] < *weights[minIndex]) {
            minIndex = i;
        }
    }

    if (weight > *weights[minIndex]) {
        *weights[minIndex] = weight;
        vertex.jointIndices[minIndex] = jointIndex;
    }
}

void NormalizeVertexInfluence(VertexData& vertex)
{
    const float totalWeight =
        vertex.weight.x + vertex.weight.y + vertex.weight.z + vertex.weight.w;
    if (totalWeight <= 0.0f) {
        vertex.weight.x = 1.0f;
        vertex.jointIndices[0] = 0;
        return;
    }

    const float inverseTotalWeight = 1.0f / totalWeight;
    vertex.weight.x *= inverseTotalWeight;
    vertex.weight.y *= inverseTotalWeight;
    vertex.weight.z *= inverseTotalWeight;
    vertex.weight.w *= inverseTotalWeight;
}

void CollectNodeIndices(
    const Node& node,
    std::map<std::string, uint32_t>& nodeIndexMap,
    uint32_t& nextIndex)
{
    nodeIndexMap[node.name] = nextIndex++;
    for (const Node& child : node.children) {
        CollectNodeIndices(child, nodeIndexMap, nextIndex);
    }
}

Node ReadNode(aiNode* node)
{
    assert(node);

    aiVector3D scale;
    aiQuaternion rotate;
    aiVector3D translate;
    node->mTransformation.Decompose(scale, rotate, translate);

    Node result;
    result.name = node->mName.C_Str();
    result.transform.scale = { scale.x, scale.y, scale.z };
    result.transform.rotate = { rotate.x, rotate.y, rotate.z, rotate.w };
    result.transform.translate = { translate.x, translate.y, translate.z };
    result.localMatrix = MakeAffineMatrix(
        result.transform.scale,
        result.transform.rotate,
        result.transform.translate
    );

    result.children.resize(node->mNumChildren);
    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
    }

    return result;
}

Animation ReadAnimation(const aiScene* scene)
{
    Animation animation;
    if (!scene || scene->mNumAnimations == 0) {
        return animation;
    }

    const aiAnimation* aiAnimation = scene->mAnimations[0];
    assert(aiAnimation);

    animation.duration = static_cast<float>(aiAnimation->mDuration);
    animation.ticksPerSecond =
        aiAnimation->mTicksPerSecond == 0.0 ?
        1.0f :
        static_cast<float>(aiAnimation->mTicksPerSecond);

    for (uint32_t channelIndex = 0; channelIndex < aiAnimation->mNumChannels; ++channelIndex) {
        const aiNodeAnim* nodeAnimation = aiAnimation->mChannels[channelIndex];
        NodeAnimation animationChannel;

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimation->mNumPositionKeys; ++keyIndex) {
            const auto& key = nodeAnimation->mPositionKeys[keyIndex];
            animationChannel.translate.push_back({
                static_cast<float>(key.mTime),
                { key.mValue.x, key.mValue.y, key.mValue.z }
            });
        }

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimation->mNumRotationKeys; ++keyIndex) {
            const auto& key = nodeAnimation->mRotationKeys[keyIndex];
            animationChannel.rotate.push_back({
                static_cast<float>(key.mTime),
                { key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w }
            });
        }

        for (uint32_t keyIndex = 0; keyIndex < nodeAnimation->mNumScalingKeys; ++keyIndex) {
            const auto& key = nodeAnimation->mScalingKeys[keyIndex];
            animationChannel.scale.push_back({
                static_cast<float>(key.mTime),
                { key.mValue.x, key.mValue.y, key.mValue.z }
            });
        }

        animation.nodeAnimations[nodeAnimation->mNodeName.C_Str()] =
            animationChannel;
    }

    return animation;
}

std::string ResolveAssimpTexturePath(
    const aiString& texturePath,
    const std::string& directoryPath)
{
    const auto preferPreconvertedDds =
        [](const std::filesystem::path& sourcePath) {
            std::filesystem::path ddsPath = sourcePath;
            ddsPath.replace_extension(".dds");
            return std::filesystem::exists(ddsPath) ? ddsPath : sourcePath;
        };

    const std::string texturePathString = texturePath.C_Str();
    if (texturePathString.empty() || texturePathString[0] == '*') {
        return kDefaultTextureFilePath;
    }

    const std::filesystem::path textureRelativePath = texturePathString;
    if (textureRelativePath.is_absolute()) {
        if (std::filesystem::exists(textureRelativePath)) {
            return preferPreconvertedDds(textureRelativePath).generic_string();
        }

        return kDefaultTextureFilePath;
    }

    const std::filesystem::path directory = directoryPath;
    const std::filesystem::path resolvedPath = directory / textureRelativePath;
    if (std::filesystem::exists(resolvedPath)) {
        return preferPreconvertedDds(resolvedPath).generic_string();
    }

    const std::filesystem::path legacyPath = directory / textureRelativePath.filename();
    if (std::filesystem::exists(legacyPath)) {
        return preferPreconvertedDds(legacyPath).generic_string();
    }

    return kDefaultTextureFilePath;
}

std::string ResolveAssimpFallbackTexture(
    const std::string& directoryPath,
    const char* textureFilename)
{
    const std::filesystem::path candidate =
        std::filesystem::path(directoryPath) / textureFilename;
    if (std::filesystem::exists(candidate)) {
        return candidate.generic_string();
    }

    return kDefaultTextureFilePath;
}

std::string GetAssimpMaterialTexturePath(
    const aiMaterial* material,
    const std::string& directoryPath)
{
    if (!material) {
        return kDefaultTextureFilePath;
    }

    aiString texturePath;
    if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &texturePath) ==
            AI_SUCCESS ||
        material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) ==
            AI_SUCCESS) {
        return ResolveAssimpTexturePath(texturePath, directoryPath);
    }

    aiString materialNameString;
    if (material->Get(AI_MATKEY_NAME, materialNameString) == AI_SUCCESS) {
        const std::string materialName = materialNameString.C_Str();
        // ガラスへ室内画像を貼ると、実際の室内面と二重になる。ガラスは材質色で描く。
        if (materialName.find("FakeInterior") != std::string::npos ||
            materialName.find("Interior") != std::string::npos) {
            return ResolveAssimpFallbackTexture(directoryPath, "T_lit_interior_1.png");
        }
    }

    return kDefaultTextureFilePath;
}

std::string GetAssimpNormalTexturePath(
    const aiMaterial* material,
    const std::string& directoryPath)
{
    if (!material) {
        return kDefaultNormalTextureFilePath;
    }

    aiString texturePath;
    if (material->GetTexture(aiTextureType_NORMALS, 0, &texturePath) ==
            AI_SUCCESS ||
        material->GetTexture(aiTextureType_HEIGHT, 0, &texturePath) ==
            AI_SUCCESS) {
        const std::string resolvedPath =
            ResolveAssimpTexturePath(texturePath, directoryPath);
        return resolvedPath == kDefaultTextureFilePath ?
            kDefaultNormalTextureFilePath :
            resolvedPath;
    }

    return kDefaultNormalTextureFilePath;
}

bool TryGetAssimpMaterialFloat(
    const aiMaterial* material,
    const char* key,
    unsigned int type,
    unsigned int index,
    float& value)
{
    if (!material) {
        return false;
    }

    return material->Get(key, type, index, value) == AI_SUCCESS;
}

MaterialData GetAssimpMaterialData(
    const aiMaterial* material,
    const std::string& directoryPath)
{
    MaterialData materialData;
    materialData.textureFilePath =
        GetAssimpMaterialTexturePath(material, directoryPath);
    if (material) {
        aiString alphaMode;
        materialData.alphaBlend =
            material->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaMode) == AI_SUCCESS &&
            std::string(alphaMode.C_Str()) == "BLEND";
        aiColor4D baseColor(1.0f, 1.0f, 1.0f, 1.0f);
        if (material->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS ||
            material->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor) == AI_SUCCESS) {
            materialData.baseColor = { baseColor.r, baseColor.g, baseColor.b, baseColor.a };
        }
        // テクスチャを持たない塗装面にUVチェッカーを掛けず、材質色をそのまま表示する。
        // 参照先が壊れているテクスチャは従来どおりチェッカーで見つけられるようにする。
        if (material->GetTextureCount(aiTextureType_BASE_COLOR) == 0 &&
            material->GetTextureCount(aiTextureType_DIFFUSE) == 0 &&
            materialData.textureFilePath == kDefaultTextureFilePath) {
            materialData.textureFilePath = "resources/human/white.png";
        }
    }
    materialData.normalTextureFilePath =
        GetAssimpNormalTexturePath(material, directoryPath);
    materialData.normalStrength =
        materialData.normalTextureFilePath == kDefaultNormalTextureFilePath ?
        0.0f :
        0.50f;

    float roughness = materialData.roughness;
    if (TryGetAssimpMaterialFloat(
            material,
            AI_MATKEY_ROUGHNESS_FACTOR,
            roughness)) {
        materialData.roughness = std::clamp(roughness, 0.04f, 1.0f);
    } else {
        float shininess = 0.0f;
        if (TryGetAssimpMaterialFloat(
                material,
                AI_MATKEY_SHININESS,
                shininess)) {
            const float convertedRoughness =
                std::sqrt(2.0f / ((std::max)(shininess, 1.0f) + 2.0f));
            materialData.roughness =
                std::clamp(convertedRoughness, 0.08f, 0.95f);
        }
    }

    float metallic = materialData.metallic;
    if (TryGetAssimpMaterialFloat(
            material,
            AI_MATKEY_METALLIC_FACTOR,
            metallic)) {
        materialData.metallic = std::clamp(metallic, 0.0f, 1.0f);
    }

    return materialData;
}

void EnsureModelDrawData(ModelData& modelData)
{
    if (modelData.material.textureFilePath.empty()) {
        modelData.material.textureFilePath = kDefaultTextureFilePath;
    }
    if (modelData.material.normalTextureFilePath.empty()) {
        modelData.material.normalTextureFilePath = kDefaultNormalTextureFilePath;
    }

    if (modelData.materials.empty()) {
        modelData.materials.push_back(modelData.material);
    }
    for (MaterialData& material : modelData.materials) {
        if (material.textureFilePath.empty()) {
            material.textureFilePath = kDefaultTextureFilePath;
        }
        if (material.normalTextureFilePath.empty()) {
            material.normalTextureFilePath = kDefaultNormalTextureFilePath;
        }
        material.roughness = std::clamp(material.roughness, 0.04f, 1.0f);
        material.metallic = std::clamp(material.metallic, 0.0f, 1.0f);
        material.normalStrength =
            material.normalTextureFilePath == kDefaultNormalTextureFilePath ?
            0.0f :
            std::clamp(material.normalStrength, 0.0f, 1.0f);
    }

    modelData.material = modelData.materials.front();

    if (modelData.drawRanges.empty() && !modelData.indices.empty()) {
        modelData.drawRanges.push_back({
            0,
            static_cast<uint32_t>(modelData.indices.size()),
            0
        });
    }

    for (ModelDrawRange& drawRange : modelData.drawRanges) {
        if (drawRange.materialIndex >= modelData.materials.size()) {
            drawRange.materialIndex = 0;
        }
    }
}

void AppendAssimpNode(
    const aiScene* scene,
    const aiNode* node,
    const Matrix4x4& parentMatrix,
    ModelData& modelData,
    const std::map<std::string, uint32_t>& nodeIndexMap)
{
    Matrix4x4 localMatrix =
        ConvertAiMatrixToMatrix4x4(node->mTransformation);
    Matrix4x4 worldMatrix = Multiply(localMatrix, parentMatrix);

    for (uint32_t meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[meshIndex]];
        assert(mesh);
        assert(mesh->HasNormals());

        const uint32_t vertexOffset =
            static_cast<uint32_t>(modelData.vertices.size());
        const uint32_t indexOffset =
            static_cast<uint32_t>(modelData.indices.size());
        std::vector<VertexData> originalVertices(mesh->mNumVertices);
        for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
            VertexData vertex = MakeDefaultVertexData();

            const aiVector3D& position = mesh->mVertices[vertexIndex];
            const aiVector3D& normal = mesh->mNormals[vertexIndex];

            if (mesh->HasBones()) {
                vertex.position = { position.x, position.y, position.z, 1.0f };
                vertex.normal = Normalize({ normal.x, normal.y, normal.z });
            } else {
                vertex.position = TransformPosition(
                    { position.x, position.y, position.z, 1.0f },
                    worldMatrix);
                vertex.normal = TransformNormal(
                    { normal.x, normal.y, normal.z },
                    worldMatrix);
            }

            if (mesh->HasTextureCoords(0)) {
                const aiVector3D& uv = mesh->mTextureCoords[0][vertexIndex];
                vertex.texcoord = { uv.x, uv.y };
            }

            originalVertices[vertexIndex] = vertex;
        }

        if (mesh->HasBones()) {
            for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
                const aiBone* bone = mesh->mBones[boneIndex];
                assert(bone);

                const std::string jointName = bone->mName.C_Str();
                auto jointIndexIt = nodeIndexMap.find(jointName);
                if (jointIndexIt == nodeIndexMap.end()) {
                    continue;
                }

                modelData.skinClusterData.jointWeights[jointName].inverseBindPoseMatrix =
                    ConvertAiMatrixToMatrix4x4(bone->mOffsetMatrix);

                for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
                    const aiVertexWeight& vertexWeight = bone->mWeights[weightIndex];
                    SetVertexInfluence(
                        originalVertices[vertexWeight.mVertexId],
                        jointIndexIt->second,
                        vertexWeight.mWeight
                    );
                }
            }

            for (VertexData& vertex : originalVertices) {
                NormalizeVertexInfluence(vertex);
            }
        }

        modelData.vertices.insert(
            modelData.vertices.end(),
            originalVertices.begin(),
            originalVertices.end()
        );

        for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
            const aiFace& face = mesh->mFaces[faceIndex];
            assert(face.mNumIndices == 3);

            for (uint32_t index = 0; index < face.mNumIndices; ++index) {
                uint32_t vertexIndex = face.mIndices[index];
                modelData.indices.push_back(vertexOffset + vertexIndex);
            }
        }

        const uint32_t indexCount =
            static_cast<uint32_t>(modelData.indices.size()) - indexOffset;
        if (indexCount > 0) {
            modelData.drawRanges.push_back({
                indexOffset,
                indexCount,
                mesh->mMaterialIndex
            });
        }
    }

    for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        AppendAssimpNode(
            scene,
            node->mChildren[childIndex],
            worldMatrix,
            modelData,
            nodeIndexMap);
    }
}

void AppendTriangle(
    ModelData& modelData,
    const VertexData& v0,
    const VertexData& v1,
    const VertexData& v2)
{
    const uint32_t startIndex =
        static_cast<uint32_t>(modelData.vertices.size());
    modelData.vertices.push_back(v0);
    modelData.vertices.push_back(v1);
    modelData.vertices.push_back(v2);
    modelData.indices.push_back(startIndex + 0);
    modelData.indices.push_back(startIndex + 1);
    modelData.indices.push_back(startIndex + 2);
}

void AppendQuad(
    ModelData& modelData,
    const VertexData& v0,
    const VertexData& v1,
    const VertexData& v2,
    const VertexData& v3)
{
    AppendTriangle(modelData, v0, v1, v2);
    AppendTriangle(modelData, v2, v1, v3);
}

}

void Model::Initialize(ModelCommon* modelCommon,
    const std::string& directoryPath,
    const std::string& filename)
{
    assert(modelCommon);
    modelCommon_ = modelCommon;

    std::filesystem::path requestedPath = filename;
    std::filesystem::path assetDirectoryPath = directoryPath;
    if (!requestedPath.parent_path().empty()) {
        assetDirectoryPath /= requestedPath.parent_path();
    }
    const std::string assetDirectory = assetDirectoryPath.string();
    const std::string assetFilename = requestedPath.filename().string();
    const std::string extension = requestedPath.extension().string();

    if (extension == ".gltf" || extension == ".glb") {
        modelData_ = LoadAssimpFile(assetDirectory, assetFilename);
    } else {
        modelData_ = LoadObjFile(assetDirectory, assetFilename);
    }

    if (modelData_.indices.empty()) {
        modelData_.indices.resize(modelData_.vertices.size());
        for (uint32_t index = 0; index < modelData_.indices.size(); ++index) {
            modelData_.indices[index] = index;
        }
    }
    EnsureModelDrawData(modelData_);

    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateMaterial();

    for (const MaterialData& material : modelData_.materials) {
        TextureManager::GetInstance()->LoadTexture(material.textureFilePath);
        TextureManager::GetInstance()->LoadTexture(
            material.normalTextureFilePath,
            false);
    }
}

void Model::Initialize(ModelCommon* modelCommon, const ModelData& modelData)
{
    assert(modelCommon);
    modelCommon_ = modelCommon;
    modelData_ = modelData;

    if (modelData_.indices.empty()) {
        modelData_.indices.resize(modelData_.vertices.size());
        for (uint32_t index = 0; index < modelData_.indices.size(); ++index) {
            modelData_.indices[index] = index;
        }
    }
    EnsureModelDrawData(modelData_);

    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateMaterial();

    for (const MaterialData& material : modelData_.materials) {
        TextureManager::GetInstance()->LoadTexture(material.textureFilePath);
        TextureManager::GetInstance()->LoadTexture(
            material.normalTextureFilePath,
            false);
    }
}

void Model::Draw(
    const D3D12_VERTEX_BUFFER_VIEW* overrideVertexBufferView,
    ID3D12Resource* overrideMaterialResource,
    const std::string* overrideTextureFilePath,
    ModelDrawPass drawPass)
{
    auto dxCommon = modelCommon_->GetDxCommon();
    auto commandList = dxCommon->GetCommandList();
    auto srvManager = modelCommon_->GetSrvManager();

    // VB
    const D3D12_VERTEX_BUFFER_VIEW* vertexBufferView =
        overrideVertexBufferView ? overrideVertexBufferView : &vertexBufferView_;
    commandList->IASetVertexBuffers(0, 1, vertexBufferView);
    commandList->IASetIndexBuffer(&indexBufferView_);

    // Material CBV
    ID3D12Resource* materialResource =
        overrideMaterialResource ? overrideMaterialResource : materialResource_.Get();
    commandList->SetGraphicsRootConstantBufferView(
        0,
        materialResource->GetGPUVirtualAddress()
    );

    srvManager->SetGraphicsRootDescriptorTable(
        4,
        TextureManager::GetInstance()->GetSrvIndex(
            modelCommon_->GetEnvironmentTexturePath()
        )
    );

    for (const ModelDrawRange& drawRange : modelData_.drawRanges) {
        if (drawRange.indexCount == 0) {
            continue;
        }

        const uint32_t materialIndex =
            drawRange.materialIndex < modelData_.materials.size() ?
            drawRange.materialIndex :
            0;
        const bool alphaBlend = modelData_.materials[materialIndex].alphaBlend;
        if ((drawPass == ModelDrawPass::Opaque && alphaBlend) ||
            (drawPass == ModelDrawPass::Transparent && !alphaBlend)) {
            continue;
        }
        const std::string& textureFilePath =
            overrideTextureFilePath ? *overrideTextureFilePath :
            modelData_.materials[materialIndex].textureFilePath;
        const std::string& normalTextureFilePath =
            modelData_.materials[materialIndex].normalTextureFilePath;

        // Root Constantsは描画コマンドへ値をコピーするので、共有CBの上書きや追加確保が不要。
        commandList->SetGraphicsRoot32BitConstants(
            kModelMaterialColorRootParameter, 4,
            &modelData_.materials[materialIndex].baseColor, 0);

        srvManager->SetGraphicsRootDescriptorTable(
            3,
            TextureManager::GetInstance()->GetSrvIndex(textureFilePath)
        );
        srvManager->SetGraphicsRootDescriptorTable(
            10,
            TextureManager::GetInstance()->GetSrvIndex(normalTextureFilePath)
        );

        commandList->DrawIndexedInstanced(
            static_cast<UINT>(drawRange.indexCount),
            1,
            drawRange.indexOffset,
            0,
            0
        );
    }
}

bool Model::HasTransparentMaterials() const
{
    return std::any_of(modelData_.materials.begin(), modelData_.materials.end(),
        [](const MaterialData& material) { return material.alphaBlend; });
}

void Model::CreateVertexBuffer()
{
    auto dxCommon = modelCommon_->GetDxCommon();
    const auto& vertices = modelData_.vertices;

    vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * vertices.size());

    VertexData* vertexData = nullptr;
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
    memcpy(vertexData, vertices.data(), sizeof(VertexData) * vertices.size());
    vertexResource_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * vertices.size());
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Model::CreateIndexBuffer()
{
    auto dxCommon = modelCommon_->GetDxCommon();
    const auto& indices = modelData_.indices;

    indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * indices.size());

    uint32_t* mappedIndex = nullptr;
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndex));
    memcpy(mappedIndex, indices.data(), sizeof(uint32_t) * indices.size());
    indexResource_->Unmap(0, nullptr);

    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = UINT(sizeof(uint32_t) * indices.size());
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
}
void Model::CreateMaterial()
{
    auto dxCommon = modelCommon_->GetDxCommon();

    materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

    materialData_->color = { 1,1,1,1 };
    materialData_->lightingMode = 2; // Half Lambert
    materialData_->shininess = 64.0f;
    materialData_->environmentCoefficient = 0.2f;
    materialData_->alphaReference = 0.0f;
    materialData_->specularColor = { 1.0f, 1.0f, 1.0f };
    materialData_->roughness =
        std::clamp(modelData_.material.roughness, 0.04f, 1.0f);
    materialData_->metallic =
        std::clamp(modelData_.material.metallic, 0.0f, 1.0f);
    materialData_->normalStrength =
        std::clamp(modelData_.material.normalStrength, 0.0f, 1.0f);
    materialData_->uvTransform = MakeIdentity4x4();
}

void Model::SetEnvironmentCoefficient(float coefficient)
{
    materialData_->environmentCoefficient = coefficient;
}

float Model::GetEnvironmentCoefficient() const
{
    return materialData_->environmentCoefficient;
}

void Model::SetColor(const Vector4& color)
{
    materialData_->color = color;
}

Vector4 Model::GetColor() const
{
    return materialData_->color;
}

void Model::SetAlphaReference(float alphaReference)
{
    materialData_->alphaReference = alphaReference;
}

float Model::GetAlphaReference() const
{
    return materialData_->alphaReference;
}

void Model::SetUVTransform(const Matrix4x4& uvTransform)
{
    materialData_->uvTransform = uvTransform;
}

void Model::SetLightingMode(int32_t lightingMode)
{
    materialData_->lightingMode = lightingMode;
}

int32_t Model::GetLightingMode() const
{
    return materialData_->lightingMode;
}

void Model::SetTextureFilePath(const std::string& textureFilePath)
{
    modelData_.material.textureFilePath = textureFilePath;
    if (modelData_.materials.empty()) {
        modelData_.materials.push_back(modelData_.material);
    }
    for (MaterialData& material : modelData_.materials) {
        material.textureFilePath = textureFilePath;
    }
    TextureManager::GetInstance()->LoadTexture(textureFilePath);
}

const std::string& Model::GetTextureFilePath() const
{
    return modelData_.material.textureFilePath;
}

MaterialData Model::LoadMaterialTemplate(const std::string& directoryPath, const std::string& filename)
{
    MaterialData materialData;
    materialData.textureFilePath = kDefaultTextureFilePath;

    std::ifstream file(directoryPath + "/" + filename);
    assert(file.is_open());

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream s(line);
        std::string identifier;
        s >> identifier;

        if (identifier == "map_Kd") {
            std::string textureFilename;
            s >> textureFilename;
            materialData.textureFilePath = directoryPath + "/" + textureFilename;
        }
    }

    return materialData;
}

ModelData Model::LoadObjFile(const std::string& directoryPath, const std::string& filename)
{
    ModelData modelData;

    std::vector<Vector4> positions;
    std::vector<Vector2> texcoords;
    std::vector<Vector3> normals;

    std::ifstream file(directoryPath + "/" + filename);
    assert(file.is_open());

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream s(line);
        std::string id;
        s >> id;

        if (id == "v") {
            Vector4 p{};
            s >> p.x >> p.y >> p.z;
            p.w = 1.0f;

            // 鬯ｮ・ｯ繝ｻ・ｷ郢晢ｽｻ繝ｻ・ｿ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｳ鬯ｮ・ｫ繝ｻ・ｰ驛｢譎｢・ｽ・ｻ髫ｶ蜴・ｽｽ・ｿ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｻ鬯ｮ・ｯ隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｾ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｦ鬯ｮ・ｫ繝ｻ・ｰ驛｢譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｼ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｻ鬯ｮ・ｯ繝ｻ・ｷ郢晢ｽｻ繝ｻ・ｿ鬮ｯ譎｢・｣・ｰ鬮ｮ諛ｶ・ｽ・｣郢晢ｽｻ繝ｻ・ｽ郢晢ｽｻ繝ｻ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・｢鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｻ鬩幢ｽ｢隴趣ｽ｢繝ｻ・ｽ繝ｻ・ｻ
            p.z *= -1.0f;
            positions.push_back(p);

        } else if (id == "vt") {
            Vector2 uv{};
            s >> uv.x >> uv.y;
            uv.y = 1.0f - uv.y; // V鬯ｮ・ｯ繝ｻ・ｷ郢晢ｽｻ繝ｻ・ｿ鬮ｯ譎｢・｣・ｰ鬮ｮ諛ｶ・ｽ・｣郢晢ｽｻ繝ｻ・ｽ郢晢ｽｻ繝ｻ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・｢
            texcoords.push_back(uv);

        } else if (id == "vn") {
            Vector3 n{};
            s >> n.x >> n.y >> n.z;
            n.z *= -1.0f; // 鬯ｮ・ｮ闕ｳ・ｻ繝ｻ・｢髦ｮ蜷ｶ繝ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・ｷ鬮ｯ諛ｶ・｣・ｰ髫ｴ蟇よ桶隨翫ｋ蜉代・・ｰ鬮ｮ諛ｶ・ｽ・｣郢晢ｽｻ繝ｻ・ｽ郢晢ｽｻ繝ｻ・ｻ驛｢譎｢・ｽ・ｻ郢晢ｽｻ繝ｻ・｢
            normals.push_back(n);

        } else if (id == "f") {
            std::vector<VertexData> faceVertices;
            std::string vStr;

            while (s >> vStr) {
                int idxV = 0, idxT = 0, idxN = 0;
                std::istringstream vs(vStr);
                std::string token;

                if (std::getline(vs, token, '/') && !token.empty()) idxV = std::stoi(token);
                if (std::getline(vs, token, '/') && !token.empty()) idxT = std::stoi(token);
                if (std::getline(vs, token, '/') && !token.empty()) idxN = std::stoi(token);

                auto resolveObjIndex = [](int index, size_t count) -> int {
                    if (index > 0) {
                        return index - 1;
                    }
                    if (index < 0) {
                        return static_cast<int>(count) + index;
                    }
                    return -1;
                };

                const int positionIndex = resolveObjIndex(idxV, positions.size());
                const int texcoordIndex = resolveObjIndex(idxT, texcoords.size());
                const int normalIndex = resolveObjIndex(idxN, normals.size());

                Vector4 pos{ 0, 0, 0, 1 };
                if (positionIndex >= 0 &&
                    positionIndex < static_cast<int>(positions.size())) {
                    pos = positions[static_cast<size_t>(positionIndex)];
                }

                Vector2 uv{ 0.0f, 0.0f };
                if (texcoordIndex >= 0 &&
                    texcoordIndex < static_cast<int>(texcoords.size())) {
                    uv = texcoords[static_cast<size_t>(texcoordIndex)];
                }

                Vector3 nor{ 0.0f, 1.0f, 0.0f };
                if (normalIndex >= 0 &&
                    normalIndex < static_cast<int>(normals.size())) {
                    nor = normals[static_cast<size_t>(normalIndex)];
                }

                faceVertices.push_back({ pos, uv, nor });
            }

            for (size_t i = 1; i + 1 < faceVertices.size(); ++i) {
                modelData.vertices.push_back(faceVertices[0]);
                modelData.vertices.push_back(faceVertices[i]);
                modelData.vertices.push_back(faceVertices[i + 1]);
            }

        } else if (id == "mtllib") {
            std::string mtl;
            s >> mtl;
            modelData.material = LoadMaterialTemplate(directoryPath, mtl);
        }
    }

    if (texcoords.empty()) {
        GenerateFallbackObjTexcoords(modelData.vertices);
    }

    if (modelData.material.textureFilePath.empty()) {
        modelData.material.textureFilePath = kDefaultTextureFilePath;
    }

    return modelData;
}

ModelData Model::LoadAssimpFile(
    const std::string& directoryPath,
    const std::string& filename)
{
    ModelData modelData;

    Assimp::Importer importer;
    std::string filePath = directoryPath + "/" + filename;

    const unsigned int flags =
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenNormals;

    const aiScene* scene = importer.ReadFile(filePath.c_str(), flags);
    assert(scene);
    assert(scene->HasMeshes());
    assert(scene->mRootNode);

    modelData.materials.reserve(scene->mNumMaterials);
    for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
        modelData.materials.push_back(
            GetAssimpMaterialData(scene->mMaterials[materialIndex], directoryPath));
    }
    if (modelData.materials.empty()) {
        modelData.materials.push_back({ kDefaultTextureFilePath });
    }
    modelData.material = modelData.materials.front();
    modelData.rootNode = ReadNode(scene->mRootNode);
    modelData.animation = ReadAnimation(scene);

    std::map<std::string, uint32_t> nodeIndexMap;
    uint32_t nextNodeIndex = 0;
    CollectNodeIndices(modelData.rootNode, nodeIndexMap, nextNodeIndex);

    AppendAssimpNode(
        scene,
        scene->mRootNode,
        MakeIdentity4x4(),
        modelData,
        nodeIndexMap);

    EnsureModelDrawData(modelData);

    return modelData;
}

QuaternionTransform Model::CalculateValue(
    const NodeAnimation& nodeAnimation,
    float time)
{
    QuaternionTransform result{
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f }
    };

    auto calculateVector3Value =
        [time](const std::vector<Keyframe<Vector3>>& keyframes, const Vector3& defaultValue) {
            if (keyframes.empty()) {
                return defaultValue;
            }
            if (keyframes.size() == 1 || time <= keyframes.front().time) {
                return keyframes.front().value;
            }

            for (size_t index = 0; index + 1 < keyframes.size(); ++index) {
                const auto& current = keyframes[index];
                const auto& next = keyframes[index + 1];
                if (time <= next.time) {
                    const float duration = next.time - current.time;
                    const float t =
                        duration == 0.0f ? 0.0f : (time - current.time) / duration;
                    return Vector3{
                        current.value.x + (next.value.x - current.value.x) * t,
                        current.value.y + (next.value.y - current.value.y) * t,
                        current.value.z + (next.value.z - current.value.z) * t
                    };
                }
            }

            return keyframes.back().value;
        };

    auto calculateQuaternionValue =
        [time](const std::vector<Keyframe<Quaternion>>& keyframes, const Quaternion& defaultValue) {
            if (keyframes.empty()) {
                return defaultValue;
            }
            if (keyframes.size() == 1 || time <= keyframes.front().time) {
                return NormalizeQuaternion(keyframes.front().value);
            }

            for (size_t index = 0; index + 1 < keyframes.size(); ++index) {
                const auto& current = keyframes[index];
                const auto& next = keyframes[index + 1];
                if (time <= next.time) {
                    const float duration = next.time - current.time;
                    const float t =
                        duration == 0.0f ? 0.0f : (time - current.time) / duration;
                    return NormalizeQuaternion(Slerp(current.value, next.value, t));
                }
            }

            return NormalizeQuaternion(keyframes.back().value);
        };

    result.translate = calculateVector3Value(
        nodeAnimation.translate,
        result.translate
    );
    result.rotate = calculateQuaternionValue(
        nodeAnimation.rotate,
        result.rotate
    );
    result.scale = calculateVector3Value(
        nodeAnimation.scale,
        result.scale
    );

    return result;
}

Skeleton Model::CreateSkeleton(const Node& rootNode)
{
    Skeleton skeleton;

    std::function<int32_t(const Node&, std::optional<int32_t>)> createJoint =
        [&](const Node& node, std::optional<int32_t> parent) -> int32_t {
        const int32_t jointIndex = static_cast<int32_t>(skeleton.joints.size());
        Joint joint{};
        joint.name = node.name;
        joint.transform = node.transform;
        joint.bindPoseTransform = node.transform;
        joint.localMatrix = node.localMatrix;
        joint.skeletonSpaceMatrix = MakeIdentity4x4();
        joint.parent = parent;
        joint.index = jointIndex;

        skeleton.joints.push_back(joint);
        skeleton.jointMap[node.name] = jointIndex;

        for (const Node& child : node.children) {
            const int32_t childIndex = createJoint(child, jointIndex);
            skeleton.joints[jointIndex].children.push_back(childIndex);
        }

        return jointIndex;
    };

    skeleton.root = createJoint(rootNode, std::nullopt);
    return skeleton;
}

void Model::ApplyAnimation(
    Skeleton& skeleton,
    const Animation& animation,
    float time)
{
    for (Joint& joint : skeleton.joints) {
        joint.transform = joint.bindPoseTransform;
    }

    for (const auto& [jointName, nodeAnimation] : animation.nodeAnimations) {
        auto jointIt = skeleton.jointMap.find(jointName);
        if (jointIt == skeleton.jointMap.end()) {
            continue;
        }

        skeleton.joints[jointIt->second].transform =
            CalculateValue(nodeAnimation, time);
    }
}

void Model::UpdateSkeleton(Skeleton& skeleton)
{
    for (Joint& joint : skeleton.joints) {
        joint.localMatrix = MakeAffineMatrix(
            joint.transform.scale,
            joint.transform.rotate,
            joint.transform.translate
        );

        if (joint.parent.has_value()) {
            joint.skeletonSpaceMatrix = Multiply(
                joint.localMatrix,
                skeleton.joints[*joint.parent].skeletonSpaceMatrix
            );
        } else {
            joint.skeletonSpaceMatrix = joint.localMatrix;
        }
    }
}

ModelData Model::CreatePlaneData(
    float width,
    float height,
    const std::string& textureFilePath)
{
    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;
    const Vector3 normal = { 0.0f, 0.0f, -1.0f };

    modelData.vertices = {
        { { -halfWidth, -halfHeight, 0.0f, 1.0f }, { 0.0f, 1.0f }, normal },
        { { -halfWidth,  halfHeight, 0.0f, 1.0f }, { 0.0f, 0.0f }, normal },
        { {  halfWidth, -halfHeight, 0.0f, 1.0f }, { 1.0f, 1.0f }, normal },
        { {  halfWidth, -halfHeight, 0.0f, 1.0f }, { 1.0f, 1.0f }, normal },
        { { -halfWidth,  halfHeight, 0.0f, 1.0f }, { 0.0f, 0.0f }, normal },
        { {  halfWidth,  halfHeight, 0.0f, 1.0f }, { 1.0f, 0.0f }, normal },
    };

    return modelData;
}

ModelData Model::CreateTriangleData(
    float width,
    float height,
    const std::string& textureFilePath)
{
    assert(width > 0.0f);
    assert(height > 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;
    const Vector3 normal = { 0.0f, 0.0f, -1.0f };

    AppendTriangle(
        modelData,
        { { 0.0f, halfHeight, 0.0f, 1.0f }, { 0.5f, 0.0f }, normal },
        { { -halfWidth, -halfHeight, 0.0f, 1.0f }, { 0.0f, 1.0f }, normal },
        { { halfWidth, -halfHeight, 0.0f, 1.0f }, { 1.0f, 1.0f }, normal }
    );

    return modelData;
}

ModelData Model::CreateCircleData(
    uint32_t divideCount,
    float radius,
    const std::string& textureFilePath)
{
    assert(divideCount >= 3);
    assert(radius > 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float radianPerDivide =
        2.0f * std::numbers::pi_v<float> / static_cast<float>(divideCount);
    const Vector3 normal = { 0.0f, 0.0f, -1.0f };
    const VertexData center = {
        { 0.0f, 0.0f, 0.0f, 1.0f },
        { 0.5f, 0.5f },
        normal
    };

    for (uint32_t index = 0; index < divideCount; ++index) {
        const float angle = static_cast<float>(index) * radianPerDivide;
        const float nextAngle =
            static_cast<float>(index + 1) * radianPerDivide;

        const float sinValue = std::sin(angle);
        const float cosValue = std::cos(angle);
        const float sinNext = std::sin(nextAngle);
        const float cosNext = std::cos(nextAngle);

        const VertexData current = {
            { -sinValue * radius, cosValue * radius, 0.0f, 1.0f },
            { 0.5f - sinValue * 0.5f, 0.5f - cosValue * 0.5f },
            normal
        };
        const VertexData next = {
            { -sinNext * radius, cosNext * radius, 0.0f, 1.0f },
            { 0.5f - sinNext * 0.5f, 0.5f - cosNext * 0.5f },
            normal
        };

        AppendTriangle(modelData, center, current, next);
    }

    return modelData;
}

ModelData Model::CreateHeartData(
    uint32_t divideCount,
    float radius,
    const std::string& textureFilePath)
{
    assert(divideCount >= 12);
    assert(radius > 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const Vector3 normal = { 0.0f, 0.0f, -1.0f };
    const VertexData center = {
        { 0.0f, 0.08f * radius, 0.0f, 1.0f },
        { 0.5f, 0.5f },
        normal
    };

    auto makeHeartVertex = [&](uint32_t index) {
        const float t =
            2.0f * std::numbers::pi_v<float> *
            static_cast<float>(index) / static_cast<float>(divideCount);
        const float sinT = std::sin(t);
        const float cosT = std::cos(t);
        const float x = 16.0f * sinT * sinT * sinT;
        const float y =
            13.0f * cosT -
            5.0f * std::cos(2.0f * t) -
            2.0f * std::cos(3.0f * t) -
            std::cos(4.0f * t);
        const float scaledX = x / 18.0f * radius;
        const float scaledY = (y - 2.0f) / 18.0f * radius;
        return VertexData{
            { scaledX, scaledY, 0.0f, 1.0f },
            { 0.5f + scaledX / (radius * 2.0f), 0.5f - scaledY / (radius * 2.0f) },
            normal
        };
    };

    for (uint32_t index = 0; index < divideCount; ++index) {
        AppendTriangle(
            modelData,
            center,
            makeHeartVertex(index),
            makeHeartVertex((index + 1) % divideCount));
    }

    return modelData;
}

ModelData Model::CreateRingData(
    uint32_t divideCount,
    float outerRadius,
    float innerRadius,
    const std::string& textureFilePath)
{
    assert(divideCount >= 3);
    assert(outerRadius > innerRadius);
    assert(innerRadius >= 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float radianPerDivide =
        2.0f * std::numbers::pi_v<float> / static_cast<float>(divideCount);
    const Vector3 normal = { 0.0f, 0.0f, -1.0f };

    for (uint32_t index = 0; index < divideCount; ++index) {
        const float angle = static_cast<float>(index) * radianPerDivide;
        const float nextAngle =
            static_cast<float>(index + 1) * radianPerDivide;

        const float sinValue = std::sin(angle);
        const float cosValue = std::cos(angle);
        const float sinNext = std::sin(nextAngle);
        const float cosNext = std::cos(nextAngle);

        const float u =
            static_cast<float>(index) / static_cast<float>(divideCount);
        const float uNext =
            static_cast<float>(index + 1) / static_cast<float>(divideCount);

        const VertexData outerCurrent = {
            { -sinValue * outerRadius, cosValue * outerRadius, 0.0f, 1.0f },
            { u, 0.0f },
            normal
        };
        const VertexData outerNext = {
            { -sinNext * outerRadius, cosNext * outerRadius, 0.0f, 1.0f },
            { uNext, 0.0f },
            normal
        };
        const VertexData innerCurrent = {
            { -sinValue * innerRadius, cosValue * innerRadius, 0.0f, 1.0f },
            { u, 1.0f },
            normal
        };
        const VertexData innerNext = {
            { -sinNext * innerRadius, cosNext * innerRadius, 0.0f, 1.0f },
            { uNext, 1.0f },
            normal
        };

        modelData.vertices.push_back(outerCurrent);
        modelData.vertices.push_back(outerNext);
        modelData.vertices.push_back(innerCurrent);
        modelData.vertices.push_back(innerCurrent);
        modelData.vertices.push_back(outerNext);
        modelData.vertices.push_back(innerNext);
    }

    return modelData;
}

ModelData Model::CreateArcData(uint32_t divideCount, float outerRadius,
    float width, float arcRadians, const std::string& textureFilePath)
{
    assert(divideCount >= 3 && outerRadius > width && width > 0.0f);
    assert(arcRadians > 0.0f && arcRadians <= 2.0f * std::numbers::pi_v<float>);
    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;
    modelData.vertices.reserve(static_cast<size_t>(divideCount) * 6);
    const auto vertex = [&](float t, bool inner) {
        const float angle = (t - 0.5f) * arcRadians;
        // 両端を尖らせた弧。輪の一部を切るだけの一定幅にはしない。
        const float taper = std::sin(t * std::numbers::pi_v<float>);
        const float radius = outerRadius - (inner ? width * taper * taper : 0.0f);
        return VertexData{ { -std::sin(angle) * radius, std::cos(angle) * radius, 0, 1 },
            { t, inner ? 1.0f : 0.0f }, { 0, 0, -1 } };
    };
    for (uint32_t index = 0; index < divideCount; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(divideCount);
        const float next = static_cast<float>(index + 1) / static_cast<float>(divideCount);
        AppendTriangle(modelData, vertex(t, false), vertex(next, false), vertex(t, true));
        AppendTriangle(modelData, vertex(t, true), vertex(next, false), vertex(next, true));
    }
    return modelData;
}

ModelData Model::CreateSphereData(
    uint32_t latDivideCount,
    uint32_t lonDivideCount,
    float radius,
    const std::string& textureFilePath)
{
    assert(latDivideCount >= 2);
    assert(lonDivideCount >= 3);
    assert(radius > 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float pi = std::numbers::pi_v<float>;

    for (uint32_t latIndex = 0; latIndex < latDivideCount; ++latIndex) {
        const float v0 =
            static_cast<float>(latIndex) / static_cast<float>(latDivideCount);
        const float v1 =
            static_cast<float>(latIndex + 1) / static_cast<float>(latDivideCount);
        const float theta0 = v0 * pi;
        const float theta1 = v1 * pi;

        for (uint32_t lonIndex = 0; lonIndex < lonDivideCount; ++lonIndex) {
            const float u0 =
                static_cast<float>(lonIndex) / static_cast<float>(lonDivideCount);
            const float u1 =
                static_cast<float>(lonIndex + 1) / static_cast<float>(lonDivideCount);
            const float phi0 = u0 * 2.0f * pi;
            const float phi1 = u1 * 2.0f * pi;

            auto makeVertex = [&](float theta, float phi, float u, float v) {
                const float sinTheta = std::sin(theta);
                const float cosTheta = std::cos(theta);
                const float sinPhi = std::sin(phi);
                const float cosPhi = std::cos(phi);

                const Vector3 normal = {
                    -sinPhi * sinTheta,
                    cosTheta,
                    cosPhi * sinTheta
                };

                return VertexData{
                    {
                        normal.x * radius,
                        normal.y * radius,
                        normal.z * radius,
                        1.0f
                    },
                    { u, v },
                    Normalize(normal)
                };
            };

            const VertexData v00 = makeVertex(theta0, phi0, u0, v0);
            const VertexData v01 = makeVertex(theta0, phi1, u1, v0);
            const VertexData v10 = makeVertex(theta1, phi0, u0, v1);
            const VertexData v11 = makeVertex(theta1, phi1, u1, v1);

            AppendQuad(modelData, v00, v01, v10, v11);
        }
    }

    return modelData;
}

ModelData Model::CreateTorusData(
    uint32_t majorDivideCount,
    uint32_t minorDivideCount,
    float majorRadius,
    float minorRadius,
    const std::string& textureFilePath)
{
    assert(majorDivideCount >= 3);
    assert(minorDivideCount >= 3);
    assert(majorRadius > 0.0f);
    assert(minorRadius > 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float pi = std::numbers::pi_v<float>;

    for (uint32_t majorIndex = 0; majorIndex < majorDivideCount; ++majorIndex) {
        const float u0 =
            static_cast<float>(majorIndex) / static_cast<float>(majorDivideCount);
        const float u1 =
            static_cast<float>(majorIndex + 1) / static_cast<float>(majorDivideCount);
        const float theta0 = u0 * 2.0f * pi;
        const float theta1 = u1 * 2.0f * pi;

        for (uint32_t minorIndex = 0; minorIndex < minorDivideCount; ++minorIndex) {
            const float v0 =
                static_cast<float>(minorIndex) / static_cast<float>(minorDivideCount);
            const float v1 =
                static_cast<float>(minorIndex + 1) / static_cast<float>(minorDivideCount);
            const float phi0 = v0 * 2.0f * pi;
            const float phi1 = v1 * 2.0f * pi;

            auto makeVertex = [&](float theta, float phi, float u, float v) {
                const float sinTheta = std::sin(theta);
                const float cosTheta = std::cos(theta);
                const float sinPhi = std::sin(phi);
                const float cosPhi = std::cos(phi);

                const float ringRadius = majorRadius + minorRadius * cosPhi;
                const Vector3 normal = Normalize({
                    -cosPhi * sinTheta,
                    sinPhi,
                    cosPhi * cosTheta
                });

                return VertexData{
                    {
                        -sinTheta * ringRadius,
                        sinPhi * minorRadius,
                        cosTheta * ringRadius,
                        1.0f
                    },
                    { u, v },
                    normal
                };
            };

            const VertexData v00 = makeVertex(theta0, phi0, u0, v0);
            const VertexData v01 = makeVertex(theta1, phi0, u1, v0);
            const VertexData v10 = makeVertex(theta0, phi1, u0, v1);
            const VertexData v11 = makeVertex(theta1, phi1, u1, v1);

            AppendQuad(modelData, v00, v01, v10, v11);
        }
    }

    return modelData;
}

ModelData Model::CreateCylinderData(
    uint32_t divideCount,
    float topRadius,
    float bottomRadius,
    float height,
    const std::string& textureFilePath)
{
    assert(divideCount >= 3);
    assert(topRadius >= 0.0f);
    assert(bottomRadius >= 0.0f);
    assert(height > 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float radianPerDivide =
        2.0f * std::numbers::pi_v<float> / static_cast<float>(divideCount);

    for (uint32_t index = 0; index < divideCount; ++index) {
        const float angle = static_cast<float>(index) * radianPerDivide;
        const float nextAngle =
            static_cast<float>(index + 1) * radianPerDivide;

        const float sinValue = std::sin(angle);
        const float cosValue = std::cos(angle);
        const float sinNext = std::sin(nextAngle);
        const float cosNext = std::cos(nextAngle);

        const float u =
            static_cast<float>(index) / static_cast<float>(divideCount);
        const float uNext =
            static_cast<float>(index + 1) / static_cast<float>(divideCount);

        const Vector3 normalCurrent =
            Normalize({ -sinValue, 0.0f, cosValue });
        const Vector3 normalNext =
            Normalize({ -sinNext, 0.0f, cosNext });

        const VertexData topCurrent = {
            { -sinValue * topRadius, height, cosValue * topRadius, 1.0f },
            { u, 0.0f },
            normalCurrent
        };
        const VertexData topNext = {
            { -sinNext * topRadius, height, cosNext * topRadius, 1.0f },
            { uNext, 0.0f },
            normalNext
        };
        const VertexData bottomCurrent = {
            { -sinValue * bottomRadius, 0.0f, cosValue * bottomRadius, 1.0f },
            { u, 1.0f },
            normalCurrent
        };
        const VertexData bottomNext = {
            { -sinNext * bottomRadius, 0.0f, cosNext * bottomRadius, 1.0f },
            { uNext, 1.0f },
            normalNext
        };

        modelData.vertices.push_back(topCurrent);
        modelData.vertices.push_back(topNext);
        modelData.vertices.push_back(bottomCurrent);
        modelData.vertices.push_back(bottomCurrent);
        modelData.vertices.push_back(topNext);
        modelData.vertices.push_back(bottomNext);
    }

    return modelData;
}

ModelData Model::CreateConeData(
    uint32_t divideCount,
    float radius,
    float height,
    const std::string& textureFilePath)
{
    assert(divideCount >= 3);
    assert(radius > 0.0f);
    assert(height > 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float radianPerDivide =
        2.0f * std::numbers::pi_v<float> / static_cast<float>(divideCount);
    const VertexData apex = {
        { 0.0f, height, 0.0f, 1.0f },
        { 0.5f, 0.0f },
        { 0.0f, 1.0f, 0.0f }
    };

    for (uint32_t index = 0; index < divideCount; ++index) {
        const float angle = static_cast<float>(index) * radianPerDivide;
        const float nextAngle =
            static_cast<float>(index + 1) * radianPerDivide;

        const float sinValue = std::sin(angle);
        const float cosValue = std::cos(angle);
        const float sinNext = std::sin(nextAngle);
        const float cosNext = std::cos(nextAngle);

        const float u =
            static_cast<float>(index) / static_cast<float>(divideCount);
        const float uNext =
            static_cast<float>(index + 1) / static_cast<float>(divideCount);

        const Vector3 normalCurrent =
            Normalize({ -sinValue * height, radius, cosValue * height });
        const Vector3 normalNext =
            Normalize({ -sinNext * height, radius, cosNext * height });

        const VertexData current = {
            { -sinValue * radius, 0.0f, cosValue * radius, 1.0f },
            { u, 1.0f },
            normalCurrent
        };
        const VertexData next = {
            { -sinNext * radius, 0.0f, cosNext * radius, 1.0f },
            { uNext, 1.0f },
            normalNext
        };

        AppendTriangle(modelData, apex, current, next);
    }

    return modelData;
}

ModelData Model::CreateBoxData(
    float width,
    float height,
    float depth,
    const std::string& textureFilePath)
{
    assert(width > 0.0f);
    assert(height > 0.0f);
    assert(depth > 0.0f);

    ModelData modelData;
    modelData.material.textureFilePath = textureFilePath;

    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;
    const float halfDepth = depth * 0.5f;

    const Vector4 leftTopFront = { -halfWidth, halfHeight, -halfDepth, 1.0f };
    const Vector4 rightTopFront = { halfWidth, halfHeight, -halfDepth, 1.0f };
    const Vector4 leftBottomFront = { -halfWidth, -halfHeight, -halfDepth, 1.0f };
    const Vector4 rightBottomFront = { halfWidth, -halfHeight, -halfDepth, 1.0f };
    const Vector4 leftTopBack = { -halfWidth, halfHeight, halfDepth, 1.0f };
    const Vector4 rightTopBack = { halfWidth, halfHeight, halfDepth, 1.0f };
    const Vector4 leftBottomBack = { -halfWidth, -halfHeight, halfDepth, 1.0f };
    const Vector4 rightBottomBack = { halfWidth, -halfHeight, halfDepth, 1.0f };

    auto makeVertex = [](const Vector4& position, const Vector2& uv, const Vector3& normal) {
        return VertexData{ position, uv, normal };
    };

    AppendQuad(
        modelData,
        makeVertex(leftTopFront, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }),
        makeVertex(rightTopFront, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }),
        makeVertex(leftBottomFront, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }),
        makeVertex(rightBottomFront, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f })
    );
    AppendQuad(
        modelData,
        makeVertex(rightTopBack, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }),
        makeVertex(leftTopBack, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }),
        makeVertex(rightBottomBack, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }),
        makeVertex(leftBottomBack, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f })
    );
    AppendQuad(
        modelData,
        makeVertex(leftTopBack, { 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }),
        makeVertex(leftTopFront, { 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }),
        makeVertex(leftBottomBack, { 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }),
        makeVertex(leftBottomFront, { 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f })
    );
    AppendQuad(
        modelData,
        makeVertex(rightTopFront, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }),
        makeVertex(rightTopBack, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }),
        makeVertex(rightBottomFront, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }),
        makeVertex(rightBottomBack, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f })
    );
    AppendQuad(
        modelData,
        makeVertex(leftTopBack, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }),
        makeVertex(rightTopBack, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }),
        makeVertex(leftTopFront, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }),
        makeVertex(rightTopFront, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f })
    );
    AppendQuad(
        modelData,
        makeVertex(leftBottomFront, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }),
        makeVertex(rightBottomFront, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }),
        makeVertex(leftBottomBack, { 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }),
        makeVertex(rightBottomBack, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f })
    );

    return modelData;
}
