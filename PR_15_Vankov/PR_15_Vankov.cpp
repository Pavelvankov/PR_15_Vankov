

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#pragma comment(lib, "Ws2_32.lib")

using namespace std;

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512

vector<SOCKET> clientSockets;
vector<string> clientNames;

void SendToClients(SOCKET currentClient, string message) {
    int sendResult;

    for (int i = 0; i < clientSockets.size(); i++) {
        if (clientSockets[i] != currentClient) {
            sendResult = send(clientSockets[i], message.c_str(), (int)message.length(), 0);
            if (sendResult == SOCKET_ERROR) {
                cout << "Ошибка send: " << WSAGetLastError() << endl;
                closesocket(clientSockets[i]);
                break;
            }
        }
    }
}

DWORD WINAPI ClientThread(LPVOID lpParam) {
    SOCKET clientSocket = (SOCKET)lpParam;

    char recvbuf[DEFAULT_BUFLEN];
    int recvResult;
    int sendResult;
    int recvbuflen = DEFAULT_BUFLEN;
    char nickname[DEFAULT_BUFLEN];

    recvResult = recv(clientSocket, nickname, recvbuflen, 0);
    if (recvResult > 0) {
        nickname[recvResult] = '\0';

        clientNames.push_back((string)nickname);

        cout << "[SERVER] Пользователь \"" << nickname << "\" подключился" << endl;

        string message = "[SERVER] Пользователь \"" + (string)nickname + "\" подключился\n";
        SendToClients(clientSocket, message);

        do {
            recvResult = recv(clientSocket, recvbuf, recvbuflen, 0);

            if (recvResult > 0) {
                recvbuf[recvResult] = '\0';

                if ((string)recvbuf == "/users") {
                    cout << "[КОМАНДА] " << nickname << ": /users" << endl;

                    for (int i = 0; i < clientNames.size(); i++) {
                        message = "[SERVER] " + clientNames[i] + "\n";

                        sendResult = send(clientSocket, message.c_str(), (int)message.length(), 0);
                        if (sendResult == SOCKET_ERROR) {
                            cout << "Ошибка send: " << WSAGetLastError() << endl;
                            closesocket(clientSocket);
                            break;
                        }
                    }
                }
                else if ((string)recvbuf == "/exit") {
                    cout << "[КОМАНДА] " << nickname << ": /exit" << endl;
                }
                else {
                    cout << "[" << nickname << "]: " << recvbuf << endl;

                    message = "[" + (string)nickname + "]: " + recvbuf + "\n";
                    SendToClients(clientSocket, message);
                }
            }

        } while (recvResult > 0 && (string)recvbuf != "/exit");

        recvResult = shutdown(clientSocket, SD_SEND);
        if (recvResult == SOCKET_ERROR) {
            cout << "[SERVER] Ошибка shutdown для " << nickname << ": " << WSAGetLastError() << endl;
        }

        clientSockets.erase(remove(clientSockets.begin(), clientSockets.end(), clientSocket), clientSockets.end());
        clientNames.erase(remove(clientNames.begin(), clientNames.end(), (string)nickname), clientNames.end());

        cout << "[SERVER] Пользователь \"" << nickname << "\" вышел из чата" << endl;
        message = "[SERVER] Пользователь \"" + (string)nickname + "\" вышел из чата\n";

        SendToClients(clientSocket, message);
    }

    closesocket(clientSocket);
    return 0;
}

int main() {
    setlocale(LC_ALL, "russian");

    WSADATA wsaData;
    int result;

    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        cout << "Ошибка WSAStartup: " << result << endl;
        return 1;
    }

    struct addrinfo* addrResult = NULL;
    struct addrinfo hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    result = getaddrinfo(NULL, DEFAULT_PORT, &hints, &addrResult);
    if (result != 0) {
        cout << "Ошибка getaddrinfo: " << result << endl;
        WSACleanup();
        return 1;
    }

    SOCKET listenSocket = INVALID_SOCKET;

    listenSocket = socket(addrResult->ai_family, addrResult->ai_socktype, addrResult->ai_protocol);
    if (listenSocket == INVALID_SOCKET) {
        cout << "Ошибка socket: " << WSAGetLastError() << endl;
        freeaddrinfo(addrResult);
        WSACleanup();
        return 1;
    }

    result = bind(listenSocket, addrResult->ai_addr, (int)addrResult->ai_addrlen);
    if (result == SOCKET_ERROR) {
        cout << "Ошибка bind: " << WSAGetLastError() << endl;
        freeaddrinfo(addrResult);
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(addrResult);

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        cout << "Ошибка listen: " << WSAGetLastError() << endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    cout << "SERVER запущен" << endl;

    vector<HANDLE> threads;

    while (true) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            cout << "Ошибка accept: " << WSAGetLastError() << endl;
            closesocket(listenSocket);
            WSACleanup();
            return 1;
        }

        clientSockets.push_back(clientSocket);

        HANDLE hThread;
        DWORD threadId;

        hThread = CreateThread(NULL, 0, ClientThread, (LPVOID)clientSocket, 0, &threadId);
        if (hThread == NULL) {
            cout << "Ошибка CreateThread: " << GetLastError() << endl;
            closesocket(clientSocket);
            return 1;
        }

        threads.push_back(hThread);
    }

    closesocket(listenSocket);

    for (int i = 0; i < threads.size(); i++) {
        CloseHandle(threads[i]);
    }

    WSACleanup();
    return 0;
}