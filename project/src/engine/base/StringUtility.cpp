#include "engine/base/StringUtility.h"
#include <Windows.h>

namespace StringUtility {

    // stringをwstringに変換する関数
    std::wstring ConvertString(const std::string& str) {
        if (str.empty()) {
            return std::wstring();
        }

        int sizeNeeded = MultiByteToWideChar(
            CP_UTF8, 0,
            str.data(),
            static_cast<int>(str.size()),
            nullptr, 0
        );
        if (sizeNeeded == 0) {
            return std::wstring();
        }

        std::wstring result(sizeNeeded, 0);
        MultiByteToWideChar(
            CP_UTF8, 0,
            str.data(),
            static_cast<int>(str.size()),
            &result[0], sizeNeeded
        );
        return result;
    }

    // wstringをstringに変換する関数
    std::string ConvertString(const std::wstring& str) {
        if (str.empty()) {
            return std::string();
        }

        int sizeNeeded = WideCharToMultiByte(
            CP_UTF8, 0,
            str.data(),
            static_cast<int>(str.size()),
            nullptr, 0,
            nullptr, nullptr
        );
        if (sizeNeeded == 0) {
            return std::string();
        }

        std::string result(sizeNeeded, 0);
        WideCharToMultiByte(
            CP_UTF8, 0,
            str.data(),
            static_cast<int>(str.size()),
            &result[0], sizeNeeded,
            nullptr, nullptr
        );
        return result;
    }

} // namespace StringUtility
