
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
//#include <commctrl.h>
#include <string>
#include <vector>

#include "WSErrors.h"
#include "WSServerThread.h"

#pragma comment(lib, "comctl32.lib")

#define IDB_START_SERVER 101
#define IDB_EXIT 102
#define IDC_LOG_EDIT 103
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 201
#define ID_TRAY_RESTORE 202

HINSTANCE hInst;
HWND hLogEdit;
NOTIFYICONDATA nid = { 0 };
WSServerThread srv;

void AddLogMessage(const std::string& message)
{
    int len = GetWindowTextLength(hLogEdit);
    SendMessage(hLogEdit, EM_SETSEL, static_cast<WPARAM>(len), len);
    std::string msg = message + "\r\n";

    // Конвертируем UTF-8 в UTF-16
    int wlen = MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, nullptr, 0);
    if (wlen > 0)
    {
        std::vector<wchar_t> wstr(wlen);
        MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, &wstr[0], wlen);
        SendMessageW(hLogEdit, EM_REPLACESEL, 0, reinterpret_cast<LPARAM>(wstr.data()));
    }
}

void onNewConnection(int id, const std::string& ip, int port)
{
    AddLogMessage("Новое соединение " + std::to_string(id) + " от " + ip + ":" + std::to_string(port));
}

void onClosedConnection(int id)
{
    AddLogMessage("Соединение "+ std::to_string(id) + "закрыто" );
}

void onDataReceiving(int id, const std::string& data)
{
    AddLogMessage(data);

    for (char ch : data)
    {
        INPUT input = {0};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = 0;
        input.ki.wScan = ch;
        input.ki.dwFlags = KEYEVENTF_UNICODE;
        SendInput(1, &input, sizeof(INPUT));

        input.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
    }

    // Press Enter key
    INPUT input = {0};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_RETURN;
    input.ki.dwFlags = 0;
    SendInput(1, &input, sizeof(INPUT));

    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

void ResizeControls(HWND hWnd)
{
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    constexpr int left = 10;
    constexpr int top = 50;

    constexpr int rightMargin = 10;
    constexpr int bottomMargin = 10;

    int width = rcClient.right - left - rightMargin;
    int height = rcClient.bottom - top - bottomMargin;

    if (width < 0) width = 0;
    if (height < 0) height = 0;

    SetWindowPos(hLogEdit, nullptr, left, top, width, height, SWP_NOZORDER);
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDB_START_SERVER: {
                    HWND hStartButton = GetDlgItem(hWnd, IDB_START_SERVER);

                    if (!srv.running()) {
                        std::string address = "192.168.1.20";
                        int port = 10001;
                        int res = srv.run(address, port);

                        if (res == ERROR_NO_ERROR) {
                            AddLogMessage("Сервер запущен по адресу " + address + ":" + std::to_string(port));
                            SetWindowTextW(hStartButton, L"Остановить сервер");
                        } else {
                            AddLogMessage("Ошибка запуска сервера: " + std::to_string(res));
                            SetWindowTextW(hStartButton, L"Запустить сервер");
                        }
                    } else {
                        int res = srv.stop();

                        if (res == ERROR_NO_ERROR) {
                            AddLogMessage("Сервер остановлен");
                            SetWindowTextW(hStartButton, L"Запустить сервер");
                        } else {
                            AddLogMessage("Ошибка остановки сервера: " + std::to_string(res));
                            SetWindowTextW(hStartButton, L"Остановить сервер");
                        }
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

    srv.sigOnNewConnection.connect(&onNewConnection);
    srv.sigOnClosedConnection.connect(onClosedConnection);
    srv.sigOnDataReceiving.connect(onDataReceiving);

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
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return 1;

    CreateWindowW(L"BUTTON", L"Запустить сервер", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                  10, 10, 150, 30, hWnd, (HMENU)IDB_START_SERVER, hInstance, nullptr);

    CreateWindowW(L"BUTTON", L"Выход", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                  170, 10, 100, 30, hWnd, (HMENU)IDB_EXIT, hInstance, nullptr);

    hLogEdit = CreateWindowW(L"EDIT", L"",
                             WS_VISIBLE | WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY |
                             WS_BORDER,
                             10, 50, 360, 200, hWnd, (HMENU)IDC_LOG_EDIT, hInstance, nullptr);

    ResizeControls(hWnd);

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    strcpy(nid.szTip, "Phone Barcode Scanner");
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
