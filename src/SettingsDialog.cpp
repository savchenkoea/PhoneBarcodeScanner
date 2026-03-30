#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <SettingsDialog.h>
#include <SettingsManager.h>
#include <NetworkUtils.h>
#include <StringUtils.h>
#include <string>

static LRESULT CALLBACK SettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        {
            auto hInst = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(hDlg, GWLP_HINSTANCE));
            const auto& settings = SettingsManager::getInstance().getSettings();
            HWND hIp = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                       WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 120, 10, 150, 200,
                                       hDlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_IP_ADDRESS)), hInst, nullptr);

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

            CreateWindowExW(0, L"STATIC", L"IP адрес:", WS_CHILD | WS_VISIBLE, 10, 10, 100, 20, hDlg, nullptr, hInst, nullptr);
            CreateWindowExW(0, L"STATIC", L"Порт:", WS_CHILD | WS_VISIBLE, 10, 40, 100, 20, hDlg, nullptr, hInst, nullptr);
            HWND hPort = CreateWindowExA(0, "EDIT", std::to_string(settings.port).c_str(),
                                         WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER, 120, 40, 60, 20, hDlg,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PORT)), hInst, nullptr);
            SendMessage(hPort, EM_SETLIMITTEXT, 5, 0);

            auto fillCombo = [&](HWND hCombo, int selectedVal) {
                SendMessageW(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"<NONE>"));
                for (int i = 1; i < 128; ++i)
                {
                    wchar_t buf[64];
                    const wchar_t* name = L"";
                    switch (i) {
                        case 13: name = L"CR"; break;
                        case 10: name = L"LF"; break;
                        case 8:  name = L"BS"; break;
                        case 9:  name = L"TAB"; break;
                        case 27: name = L"ESC"; break;
                        case 32: name = L"Space"; break;
                        default: break;
                    }

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

            CreateWindowExW(0, L"STATIC", L"Префикс:", WS_CHILD | WS_VISIBLE, 10, 70, 100, 20, hDlg, nullptr, hInst, nullptr);
            HWND hPrefix = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                           WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 120, 70, 150, 200,
                                           hDlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_PREFIX)), hInst, nullptr);

            CreateWindowExW(0, L"STATIC", L"Постфикс 1:", WS_CHILD | WS_VISIBLE, 10, 100, 100, 20, hDlg, nullptr, hInst, nullptr);
            HWND hPostfix1 = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                             WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 120, 100, 150, 200,
                                             hDlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_POSTFIX1)), hInst, nullptr);

            CreateWindowExW(0, L"STATIC", L"Постфикс 2:", WS_CHILD | WS_VISIBLE, 10, 130, 100, 20, hDlg, nullptr, hInst, nullptr);
            HWND hPostfix2 = CreateWindowExW(0, L"COMBOBOX", nullptr,
                                             WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 120, 130, 150, 200,
                                             hDlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_POSTFIX2)), hInst, nullptr);

            fillCombo(hPrefix, settings.prefix);
            fillCombo(hPostfix1, settings.postfix1);
            fillCombo(hPostfix2, settings.postfix2);

            CreateWindowExW(0, L"BUTTON", L"Сохранить", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 100, 170, 100, 30,
                            hDlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDB_SAVE_SETTINGS)), hInst, nullptr);

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

void SettingsDialog::Show(HWND hWndParent, HINSTANCE hInst) {
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
