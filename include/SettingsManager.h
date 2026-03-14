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

    void save() {
        std::string path = getSettingsFilePath();
        WritePrivateProfileStringA("Server", "IP", settings.ip.c_str(), path.c_str());
        WritePrivateProfileStringA("Server", "Port", std::to_string(settings.port).c_str(), path.c_str());
        WritePrivateProfileStringA("Data", "Prefix", std::to_string(settings.prefix).c_str(), path.c_str());
        WritePrivateProfileStringA("Data", "Postfix1", std::to_string(settings.postfix1).c_str(), path.c_str());
        WritePrivateProfileStringA("Data", "Postfix2", std::to_string(settings.postfix2).c_str(), path.c_str());
    }

    bool load() {
        std::string path = getSettingsFilePath();
        if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
            return false;
        }
        char ipBuf[256];
        GetPrivateProfileStringA("Server", "IP", "127.0.0.1", ipBuf, sizeof(ipBuf), path.c_str());
        settings.ip = ipBuf;
        settings.port = GetPrivateProfileIntA("Server", "Port", 10001, path.c_str());
        settings.prefix = GetPrivateProfileIntA("Data", "Prefix", 0, path.c_str());
        settings.postfix1 = GetPrivateProfileIntA("Data", "Postfix1", 13, path.c_str());
        settings.postfix2 = GetPrivateProfileIntA("Data", "Postfix2", 0, path.c_str());
        return true;
    }

private:
    SettingsManager() { load(); }
    AppSettings settings;

    std::string getExeDirectory() {
        char buffer[MAX_PATH];
        GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        std::string path(buffer);
        return path.substr(0, path.find_last_of("\\/"));
    }

    std::string getSettingsFilePath() {
        return getExeDirectory() + "\\settings.ini";
    }
};
