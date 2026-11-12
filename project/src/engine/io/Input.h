// DirectInputから取得したキーとマウスの状態を保存する　前回との差分で押した瞬間を判定する

#pragma once
#include <Windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定
#include <dinput.h>
#include "engine/base/Math.h"
#include "engine/base/WinApp.h"
#include <array>

// 入力
class Input {
public:
	template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	// 初期化
	void Initialize(WinApp* winApp);

	// 更新
	void Update();

	/// <summary>
	/// キーの押下をチェック
	/// </summary>
	/// <param name="keyNumber"></param>
	/// <returns>押されているか</returns>
	bool PushKey(BYTE keyNumber);

	/// <summary>
	/// キーのトリガーをチェック
	/// </summary>
	/// <param name="keyNumber"></param>
	/// <returns>トリガーか</returns>
	bool TriggerKey(BYTE keyNumber);

	const Math::Vector2& GetMousePosition() const { return playbackActive_ ? playbackMouse_ : mousePosition_; }
    // デモの操作をゲーム内だけで再生する　OSの入力とその前フレームは変更しない
    void SetPlaybackFrame(const std::array<BYTE, 256>& keys, const Math::Vector2& mouse) {
        playbackPrevious_ = playbackKeys_;
        playbackKeys_ = keys;
        playbackMouse_ = mouse;
        playbackActive_ = true;
    }
#ifdef _DEBUG
    // 通しテスト専用　OSへ入力は送らず、通常のPushKey/TriggerKey経路を検証する
    void SetTestFrame(const std::array<BYTE, 256>& keys, const Math::Vector2& mouse) {
        memcpy(key, keys.data(), sizeof(key));
        mousePosition_ = mouse;
    }
#endif

private:
	// メンバ変数
	ComPtr<IDirectInputDevice8> keyboard; // キーボードデバイス

	//  DirectInputインターフェース
	ComPtr<IDirectInput8> directInput;

	// 全キーの状態
	BYTE key[256] = {};
	BYTE keyPre[256] = {};
	Math::Vector2 mousePosition_{ 0.0f, 0.0f };
    std::array<BYTE, 256> playbackKeys_{}, playbackPrevious_{};
    Math::Vector2 playbackMouse_{};
    bool playbackActive_ = false;

	//WindowsAPI
	WinApp* winApp = nullptr;
};
