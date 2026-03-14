#pragma once

#include <string>
#include <vector>
#include <windows.h>

namespace StringUtils {
    inline std::wstring Utf8ToWide(const std::string& text) {
        if (text.empty()) return {};
        int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0);
        if (size <= 0) return {};
        std::wstring wide(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), &wide[0], size);
        return wide;
    }

    inline std::string WideToUtf8(const std::wstring& text) {
        if (text.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string utf8(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), (int)text.size(), &utf8[0], size, nullptr, nullptr);
        return utf8;
    }
}
