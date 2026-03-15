#include "Logger.h"
#include "StringUtils.h"
#include <memory>

HWND Logger::m_hLogEdit = nullptr;
HWND Logger::m_hMainWnd = nullptr;

void Logger::Initialize(HWND hLogEdit, HWND hMainWnd) {
    m_hLogEdit = hLogEdit;
    m_hMainWnd = hMainWnd;
}

void Logger::AddLogMessageToEdit(const std::string& message) {
    if (!m_hLogEdit || !IsWindow(m_hLogEdit))
        return;

    std::wstring wmsg = StringUtils::Utf8ToWide(message + "\r\n");
    if (wmsg.empty())
        return;

    SendMessageW(m_hLogEdit, EM_SETSEL, static_cast<WPARAM>(-1), -1);
    SendMessageW(m_hLogEdit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(wmsg.c_str()));
}

void Logger::PostLogMessage(const std::string& message) {
    if (m_hMainWnd == nullptr) {
        return;
    }

    auto text = std::make_unique<std::string>(message);
    if (PostMessage(m_hMainWnd, WM_ADD_LOG_MESSAGE, 0, reinterpret_cast<LPARAM>(text.get()))) {
        text.release();
    }
}
