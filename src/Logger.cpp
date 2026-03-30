#include <memory>

#include <Logger.h>
#include <StringUtils.h>

HWND Logger::m_hLogEdit = nullptr;
HWND Logger::m_hMainWnd = nullptr;

void Logger::Initialize(HWND hLogEdit, HWND hMainWnd) {
    m_hLogEdit = hLogEdit;
    m_hMainWnd = hMainWnd;
}


void Logger::TrimLogIfNeeded() {
    if (!m_hLogEdit || !IsWindow(m_hLogEdit))
        return;

    DWORD textLength = static_cast<DWORD>(SendMessageW(m_hLogEdit, WM_GETTEXTLENGTH, 0, 0));

    if (textLength > MAX_LOG_SIZE) {
        // Удаляем первые 20% текста
        DWORD removeCount = textLength / 5;

        // Находим позицию первого перевода строки после removeCount символов
        SendMessageW(m_hLogEdit, EM_SETSEL, removeCount, removeCount);
        DWORD lineIndex = static_cast<DWORD>(SendMessageW(m_hLogEdit, EM_LINEFROMCHAR, removeCount, 0));
        DWORD lineStart = static_cast<DWORD>(SendMessageW(m_hLogEdit, EM_LINEINDEX, lineIndex, 0));

        // Удаляем текст от начала до найденной позиции
        SendMessageW(m_hLogEdit, EM_SETSEL, 0, lineStart);
        SendMessageW(m_hLogEdit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L""));

        // Прокручиваем в конец
        SendMessageW(m_hLogEdit, EM_SETSEL, static_cast<WPARAM>(-1), -1);
    }
}


void Logger::AddLogMessageToEdit(const std::string& message) {
    if (!m_hLogEdit || !IsWindow(m_hLogEdit))
        return;

    std::wstring wmsg = StringUtils::Utf8ToWide(message + "\r\n");
    if (wmsg.empty())
        return;

    SendMessageW(m_hLogEdit, EM_SETSEL, static_cast<WPARAM>(-1), -1);
    SendMessageW(m_hLogEdit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(wmsg.c_str()));

    // Проверяем и обрезаем лог при необходимости
    TrimLogIfNeeded();

}

void Logger::ClearLog() {
    if (!m_hLogEdit || !IsWindow(m_hLogEdit))
        return;

    SetWindowTextW(m_hLogEdit, L"");
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
