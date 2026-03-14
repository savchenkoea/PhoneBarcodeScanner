#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace InputEmulator {
    inline bool SendUnicodeChar(wchar_t ch) {
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = 0;
        inputs[0].ki.wScan = ch;
        inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;

        inputs[1] = inputs[0];
        inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

        return SendInput(2, inputs, sizeof(INPUT)) == 2;
    }

    inline bool SendVirtualKey(WORD vk) {
        INPUT inputs[2] = {};
        inputs[0].type = INPUT_KEYBOARD;
        inputs[0].ki.wVk = vk;
        inputs[0].ki.dwFlags = 0;

        inputs[1] = inputs[0];
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

        return SendInput(2, inputs, sizeof(INPUT)) == 2;
    }

    inline void SendString(const std::wstring& text, int prefix = 0, int postfix1 = 0, int postfix2 = 0) {
        if (prefix > 0) SendUnicodeChar(static_cast<wchar_t>(prefix));
        for (wchar_t ch : text) SendUnicodeChar(ch);
        if (postfix1 > 0) SendUnicodeChar(static_cast<wchar_t>(postfix1));
        if (postfix2 > 0) SendUnicodeChar(static_cast<wchar_t>(postfix2));
    }
}
