#pragma once

#include <string>
#include <windows.h>

struct AppSettings {
    std::string ip = "127.0.0.1";
    int port = 10001;
    int prefix = 0;
    int postfix1 = 13;
    int postfix2 = 0;
};

class SettingsManager {
public:
    static SettingsManager& getInstance() {
        static SettingsManager instance;
        return instance;
    }

    const AppSettings& getSettings() const { return settings; }
    void setSettings(const AppSettings& newSettings) { settings = newSettings; }

    // Возвращает true, если настройки были найдены в реестре при запуске
    bool hasSettings() const { return settingsFound; }

    void save() {
        HKEY hKey = nullptr;
        if (RegCreateKeyExA(HKEY_CURRENT_USER, REG_KEY_PATH, 0, nullptr,
                            REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr,
                            &hKey, nullptr) != ERROR_SUCCESS) return;

        RegSetValueExA(hKey, "IP", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(settings.ip.c_str()),
            static_cast<DWORD>(settings.ip.size() + 1));

        DWORD val;
        val = static_cast<DWORD>(settings.port);
        RegSetValueExA(hKey, "Port", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(val));
        val = static_cast<DWORD>(settings.prefix);
        RegSetValueExA(hKey, "Prefix", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(val));
        val = static_cast<DWORD>(settings.postfix1);
        RegSetValueExA(hKey, "Postfix1", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(val));
        val = static_cast<DWORD>(settings.postfix2);
        RegSetValueExA(hKey, "Postfix2", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&val), sizeof(val));

        RegCloseKey(hKey);
    }

    bool load() {
        HKEY hKey = nullptr;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY_PATH, 0,
                          KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) return false;

        char ipBuf[256] = {};
        DWORD ipBufSize = sizeof(ipBuf);
        DWORD type = REG_SZ;
        if (RegQueryValueExA(hKey, "IP", nullptr, &type,
                             reinterpret_cast<BYTE*>(ipBuf), &ipBufSize) == ERROR_SUCCESS)
            settings.ip = ipBuf;

        DWORD val = 0;
        DWORD valSize = sizeof(val);
        type = REG_DWORD;
        if (RegQueryValueExA(hKey, "Port", nullptr, &type,
                             reinterpret_cast<BYTE*>(&val), &valSize) == ERROR_SUCCESS)
            settings.port = static_cast<int>(val);

        val = 0; valSize = sizeof(val);
        if (RegQueryValueExA(hKey, "Prefix", nullptr, &type,
                             reinterpret_cast<BYTE*>(&val), &valSize) == ERROR_SUCCESS)
            settings.prefix = static_cast<int>(val);

        val = 0; valSize = sizeof(val);
        if (RegQueryValueExA(hKey, "Postfix1", nullptr, &type,
                             reinterpret_cast<BYTE*>(&val), &valSize) == ERROR_SUCCESS)
            settings.postfix1 = static_cast<int>(val);

        val = 0; valSize = sizeof(val);
        if (RegQueryValueExA(hKey, "Postfix2", nullptr, &type,
                             reinterpret_cast<BYTE*>(&val), &valSize) == ERROR_SUCCESS)
            settings.postfix2 = static_cast<int>(val);

        RegCloseKey(hKey);
        return true;
    }

private:
    static constexpr char REG_KEY_PATH[] = "Software\\PhoneBarcodeScanner";

    SettingsManager() : settingsFound(load()) {}
    AppSettings settings;
    bool settingsFound = false;
};
