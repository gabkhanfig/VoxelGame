#include "engine/net/udp.h"
#include "engine/net/tcp.h"
#include "engine/net/transport.h"

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
#include <thread>
#include <chrono>

using namespace net;

// int main(int argc, char* argv[]) {
//     char a1[] = "a";
//     char a2[] = "some string idk";
//     char* buf[2] = {a1, a2};
//     if(argc == 1) {
//         argv = buf;
//         argc = 2;
//     }

//     UdpSocket sock = UdpSocket::create();

//     TransportAddress server("127.0.0.1", 54000);

//     while(true) {
//         char bytes[] = {'a', 'b', 'c', 0, 'e', 'f', 'g'};
//         const auto result = sock.sendTo(
//             reinterpret_cast<const uint8_t*>(bytes), 
//             static_cast<uint16_t>(sizeof(bytes) + 1),
//             server
//         );
//         if(result.has_value() == false) {
//             std::cerr << "Failed to send: " << result.error() << std::endl;
//         }
//         using namespace std::chrono_literals;
//         std::this_thread::sleep_for(5000ms);
//     }
// }

int main(int argc, char* argv[]) {
    (void)argc;
    TcpSocket sock = TcpSocket::create();

    TransportAddress addr(argv[1], 80);

    sock.connect(addr);

    char msg[] = "GET / HTTP/1.1\r\n\r\n";
    size_t sendbytes = strlen(msg);

    if(
        auto sentRes = sock.send(reinterpret_cast<const uint8_t*>(msg), static_cast<uint16_t>(sendbytes));
        sentRes.has_value() == false
    ) {
        std::cerr << "Failed to send tcp bytes " << sentRes.error() << std::endl;
        std::terminate();
    }

    auto receiveResult = sock.receive();
    if(receiveResult.has_value()) {
        std::cout << reinterpret_cast<const char*>(receiveResult.value().bytes) << std::endl;
    } else {
        std::cerr << "Failed to receive tcp bytes " << receiveResult.error() << std::endl;
        std::terminate();
    }
}
