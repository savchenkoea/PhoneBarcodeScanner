#ifndef INCLUDE_STRINGUTILS_H
#define INCLUDE_STRINGUTILS_H

#include <string>
#include <stdexcept>
#include <windows.h>

namespace StringUtils {
    inline std::string Base64Decode(const std::string& input) {
        static constexpr unsigned char kDecodeTable[256] = {
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
            52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
            64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
            15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
            64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
            41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
        };

        const std::size_t len = input.size();
        if (len % 4 != 0) {
            throw std::invalid_argument("Base64Decode: input length is not a multiple of 4");
        }

        std::size_t out_len = len / 4 * 3;
        if (len > 0 && input[len - 1] == '=') --out_len;
        if (len > 1 && input[len - 2] == '=') --out_len;

        std::string out(out_len, '\0');
        std::size_t j = 0;
        for (std::size_t i = 0; i < len; i += 4) {
            const unsigned char a = kDecodeTable[static_cast<unsigned char>(input[i])];
            const unsigned char b = kDecodeTable[static_cast<unsigned char>(input[i + 1])];
            const unsigned char c = kDecodeTable[static_cast<unsigned char>(input[i + 2])];
            const unsigned char d = kDecodeTable[static_cast<unsigned char>(input[i + 3])];
            if (a == 64 || b == 64) {
                throw std::invalid_argument("Base64Decode: invalid character in input");
            }
            out[j++] = static_cast<char>((a << 2) | (b >> 4));
            if (input[i + 2] != '=') {
                if (c == 64) throw std::invalid_argument("Base64Decode: invalid character in input");
                out[j++] = static_cast<char>(((b & 0x0F) << 4) | (c >> 2));
            }
            if (input[i + 3] != '=') {
                if (d == 64) throw std::invalid_argument("Base64Decode: invalid character in input");
                out[j++] = static_cast<char>(((c & 0x03) << 6) | d);
            }
        }
        return out;
    }


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

#endif
