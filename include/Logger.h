#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <windows.h>

class Logger {
public:
    static void Initialize(HWND hLogEdit, HWND hMainWnd);
    static void AddLogMessageToEdit(const std::string& message);
    static void PostLogMessage(const std::string& message);
    static void ClearLog();

private:
    static HWND m_hLogEdit;
    static HWND m_hMainWnd;

    static constexpr DWORD MAX_LOG_SIZE = 10 * 1024 * 1024; // 10 МБ

    static void TrimLogIfNeeded();
};

// Сообщение для передачи лога в основной поток
constexpr UINT WM_ADD_LOG_MESSAGE = WM_APP + 1;

#endif // LOGGER_H
