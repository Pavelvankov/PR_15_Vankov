
#define WIN32_LEAN_AND_MEAN
#include <iostream>

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512

CRITICAL_SECTION cs;

DWORD WINAPI ReceiveThread(LPVOID lpParam) {
    SOCKET connectSocket = (SOCKET)lpParam;

    int recvResult;
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    while (true) {
        recvResult = recv(connectSocket, recvbuf, recvbuflen, 0);

        EnterCriticalSection(&cs);

        if (recvResult > 0) {
            recvbuf[recvResult] = '\0';
            cout << "\r" << recvbuf << "> ";
        }
        else if (recvResult == 0) {
            cout << "[SERVER] Соединение закрыто" << endl;
            LeaveCriticalSection(&cs);
            break;
        }
        else {
            cout << "[SERVER] Ошибка recv: " << WSAGetLastError() << endl;
            LeaveCriticalSection(&cs);
            break;
        }

        LeaveCriticalSection(&cs);
    }

    return 0;
}

int InitConnection(WSADATA* wsaData, int result, SOCKET* connectSocket) {
    result = WSAStartup(MAKEWORD(2, 2), wsaData);
    if (result != 0) {
        cout << "Ошибка WSAStartup: " << result << endl;
        return 1;
    }

    struct addrinfo* addrResult = NULL;
    struct addrinfo* ptr = NULL;
    struct addrinfo hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char ipaddress[256];
    cout << "Введите IP SERVER: ";
    cin >> ipaddress;
    cin.ignore();

    result = getaddrinfo(ipaddress, DEFAULT_PORT, &hints, &addrResult);
    if (result != 0) {
        cout << "Ошибка getaddrinfo: " << result << endl;
        WSACleanup();
        return 1;
    }

    *connectSocket = INVALID_SOCKET;

    ptr = addrResult;
    *connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

    if (*connectSocket == INVALID_SOCKET) {
        cout << "Ошибка socket: " << WSAGetLastError() << endl;
        freeaddrinfo(addrResult);
        WSACleanup();
        return 1;
    }

    result = connect(*connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (result == SOCKET_ERROR) {
        closesocket(*connectSocket);
        *connectSocket = INVALID_SOCKET;
    }

    freeaddrinfo(addrResult);

    if (*connectSocket == INVALID_SOCKET) {
        cout << "Не удалось подключиться к SERVERу!" << endl;
        WSACleanup();
        return 1;
    }

    return 0;
}

int main() {
    setlocale(LC_ALL, "russian");

    WSADATA wsaData;
    int result = 0;
    SOCKET connectSocket;

    InitializeCriticalSection(&cs);

    int check = InitConnection(&wsaData, result, &connectSocket);
    if (check != 0) return 1;

    char nickname[256];
    cout << "Введите ваше имя: ";
    cin >> nickname;
    cin.ignore();

    result = send(connectSocket, nickname, (int)strlen(nickname), 0);
    if (result == SOCKET_ERROR) {
        cout << "Ошибка send: " << WSAGetLastError() << endl;
        closesocket(connectSocket);
        return 1;
    }

    HANDLE hThread;
    DWORD threadId;

    hThread = CreateThread(NULL, 0, ReceiveThread, (LPVOID)connectSocket, 0, &threadId);
    if (hThread == NULL)
        return GetLastError();

    while (true) {
        char sendbuf[256];
        cout << "> ";
        cin.getline(sendbuf, 256);

        if (sendbuf[0] == '/') {
            if (strcmp(sendbuf, "/exit") != 0 && strcmp(sendbuf, "/users") != 0) {
                cout << "Неизвестная команда" << endl;
                continue;
            }
        }

        result = send(connectSocket, sendbuf, (int)strlen(sendbuf), 0);
        if (result == SOCKET_ERROR) {
            cout << "Ошибка send: " << WSAGetLastError() << endl;
            closesocket(connectSocket);
            break;
        }

        if (strcmp(sendbuf, "/exit") == 0) {
            break;
        }
    }

    result = shutdown(connectSocket, SD_SEND);
    if (result == SOCKET_ERROR) {
        cout << "Ошибка shutdown: " << WSAGetLastError() << endl;
        closesocket(connectSocket);
        WSACleanup();
        return 1;
    }

    DeleteCriticalSection(&cs);
    CloseHandle(hThread);
    closesocket(connectSocket);
    WSACleanup();

    return 0;
}