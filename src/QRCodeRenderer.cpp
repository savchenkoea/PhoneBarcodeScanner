#include "QRCodeRenderer.h"
#include <qrencode.h>

void QRCodeRenderer::DrawQrCode(HWND hWnd, HDC hdc, const std::string& text) {
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    constexpr int qrMargin = 10;
    constexpr int qrDrawSize = 200;
    const int xStart = rcClient.right - qrDrawSize - qrMargin;
    int yStart = 50;

    LOGFONTW lf = {0};
    if (GetObjectW(GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf)) {
        lf.lfHeight = -14; // Увеличиваем размер (было около -12)
        lf.lfWeight = FW_NORMAL;
    }
    HFONT hFont = CreateFontIndirectW(&lf);
    auto hOldFont = static_cast<HFONT>(SelectObject(hdc, hFont));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));

    if (text.empty()) {
        const wchar_t* msg = L"Для начала работы нажмите кнопку \"Запустить сервер\"";
        RECT rcMsg = { xStart, yStart, xStart + qrDrawSize, yStart + 80 };
        DrawTextW(hdc, msg, -1, &rcMsg, DT_CENTER | DT_WORDBREAK | DT_TOP);

        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);
        return;
    }

    QRcode* qr = QRcode_encodeString(text.c_str(), 0, QR_ECLEVEL_H, QR_MODE_8, 1);
    if (!qr) {
        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);
        return;
    }

    try {
        // Рисуем пояснительный текст над QR-кодом
        const wchar_t* hintText = L"Отсканируйте QR код в приложении на смартфоне для подключения";
        RECT rcText = { xStart, yStart, xStart + qrDrawSize, yStart + 80 };
        
        DrawTextW(hdc, hintText, -1, &rcText, DT_CENTER | DT_WORDBREAK | DT_TOP);
        
        yStart = 130;
        
        // Quiet zone (свободная зона) вокруг QR-кода должна быть минимум 4 модуля
        constexpr int border = 4;
        const int modules = qr->width;
        const int totalModules = modules + border * 2;
        int dotSize = qrDrawSize / totalModules;
        if (dotSize < 1) dotSize = 1;

        const int actualSize = dotSize * totalModules;
        const int xOffset = xStart + (qrDrawSize - actualSize) / 2;
        const int yOffset = yStart + (qrDrawSize - actualSize) / 2;

        HBRUSH hBlack = CreateSolidBrush(RGB(0, 0, 0));
        HBRUSH hWhite = CreateSolidBrush(RGB(255, 255, 255));

        // Фон всей области QR
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
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}
