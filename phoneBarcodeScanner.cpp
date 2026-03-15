
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <shellapi.h>
#include <memory>
#include <string>
#include <vector>
#include <strsafe.h>
#include <commctrl.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "WSErrors.h"
#include "WSServerThread.h"
#include "SettingsManager.h"
#include "StringUtils.h"
#include "InputEmulator.h"
#include <qrencode.h>
#include "NetworkUtils.h"

#pragma comment(lib, "comctl32.lib")

#define IDB_START_SERVER 101
#define IDB_EXIT 102
#define IDB_SETTINGS 104
#define IDC_LOG_EDIT 103
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 201
#define ID_TRAY_RESTORE 202

#define WM_QR_UPDATE (WM_USER + 2)

// Константы для окна настроек
#define IDC_IP_ADDRESS 301
#define IDC_PORT 302
#define IDC_PREFIX 303
#define IDC_POSTFIX1 304
#define IDC_POSTFIX2 305
#define IDB_SAVE_SETTINGS 306

constexpr UINT WM_ADD_LOG_MESSAGE = WM_APP + 1;

HINSTANCE hInst;
HWND hMainWnd;
HWND hLogEdit;
HWND hLogContainer;
NOTIFYICONDATA nid = { 0 };
WSServerThread srv;
std::string currentQRText;

void AddLogMessageToEdit(const std::string& message)
{
    if (!hLogEdit || !IsWindow(hLogEdit))
        return;

    std::wstring wmsg = StringUtils::Utf8ToWide(message + "\r\n");
    if (wmsg.empty())
        return;

    SendMessageW(hLogEdit, EM_SETSEL, static_cast<WPARAM>(-1), -1);
    SendMessageW(hLogEdit, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(wmsg.c_str()));
}

void PostLogMessage(const std::string& message)
{
    if (hMainWnd == nullptr) {
        return;
    }

    auto* text = new std::string(message);
    if (!PostMessage(hMainWnd, WM_ADD_LOG_MESSAGE, 0, reinterpret_cast<LPARAM>(text)))
    {
        delete text;
    }
}

void onNewConnection(int id, const std::string& ip, int port)
{
    PostLogMessage("Новое соединение " + std::to_string(id) + " от " + ip + ":" + std::to_string(port));
}

void onClosedConnection(int id)
{
    PostLogMessage("Соединение " + std::to_string(id) + " закрыто");
}

// Удалены SendUnicodeChar, SendVirtualKey, Utf8ToWide,
// так как они теперь в соответствующих хедерах.

void onDataReceiving(int id, const std::string& data)
{
    try {
        std::stringstream ss(data);
        boost::property_tree::ptree pt;
        boost::property_tree::read_json(ss, pt);

        std::string command = pt.get<std::string>("command", "");
        std::string payload = pt.get<std::string>("data", "");

        if (command == "auth") {
            if (payload == srv.getPasskey()) {
                srv.setAuthenticated(id, true);
                PostLogMessage("Клиент " + std::to_string(id) + " успешно авторизован");
                srv.generateNewPasskey();
            } else {
                PostLogMessage("Клиент " + std::to_string(id) + ": неверный passkey. Соединение разорвано.");
                srv.close(id);
            }
            return;
        }

        if (!srv.isAuthenticated(id)) {
            PostLogMessage("Клиент " + std::to_string(id) + ": попытка передачи данных без авторизации. Соединение разорвано.");
            srv.close(id);
            return;
        }

        if (command == "scan") {
            PostLogMessage("Данные от " + std::to_string(id) + ": " + payload);
            const std::wstring wideData = StringUtils::Utf8ToWide(payload);
            const auto& s = SettingsManager::getInstance().getSettings();
            InputEmulator::SendString(wideData, s.prefix, s.postfix1, s.postfix2);
        } else {
            PostLogMessage("Неизвестная команда от " + std::to_string(id) + ": " + command);
        }

    } catch (const std::exception& e) {
        PostLogMessage("Ошибка разбора JSON от " + std::to_string(id) + ": " + e.what());
        // Если это не JSON или ошибка, и клиент не авторизован - закрываем
        if (!srv.isAuthenticated(id)) {
            srv.close(id);
        }
    }
}

void ResizeControls(HWND hWnd)
{
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    constexpr int left = 10;
    constexpr int top = 50;
    constexpr int qrSize = 200;
    constexpr int spacing = 10;

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

void DrawQrCode(HWND hWnd, HDC hdc, const std::string& text) {
    if (text.empty()) return;

    QRcode* qr = QRcode_encodeString(text.c_str(), 0, QR_ECLEVEL_H, QR_MODE_8, 1);
    if (!qr) return;

    try {
        RECT rcClient;
        GetClientRect(hWnd, &rcClient);
        
        int qrMargin = 10;
        int qrDrawSize = 200;
        int xStart = rcClient.right - qrDrawSize - qrMargin;
        int yStart = 50;

        // Quiet zone (свободная зона) вокруг QR-кода должна быть минимум 4 модуля
        int border = 4;
        int modules = qr->width;
        int totalModules = modules + border * 2;
        int dotSize = qrDrawSize / totalModules;
        if (dotSize < 1) dotSize = 1;

        int actualSize = dotSize * totalModules;
        int xOffset = xStart + (qrDrawSize - actualSize) / 2;
        int yOffset = yStart + (qrDrawSize - actualSize) / 2;

        HBRUSH hBlack = CreateSolidBrush(RGB(0, 0, 0));
        HBRUSH hWhite = CreateSolidBrush(RGB(255, 255, 255));

        // Фон всей области QR (включая центрирование)
        RECT rcBg = { xStart, yStart, xStart + qrDrawSize, yStart + qrDrawSize };
        FillRect(hdc, &rcBg, hWhite);

        for (int y = 0; y < modules; y++) {
            for (int x = 0; x < modules; x++) {
                if (qr->data[y * modules + x] & 1) {
                    RECT rc = {
                        xOffset + (x + border) * dotSize,
                        yOffset + (y + border) * dotSize,
                        xOffset + (x + border + 1) * dotSize,
                        yOffset + (y + border + 1) * dotSize
                    };
                    FillRect(hdc, &rc, hBlack);
                }
            }
        }

        DeleteObject(hBlack);
        DeleteObject(hWhite);
    } catch (...) {
        // Ошибка отрисовки QR
    }
    QRcode_free(qr);
}


LRESULT CALLBACK SettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        {
            const auto& settings = SettingsManager::getInstance().getSettings();
            HWND hIp = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                       WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 120, 10, 150, 200,
                                       hDlg, reinterpret_cast<HMENU>(IDC_IP_ADDRESS), hInst, nullptr);

            auto addresses = NetworkUtils::GetActiveIPv4Addresses();
            bool found = false;
            for (const auto& addr : addresses)
            {
                SendMessageW(hIp, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(StringUtils::Utf8ToWide(addr).c_str()));
                if (addr == settings.ip) {
                    found = true;
                }
            }

            int index = CB_ERR;
            if (found) {
                index = static_cast<int>(SendMessageW(hIp, CB_FINDSTRINGEXACT, -1, reinterpret_cast<LPARAM>(StringUtils::Utf8ToWide(settings.ip).c_str())));
            }

            if (index != CB_ERR) {
                SendMessage(hIp, CB_SETCURSEL, index, 0);
            } else if (SendMessage(hIp, CB_GETCOUNT, 0, 0) > 0) {
                SendMessage(hIp, CB_SETCURSEL, 0, 0);
            }

            CreateWindowExW(0, L"STATIC", L"IP адрес:", WS_CHILD | WS_VISIBLE, 10, 10, 100, 20, hDlg, nullptr, hInst,
                            nullptr);
            CreateWindowExW(0, L"STATIC", L"Порт:", WS_CHILD | WS_VISIBLE, 10, 40, 100, 20, hDlg, nullptr, hInst,
                            nullptr);
            HWND hPort = CreateWindowExA(0, "EDIT", std::to_string(settings.port).c_str(),
                                         WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 120, 40, 60, 20, hDlg,
                                         reinterpret_cast<HMENU>(IDC_PORT), hInst, nullptr);
            SendMessage(hPort, EM_SETLIMITTEXT, 5, 0);

            CreateWindowExW(0, L"STATIC", L"Префикс:", WS_CHILD | WS_VISIBLE, 10, 70, 100, 20, hDlg, nullptr, hInst,
                            nullptr);
            HWND hPrefix = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                           WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 120, 70, 150, 200,
                                           hDlg, reinterpret_cast<HMENU>(IDC_PREFIX), hInst, nullptr);

            CreateWindowExW(0, L"STATIC", L"Постфикс 1:", WS_CHILD | WS_VISIBLE, 10, 100, 100, 20, hDlg, nullptr, hInst,
                            nullptr);
            HWND hPostfix1 = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                             WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 120, 100, 150, 200,
                                             hDlg, reinterpret_cast<HMENU>(IDC_POSTFIX1), hInst, nullptr);

            CreateWindowExW(0, L"STATIC", L"Постфикс 2:", WS_CHILD | WS_VISIBLE, 10, 130, 100, 20, hDlg, nullptr, hInst,
                            nullptr);
            HWND hPostfix2 = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                             WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 120, 130, 150, 200,
                                             hDlg, reinterpret_cast<HMENU>(IDC_POSTFIX2), hInst, nullptr);

            CreateWindowExW(0, L"BUTTON", L"Сохранить", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 100, 170, 100, 30, hDlg,
                            reinterpret_cast<HMENU>(IDB_SAVE_SETTINGS), hInst, nullptr);

            auto fillCombo = [&](HWND hCombo, int selectedVal) {
                SendMessageW(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"<NONE>"));
                for (int i = 1; i < 128; ++i)
                {
                    wchar_t buf[64];
                    auto name = L"";
                    if (i == 13) name = L"CR";
                    else if (i == 10) name = L"LF";
                    else if (i == 8) name = L"BS";
                    else if (i == 9) name = L"TAB";
                    else if (i == 27) name = L"ESC";
                    else if (i == 32) name = L"Space";

                    if (*name)
                    {
                        swprintf(buf, 64, L"%d(%s)", i, name);
                    }
                    else if (isprint(i))
                    {
                        swprintf(buf, 64, L"%d(%c)", i, static_cast<wchar_t>(i));
                    }
                    else
                    {
                        swprintf(buf, 64, L"%d", i);
                    }
                    SendMessageW(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(buf));
                }
                SendMessage(hCombo, CB_SETCURSEL, selectedVal, 0);
            };

            fillCombo(hPrefix, settings.prefix);
            fillCombo(hPostfix1, settings.postfix1);
            fillCombo(hPostfix2, settings.postfix2);

            return TRUE;
        }
    case WM_COMMAND:
        {
            if (LOWORD(wParam) == IDB_SAVE_SETTINGS)
            {
                AppSettings settings;
                HWND hIp = GetDlgItem(hDlg, IDC_IP_ADDRESS);
                int ipIdx = static_cast<int>(SendMessage(hIp, CB_GETCURSEL, 0, 0));
                if (ipIdx != CB_ERR) {
                    wchar_t ipBufW[32];
                    SendMessageW(hIp, CB_GETLBTEXT, ipIdx, reinterpret_cast<LPARAM>(ipBufW));
                    settings.ip = StringUtils::WideToUtf8(ipBufW);
                } else {
                    settings.ip = SettingsManager::getInstance().getSettings().ip;
                }

                char portBuf[16];
                GetDlgItemTextA(hDlg, IDC_PORT, portBuf, sizeof(portBuf));
                settings.port = strtol(portBuf, nullptr, 10);
                if (settings.port < 1) settings.port = 1;
                if (settings.port > 65535) settings.port = 65535;

                settings.prefix = static_cast<int>(SendMessage(GetDlgItem(hDlg, IDC_PREFIX), CB_GETCURSEL, 0, 0));
                settings.postfix1 = static_cast<int>(SendMessage(GetDlgItem(hDlg, IDC_POSTFIX1), CB_GETCURSEL, 0, 0));
                settings.postfix2 = static_cast<int>(SendMessage(GetDlgItem(hDlg, IDC_POSTFIX2), CB_GETCURSEL, 0, 0));

                SettingsManager::getInstance().setSettings(settings);
                SettingsManager::getInstance().save();
                EndDialog(hDlg, IDOK);
                return TRUE;
            }
            break;
        }
    case WM_CLOSE:
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
    default: return FALSE;
    }
    return FALSE;
}

void ShowSettingsDialog(HWND hWndParent) {
    [[maybe_unused]] auto lpDialogFunc = [](HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) -> INT_PTR {
        return (INT_PTR)SettingsDlgProc(hDlg, msg, wp, lp);
    };

    // Создаем шаблон диалога в памяти (DLGTEMPLATE)
    HGLOBAL hgbl = GlobalAlloc(GMEM_ZEROINIT, 1024);
    if (!hgbl) return;
    auto lpdt = static_cast<LPDLGTEMPLATE>(GlobalLock(hgbl));
    lpdt->style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER;
    lpdt->cdit = 0;
    lpdt->x = 0; lpdt->y = 0; lpdt->cx = 150; lpdt->cy = 110;
    GlobalUnlock(hgbl);

    DialogBoxIndirectParamA(hInst, lpdt, hWndParent, (DLGPROC)SettingsDlgProc, 0);
    GlobalFree(hgbl);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_QR_UPDATE:
        {
            const auto& s = SettingsManager::getInstance().getSettings();
            currentQRText = "{\"ip\":\"" + s.ip + "\",\"port\":" + std::to_string(s.port) + ",\"passkey\":\"" + srv.getPasskey() + "\"}";
            InvalidateRect(hWnd, nullptr, TRUE);
            break;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            if (!currentQRText.empty()) {
                DrawQrCode(hWnd, hdc, currentQRText);
            }
            EndPaint(hWnd, &ps);
            break;
        }
        case WM_ADD_LOG_MESSAGE:
        {
            std::unique_ptr<std::string> text(reinterpret_cast<std::string*>(lParam));
            AddLogMessageToEdit(*text);
            break;
        }
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDB_START_SERVER: {
                    HWND hStartButton = GetDlgItem(hWnd, IDB_START_SERVER);
                    const auto& settings = SettingsManager::getInstance().getSettings();

                    if (!srv.running()) {
                        int res = srv.run(settings.ip, settings.port);

                        if (res == ERROR_NO_ERROR) {
                            AddLogMessageToEdit("Сервер запущен по адресу " + settings.ip + ":" + std::to_string(settings.port));
                            SetWindowTextW(hStartButton, L"Остановить сервер");
                            SendMessage(hWnd, WM_QR_UPDATE, 0, 0);
                        } else {
                            AddLogMessageToEdit("Ошибка запуска сервера: " + std::to_string(res));
                            SetWindowTextW(hStartButton, L"Запустить сервер");
                        }
                    } else {
                        int res = srv.stop();

                        if (res == ERROR_NO_ERROR) {
                            AddLogMessageToEdit("Сервер остановлен");
                            SetWindowTextW(hStartButton, L"Запустить сервер");
                            currentQRText.clear();
                            InvalidateRect(hWnd, nullptr, TRUE);
                        } else {
                            AddLogMessageToEdit("Ошибка остановки сервера: " + std::to_string(res));
                            SetWindowTextW(hStartButton, L"Остановить сервер");
                        }
                    }
                    break;
                }
                case IDB_SETTINGS: {
                    if (srv.running()) {
                        MessageBoxW(hWnd, L"Остановите сервер перед изменением настроек.", L"Предупреждение", MB_OK | MB_ICONWARNING);
                    } else {
                        ShowSettingsDialog(hWnd);
                    }
                    break;
                }
                case IDB_EXIT:
                case ID_TRAY_EXIT:
                    Shell_NotifyIcon(NIM_DELETE, &nid);
                    PostQuitMessage(0);
                    break;
                case ID_TRAY_RESTORE:
                    ShowWindow(hWnd, SW_RESTORE);
                    SetForegroundWindow(hWnd);
                    break;
                default: break;
            }
        }
        break;
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
            if (lParam == WM_LBUTTONDBLCLK) {
                ShowWindow(hWnd, SW_RESTORE);
                SetForegroundWindow(hWnd);
            } else if (lParam == WM_RBUTTONUP) {
                POINT curPoint;
                GetCursorPos(&curPoint);
                HMENU hMenu = CreatePopupMenu();
                InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_TRAY_RESTORE, L"Восстановить");
                InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Выход");
                SetForegroundWindow(hWnd);
                TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, curPoint.x, curPoint.y, 0, hWnd, nullptr);
                DestroyMenu(hMenu);
            }
            break;
        case WM_DESTROY:
            Shell_NotifyIcon(NIM_DELETE, &nid);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

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
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1));
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassEx(&wcex)) return 1;

    HWND hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 650, 450, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return 1;

    hMainWnd = hWnd;

    SettingsManager::getInstance().load();

    CreateWindowW(L"BUTTON", L"Запустить сервер", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                  10, 10, 150, 30, hWnd, (HMENU)IDB_START_SERVER, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Настройка", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                  170, 10, 100, 30, hWnd, (HMENU)IDB_SETTINGS, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Выход", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                  280, 10, 80, 30, hWnd, (HMENU)IDB_EXIT, hInstance, nullptr);

    hLogContainer = CreateWindowW(L"STATIC", L"", WS_VISIBLE | WS_CHILD | WS_BORDER,
                                 10, 50, 360, 200, hWnd, nullptr, hInstance, nullptr);

    hLogEdit = CreateWindowExW(0, L"EDIT", L"",
                             WS_VISIBLE | WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                             0, 0, 360, 200, hLogContainer, (HMENU)IDC_LOG_EDIT, hInstance, nullptr);

    // Увеличиваем лимит текста до ~10 МБ
    SendMessageW(hLogEdit, EM_SETLIMITTEXT,  10 * 1024 * 1024, 0);

    ResizeControls(hWnd);

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
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
