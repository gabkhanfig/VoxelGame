#include <iostream>
#include "engine/net/udp.h"
#include "engine/net/transport.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment (lib, "ws2_32.lib")
#endif
#include <exception>
#include <thread>
#include <chrono>

using namespace net;

int main() {
    UdpSocket sock = UdpSocket::create();

    TransportAddress serverHint(54000);
    if(const auto result = sock.bind(serverHint); !result.has_value()) {
        std::cerr << "Failed to bind socket: " << result.error() << std::endl;
        std::terminate();
    }

    while(true) {
        if(!sock.readable()) {
            std::cout << "no data to read\n";
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(5000ms);
            continue;
        }
        else {
            std::cout << "data to read!\n";
            const auto received = sock.receiveFrom();
            if(received.has_value()) {
                const ReceiveTransportBytes& success = received.value();
                std::cout << "Message recieved from " 
                    << success.addr.ipv4Address() 
                    << ':' 
                    << success.addr.port()
                    << " | " << reinterpret_cast<const char*>(success.bytes) << std::endl;
                std::cout << "\tas bytes [";
                for(int i = 0; i < success.len; i++) {
                    std::cout << (int)success.bytes[i];
                    if(i != (success.len - 1)) {
                        std::cout << ", ";
                    } else {
                        std::cout << "]";
                    }
                }
                std::cout << std::endl;
            } else {
                std::cerr << "Error receiving from client: " << received.error() << std::endl;
                continue;
            }
        }

    }
}