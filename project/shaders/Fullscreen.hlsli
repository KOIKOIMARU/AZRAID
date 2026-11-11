// 全画面エフェクトで共有する入出力。頂点からピクセルへ画面位置とUVを渡す。

struct VertexShaderOutput {
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
};
