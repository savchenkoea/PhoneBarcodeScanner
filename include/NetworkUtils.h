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
        PIP_ADAPTER_ADDRESSES pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
        if (pAddresses == nullptr) return addresses;

        ULONG dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &outBufLen);
        if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
            free(pAddresses);
            pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
            if (pAddresses == nullptr) return addresses;
            dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &outBufLen);
        }

        if (dwRetVal == NO_ERROR) {
            PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses;
            while (pCurrAddresses) {
                if (pCurrAddresses->OperStatus == IfOperStatusUp) {
                    PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
                    while (pUnicast) {
                        sockaddr_in* sa_in = (sockaddr_in*)pUnicast->Address.lpSockaddr;
                        char buf[INET_ADDRSTRLEN];
                        if (inet_ntop(AF_INET, &(sa_in->sin_addr), buf, sizeof(buf))) {
                            std::string addr = buf;
                            if (addr != "127.0.0.1") {
                                addresses.push_back(addr);
                            }
                        }
                        pUnicast = pUnicast->Next;
                    }
                }
                pCurrAddresses = pCurrAddresses->Next;
            }
        }

        if (pAddresses) free(pAddresses);
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

            if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
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
