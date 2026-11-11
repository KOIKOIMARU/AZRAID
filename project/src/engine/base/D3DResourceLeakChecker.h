// 終了時のDirectX診断で、解放されずに残った描画リソースを調べる。

#pragma once

class D3DResourceLeakChecker
{
public:
    /// <summary>
    /// デストラクタ
    /// </summary>
    ~D3DResourceLeakChecker();
};
