// ===== ModelCommon.cpp =====
// モデルの初期化と描画で共有するDirectXとSRV管理の参照を受け渡す

#include "engine/3d/ModelCommon.h"
#include "engine/base/SrvManager.h"

void ModelCommon::Initialize(
    DirectXCommon* dxCommon,
    SrvManager* srvManager)
{
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
}
