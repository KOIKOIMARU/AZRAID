// Windowsのウィンドウとメッセージ処理を担当する　通常表示と全画面の切り替えもここで扱う

#pragma once
#include <Windows.h>
#include <cstdint>

//WindowsAPI
class WinApp
{
public: // 静的メンバ関数
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	// クライアント領域のサイズ
	static const int32_t kClientWidth = 1280;
	static const int32_t kClientHeight = 720;

public: // メンバ関数
	// 初期化
	void Initialize();
	// 終了
	void Finalize();

	// getter
	HWND GetHwnd() const { return hwnd; }

	HINSTANCE GetHInstance() const { return wc.hInstance; }

	// メッセージの処理
	bool ProcessMessage();

    // 排他的モードは使わず、現在のモニターを覆う枠なし全画面へ切り替える
    void ToggleFullscreen();
    bool IsFullscreen() const { return isFullscreen_; }
    void MarkFramePresented() { hasPresentedFrame_ = true; }


private:
	// ウィンドウハンドル
	HWND hwnd = nullptr;

	// ウィンドウクラスの定義
	WNDCLASS wc = {};
    bool isFullscreen_ = false;
    bool hasPresentedFrame_ = false;
    bool toggleFullscreenRequested_ = false; // ウィンドウ処理を抜けたフレーム境界で適用する
    WINDOWPLACEMENT windowedPlacement_{ sizeof(WINDOWPLACEMENT) }; // 元の位置・最大化状態
};
