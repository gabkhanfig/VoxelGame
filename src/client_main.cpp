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

    UdpSocket sock = UdpSocket::create();

    UdpTransportAddress server("127.0.0.1", 54000);

    while(true) {
        char bytes[] = {'a', 'b', 'c', 0, 'e', 'f', 'g'};
        const auto result = sock.sendTo(
            reinterpret_cast<const uint8_t*>(bytes), 
            static_cast<uint16_t>(sizeof(bytes) + 1),
            server
        );
        if(result.has_value() == false) {
            std::cerr << "Failed to send: " << result.error() << std::endl;
        }
        Sleep(1000);
    }
}