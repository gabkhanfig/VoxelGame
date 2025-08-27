#include "engine/net/udp.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment (lib, "ws2_32.lib")
#endif

#include <string>
#include <thread>
#include <iostream>

int main(int argc, char* argv[]) {
    char a1[] = "a";
    char a2[] = "some string idk";
    char* buf[2] = {a1, a2};
    if(argc == 1) {
        argv = buf;
        argc = 2;
    }

    std::cout << "hi\n";

    UdpSocket sock = UdpSocket::create();
    
    std::cout << "hello\n";

    // create a hint structure for the server
    sockaddr_in server;
    server.sin_family = AF_INET; // ipv4
    server.sin_port = htons(54000); // convert little to big endian

    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    // write to it
    while(true) {
        char bytes[] = {'a', 'b', 'c', 0, 'e', 'f', 'g'};
        int sendOk = sendto(sock, bytes, static_cast<int>(sizeof(bytes) + 1), 0, (sockaddr*)&server, sizeof(server));
        if(sendOk == SOCKET_ERROR) {
            std::cerr << "Failed to send! " << WSAGetLastError() << std::endl;
        }

        Sleep(1000);
    }
}