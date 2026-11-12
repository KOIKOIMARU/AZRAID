// ===== D3DResourceLeakChecker.cpp =====
// Debug版の終了時に、解放されずに残ったDirectXのオブジェクトを報告する

#include "engine/base/D3DResourceLeakChecker.h"

#include <dxgidebug.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#pragma comment(lib, "dxguid.lib")   // DXGIGetDebugInterface1 用

using Microsoft::WRL::ComPtr;

// デバッグビルドの終了時に、解放されずに残ったDirectX関連のオブジェクトを報告する
D3DResourceLeakChecker::~D3DResourceLeakChecker()
{
#ifdef _DEBUG
    // リソースリークチェック
    ComPtr<IDXGIDebug1> debug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
        debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
    }
#endif
}
