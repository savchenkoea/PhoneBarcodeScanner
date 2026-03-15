#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <windows.h>

class SettingsDialog {
public:
    static void Show(HWND hWndParent, HINSTANCE hInst);
};

// Константы для окна настроек
constexpr int IDC_IP_ADDRESS = 301;
constexpr int IDC_PORT = 302;
constexpr int IDC_PREFIX = 303;
constexpr int IDC_POSTFIX1 = 304;
constexpr int IDC_POSTFIX2 = 305;
constexpr int IDB_SAVE_SETTINGS = 306;

#endif // SETTINGS_DIALOG_H
