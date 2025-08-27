#include <iostream>
#include "engine/net/udp.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment (lib, "ws2_32.lib")
#endif


// https://www.youtube.com/watch?v=uIanSvWou1M&list=PLI2Dn-ewQf7aSQ6A3etSPUO6SF9qGdilN

int main() {
    UdpSocket sock = UdpSocket::create();

    sockaddr_in serverHint;
    serverHint.sin_addr.S_un.S_addr = ADDR_ANY; // just use any one?
    serverHint.sin_family = AF_INET; // ipv4
    serverHint.sin_port = htons(54000); // convert little to big endian

    if(bind(sock, (sockaddr*)&serverHint, sizeof(serverHint)) == SOCKET_ERROR) {
        std::cerr << "Can't bind socket! " <<  WSAGetLastError() << std::endl;
        std::terminate();
    }

    sockaddr_in client;
    int clientLength = sizeof(client);
    ZeroMemory(&client, clientLength);

    char buf[1024];
    (void)buf;

    while(true) {
        ZeroMemory(&buf, sizeof(buf));

        // wait for message
        int bytesIn = recvfrom(sock, buf, 1024, 0, (sockaddr*)&client, &clientLength);
        if(bytesIn == SOCKET_ERROR) {
            std::cerr << "Error receiving from client! " <<  WSAGetLastError() << std::endl;
            continue;
        }

        // display message
        char clientIp[256];
        ZeroMemory(&clientIp, sizeof(clientIp));

        inet_ntop(AF_INET, &client.sin_addr, clientIp, sizeof(clientIp));

        std::cout << "Message recieved from " << clientIp << ':' << client.sin_port << " | " << buf << std::endl;
    }
}