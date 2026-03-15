#pragma once

#include <string>
#include <vector>
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#pragma comment(lib, "IPHLPAPI.lib")

namespace NetworkUtils {
    inline std::vector<std::string> GetActiveIPv4Addresses() {
        std::vector<std::string> addresses;
        ULONG outBufLen = 15000;
        std::vector<BYTE> buffer;
        ULONG dwRetVal = ERROR_BUFFER_OVERFLOW;

        // Попробуем получить список адресов до 3-х раз в случае изменения размера буфера
        for (int i = 0; i < 3 && dwRetVal == ERROR_BUFFER_OVERFLOW; ++i) {
            buffer.resize(outBufLen);
            dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr,
                                            reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data()), &outBufLen);
        }

        if (dwRetVal == NO_ERROR) {
            auto pCurrAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
            while (pCurrAddresses) {
                if (pCurrAddresses->OperStatus == IfOperStatusUp) {
                    for (auto pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast; pUnicast = pUnicast->Next) {
                        if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                            auto sa_in = reinterpret_cast<sockaddr_in*>(pUnicast->Address.lpSockaddr);
                            char buf[INET_ADDRSTRLEN];
                            if (inet_ntop(AF_INET, &(sa_in->sin_addr), buf, sizeof(buf))) {
                                std::string addr = buf;
                                if (addr != "127.0.0.1") {
                                    addresses.push_back(addr);
                                }
                            }
                        }
                    }
                }
                pCurrAddresses = pCurrAddresses->Next;
            }
        }

        return addresses;
    }

    inline bool IsPortBusy(const std::string& ip, int port) {
        WSADATA wsaData;
        bool result = false;

        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return false;
        }

        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock != INVALID_SOCKET) {
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(static_cast<unsigned short>(port));
            inet_pton(AF_INET, ip.c_str(), &(addr.sin_addr));

            if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
                result = true;
                // auto err = WSAGetLastError();
                // if (err == WSAEADDRINUSE) {
                //}
            }
            closesocket(sock);
        }

        WSACleanup();
        return result;
    }
}
