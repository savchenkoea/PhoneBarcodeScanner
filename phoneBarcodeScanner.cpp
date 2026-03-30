
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <shellapi.h>
#include <memory>
#include <string>
#include <vector>
#include <strsafe.h>

#include <WSErrors.h>
#include <WSServerThread.h>
#include <SettingsManager.h>
#include <NetworkUtils.h>
#include <Logger.h>
#include <QRCodeRenderer.h>
#include <SettingsDialog.h>
#include <CommandHandler.h>
#include <Resource.h>

#pragma comment(lib, "comctl32.lib")

constexpr int IDB_START_SERVER = 101;
constexpr int IDB_EXIT = 102;
constexpr int IDC_LOG_EDIT = 103;
constexpr int IDB_SETTINGS = 104;
constexpr int IDB_CLEAR_LOG = 105;
constexpr int WM_TRAYICON = WM_USER + 1;
constexpr int ID_TRAY_EXIT = 201;
constexpr int ID_TRAY_RESTORE = 202;

constexpr int WM_QR_UPDATE = WM_USER + 2;
extern const int WM_MINIMIZE_TO_TRAY = WM_USER + 3;

HINSTANCE hInst;
HWND hMainWnd;
HWND hLogEdit;
HWND hLogContainer;
HFONT hLogFont = nullptr;
NOTIFYICONDATA nid = { 0 };
WSServerThread srv;
std::string currentQRText;

void onNewConnection(int id, const std::string& ip, int port)
{
    Logger::PostLogMessage("Новое соединение " + std::to_string(id) + " от " + ip + ":" + std::to_string(port));
}

void onClosedConnection(int id)
{
    Logger::PostLogMessage("Соединение " + std::to_string(id) + " закрыто");
}

void onDataReceiving(int id, const std::string& data)
{
    CommandHandler::HandleData(id, data, srv);
}

void ResizeControls(HWND hWnd)
{
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    constexpr int left = 10;
    constexpr int top = 50;
    constexpr int qrSize = 300;
    constexpr int spacing = 4;

    int logWidth = rcClient.right - left - qrSize - 2 * spacing;
    int height = rcClient.bottom - top - 10;

    if (logWidth < 100) logWidth = 100;
    if (height < 0) height = 0;

    SetWindowPos(hLogContainer, nullptr, left, top, logWidth, height, SWP_NOZORDER);

    RECT rcLog;
    GetClientRect(hLogContainer, &rcLog);
    SetWindowPos(hLogEdit, nullptr, 0, 0, rcLog.right, rcLog.bottom, SWP_NOZORDER);
    
    // Перерисовываем всё окно, чтобы обновить QR-код
    InvalidateRect(hWnd, nullptr, TRUE);
}





void OnStartServerCommand(HWND hWnd) {
    HWND hStartButton = GetDlgItem(hWnd, IDB_START_SERVER);
    const auto& settings = SettingsManager::getInstance().getSettings();

    if (!srv.running()) {
        const int res = srv.run(settings.ip, settings.port);

        if (res == ERROR_NO_ERROR) {
            Logger::AddLogMessageToEdit("Сервер запущен по адресу " + settings.ip + ":" + std::to_string(settings.port));
            SetWindowTextW(hStartButton, L"Остановить сервер");
            SendMessage(hWnd, WM_QR_UPDATE, 0, 0);
        } else {
            Logger::AddLogMessageToEdit("Ошибка запуска сервера: " + std::to_string(res));
            SetWindowTextW(hStartButton, L"Запустить сервер");
        }
    } else {
        const int res = srv.stop();

        if (res == ERROR_NO_ERROR) {
            Logger::AddLogMessageToEdit("Сервер остановлен");
            SetWindowTextW(hStartButton, L"Запустить сервер");
            currentQRText.clear();
            InvalidateRect(hWnd, nullptr, TRUE);
        } else {
            Logger::AddLogMessageToEdit("Ошибка остановки сервера: " + std::to_string(res));
            SetWindowTextW(hStartButton, L"Остановить сервер");
        }
    }
}

void OnSettingsCommand(HWND hWnd) {
    if (srv.running()) {
        MessageBoxW(hWnd, L"Остановите сервер перед изменением настроек.", L"Предупреждение", MB_OK | MB_ICONWARNING);
    } else {
        SettingsDialog::Show(hWnd, hInst);
    }
}

void OnTrayIcon(HWND hWnd, LPARAM lParam) {
    if (lParam == WM_LBUTTONDBLCLK) {
        ShowWindow(hWnd, SW_RESTORE);
        SetForegroundWindow(hWnd);
    } else if (lParam == WM_RBUTTONUP) {
        POINT curPoint;
        GetCursorPos(&curPoint);
        if (HMENU hMenu = CreatePopupMenu()) {
            InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_RESTORE, L"Восстановить");
            InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Выход");
            SetForegroundWindow(hWnd);
            TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, curPoint.x, curPoint.y, 0, hWnd, nullptr);
            DestroyMenu(hMenu);
        }
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_QR_UPDATE: {
            const auto& s = SettingsManager::getInstance().getSettings();
            currentQRText = R"({"ip":")" + s.ip +
                            R"(","port":)" + std::to_string(s.port) +
                            R"(,"passkey":")" + srv.getPasskey() +
                            R"(","certPin":")" + srv.getCertPin() + R"("})";
            InvalidateRect(hWnd, nullptr, TRUE);
            break;
        }
        case WM_MINIMIZE_TO_TRAY: {
            ShowWindow(hWnd, SW_HIDE);
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            QRCodeRenderer::DrawQrCode(hWnd, hdc, currentQRText);
            EndPaint(hWnd, &ps);
            break;
        }
        case WM_ADD_LOG_MESSAGE: {
            std::unique_ptr<std::string> text(reinterpret_cast<std::string*>(lParam));
            if (text) {
                Logger::AddLogMessageToEdit(*text);
            }
            break;
        }
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
            case IDB_START_SERVER:
                OnStartServerCommand(hWnd);
                break;
            case IDB_SETTINGS:
                OnSettingsCommand(hWnd);
                break;
            case IDB_CLEAR_LOG:
                Logger::ClearLog();
                break;
            case IDB_EXIT:
            case ID_TRAY_EXIT:
                if (hLogFont) DeleteObject(hLogFont);
                Shell_NotifyIcon(NIM_DELETE, &nid);
                PostQuitMessage(0);
                break;
            case ID_TRAY_RESTORE:
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
                break;
            default: break;
            }
            break;
        }
        case WM_SIZE:
            if (hLogEdit != nullptr) {
                ResizeControls(hWnd);
            }
            break;
        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_MINIMIZE) {
                ShowWindow(hWnd, SW_HIDE);
                return 0;
            }
            return DefWindowProc(hWnd, message, wParam, lParam);
        case WM_CLOSE:
            ShowWindow(hWnd, SW_HIDE);
            break;
        case WM_TRAYICON:
            OnTrayIcon(hWnd, lParam);
            break;
        case WM_DESTROY:
            if (hLogFont) DeleteObject(hLogFont);
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    // Проверка на повторный запуск приложения
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\PhoneBarcodeScannerMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"Приложение PhoneBarcodeScanner уже запущено", L"Предупреждение", MB_OK | MB_ICONWARNING);
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    auto addresses = NetworkUtils::GetActiveIPv4Addresses();
    if (addresses.empty()) {
        MessageBoxW(nullptr, L"Сетевые интерфейсы не найдены. Работа программы будет завершена.", L"Ошибка", MB_OK | MB_ICONERROR);
        return 0;
    }

    srv.sigOnNewConnection.connect(&onNewConnection);
    srv.sigOnClosedConnection.connect(onClosedConnection);
    srv.sigOnDataReceiving.connect(onDataReceiving);
    srv.sigPasskeyChanged.connect([](std::string) {
        if (hMainWnd) PostMessage(hMainWnd, WM_QR_UPDATE, 0, 0);
    });

    hInst = hInstance;
    constexpr char szWindowClass[] = "PhoneBarcodeScannerClass";
    constexpr char szTitle[] = "Phone Barcode Scanner";

    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));

    if (!RegisterClassEx(&wcex)) return 1;

    HWND hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 650, 450, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return 1;

    hMainWnd = hWnd;

    DeleteMenu(GetSystemMenu(hWnd, FALSE), SC_CLOSE, MF_BYCOMMAND);

    bool settingsExist = SettingsManager::getInstance().hasSettings();

    CreateWindowW(L"BUTTON", L"Запустить сервер", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                  10, 10, 150, 30, hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDB_START_SERVER)), hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Настройка", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                  170, 10, 100, 30, hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDB_SETTINGS)), hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Очистить лог", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                   280, 10, 120, 30, hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDB_CLEAR_LOG)), hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Выход", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                  410, 10, 80, 30, hWnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDB_EXIT)), hInstance, nullptr);

    hLogContainer = CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD | WS_BORDER,
                                 10, 50, 360, 200, hWnd, nullptr, hInstance, nullptr);

    hLogEdit = CreateWindowExW(0, L"EDIT", L"",
                             WS_VISIBLE | WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                             0, 0, 360, 200, hLogContainer, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LOG_EDIT)), hInstance, nullptr);

    // Увеличиваем лимит текста до ~10 МБ
    SendMessageW(hLogEdit, EM_SETLIMITTEXT,  10 * 1024 * 1024, 0);

    // Устанавливаем шрифт как на надписях
    LOGFONTW lf = { 0 };
    if (GetObjectW(GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf)) {
        lf.lfHeight = -14;
        lf.lfWeight = FW_NORMAL;
        hLogFont = CreateFontIndirectW(&lf);
        if (hLogFont) {
            SendMessageW(hLogEdit, WM_SETFONT, reinterpret_cast<WPARAM>(hLogFont), TRUE);
        }
    }

    Logger::Initialize(hLogEdit, hMainWnd);

    if (!settingsExist) {
        SettingsDialog::Show(hWnd, hInst);
    } else {
        OnStartServerCommand(hWnd);
    }

    ResizeControls(hWnd);

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAIN_ICON));
    lstrcpyn(nid.szTip, TEXT("Phone Barcode Scanner"), ARRAYSIZE(nid.szTip));
    Shell_NotifyIcon(NIM_ADD, &nid);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return static_cast<int>(msg.wParam);
}

int main(int argc, char* argv[]) {

    return WinMain(GetModuleHandle(nullptr), nullptr, GetCommandLineA(), SW_SHOWNORMAL);
}
