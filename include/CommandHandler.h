#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>
#include "WSServerThread.h"

extern HWND hMainWnd;
extern const int WM_MINIMIZE_TO_TRAY;

class CommandHandler {
public:
    static void HandleData(int id, const std::string& data, WSServerThread& srv);
};

#endif // COMMAND_HANDLER_H
