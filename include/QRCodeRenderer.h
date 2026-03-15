#ifndef QRCODE_RENDERER_H
#define QRCODE_RENDERER_H

#include <string>
#include <windows.h>

class QRCodeRenderer {
public:
    static void DrawQrCode(HWND hWnd, HDC hdc, const std::string& text);
};

#endif // QRCODE_RENDERER_H
