// ファイルパスや表示文字列を、Windows APIで使う形式へ変換する。

#pragma once
#include <string>

// 文字コードユーティリティ
namespace StringUtility {
	// stringをwstringに変換する関数
	std::wstring ConvertString(const std::string& str);
	// wstringをstringに変換する関数
	std::string ConvertString(const std::wstring& str);
}
