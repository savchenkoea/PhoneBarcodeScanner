#include <iostream>
#include <string>

#if defined(_WINDOWS) || defined(WINAPI_FAMILY)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "WSErrors.h"
#include "WSServerThread.h"

void help()
{
    std::cout << "\t1 - Run server" << std::endl;
    std::cout << "\t2 - Stop server" << std::endl;
    std::cout << "\t3 - Send message" << std::endl;
    std::cout << "\t4 - Close single connection" << std::endl;
    std::cout << "\t5 - Close all connections" << std::endl;
    std::cout << "\t6 - Display all current connections" << std::endl;
    std::cout << "\t9 - Exit" << std::endl;
    std::cout << "\tOther numbers - help" << std::endl;
}

void onNewConnection(int id, const std::string& ip, int port)
{
    std::cout << "New connection: " << id << " " << ip << ":" << port << std::endl;
}

void onClosedConnection(int id)
{
    std::cout << "Connection closed: " << id << std::endl;
}

void onDataReceiving(int id, const std::string& data)
{
    std::cout << "Data received from " << id << ": " << data << std::endl;

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

int main()
{
    short command(0);

    WSServerThread srv;
    srv.sigOnNewConnection.connect(&onNewConnection);
    srv.sigOnClosedConnection.connect(onClosedConnection);
    srv.sigOnDataReceiving.connect(onDataReceiving);


    int id;
    int res;

    help();

    do
    {
        std::cout << "Enter command (0-9): ";
        std::cin >> command;

        switch (command)
        {
        case 1:
            {
                std::string address = "192.168.1.20";
                int port = 10001;
                res = srv.run(address, port);

                if (res == ERROR_NO_ERROR) std::cout << "Server started on address "
                    << address << ":" << port << std::endl;
                else std::cout << "Error code: " << res << std::endl;
                break;
            }
        case 2:
            {
                res = srv.stop();

                if (res == ERROR_NO_ERROR) std::cout << "Server stopped" << std::endl;
                else std::cout << "Error code: " << res << std::endl;
                break;
            }
        case 3:
            {
                std::string mes;
                std::cout << "Enter thread id and message without spaces:";
                std::cin >> id >> mes;
                if (!mes.empty() && id != 0)
                {
                    res = srv.send(id, mes);
                    if (res == ERROR_NO_ERROR) std::cout << "Message sent" << std::endl;
                    else std::cout << "Error code: " << res << std::endl;
                }
                break;
            }
        case 4:
            {
                std::cout << "Enter thread id:";
                std::cin >> id;
                if (id != 0)
                {
                    res = srv.close(id);
                    if (res == ERROR_NO_ERROR) std::cout << "Thread closed" << std::endl;
                    else std::cout << "Error code: " << res << std::endl;
                }
                break;
            }
        case 5:
            {
                res = srv.closeAll();
                if (res == ERROR_NO_ERROR) std::cout << "All thread are closed" << std::endl;
                else std::cout << "Error code: " << res << std::endl;
                break;
            }
        case 6:
            {
                std::vector<WSConnectionInfo> connections_list;
                srv.getConnectionsList(connections_list);
                std::cout << "Active connections:" << std::endl;
                for (const auto& info : connections_list) std::cout << info.id << " " << info.ip << ":" << info.port << std::endl;
                break;
            }
        case 9: break;
        default:
            {
                help();
            }
        }

    } while (command != 9);

    res = srv.stop();
    if (res == ERROR_NO_ERROR) std::cout << "Server stopped." << std::endl;
    else {
        std::cout << "Error code: " << res << std::endl;
        return res;
    }

    return 0;
}