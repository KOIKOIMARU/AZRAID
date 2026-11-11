// ===== Logger.cpp =====
// プログラム内のログをWindowsのデバッグ出力へ送る。

#include "engine/base/Logger.h"
#include <Windows.h>

namespace Logger {

    void Log(const std::string& message) {
        OutputDebugStringA(message.c_str());
    }

}
