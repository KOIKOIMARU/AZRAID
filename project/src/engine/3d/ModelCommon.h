// モデルの初期化と描画が参照するDirectXの共通機能を保持する。

#pragma once
#include "engine/base/DirectXCommon.h"
#include <string>

class SrvManager;

class ModelCommon
{
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    SrvManager* GetSrvManager() const { return srvManager_; }
    void SetEnvironmentTexturePath(const std::string& texturePath) {
        environmentTexturePath_ = texturePath;
    }
    const std::string& GetEnvironmentTexturePath() const {
        return environmentTexturePath_;
    }

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    std::string environmentTexturePath_;
};
