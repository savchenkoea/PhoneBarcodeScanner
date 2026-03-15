#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>
#include "WSServerThread.h"

class CommandHandler {
public:
    static void HandleData(int id, const std::string& data, WSServerThread& srv);
};

#endif // COMMAND_HANDLER_H
