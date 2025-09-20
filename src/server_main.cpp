// #include "engine/net/_internal.h"
// #include "engine/net/tcp.h"
// #include "engine/net/transport.h"
// #include "engine/net/udp.h"
// #include <iostream>

// #if defined(_WIN32)
// #define WIN32_LEAN_AND_MEAN
// #include <WS2tcpip.h>
// #include <WinSock2.h>
// #include <windows.h>

// #pragma comment(lib, "ws2_32.lib")
// #endif
// #include <chrono>
// #include <exception>
// #include <thread>

// using namespace net;

// int main() {
//     UdpSocket sock = UdpSocket::create();

//     TransportAddress serverHint(54000);
//     if(const auto result = sock.bind(serverHint); !result.has_value()) {
//         std::cerr << "Failed to bind socket: " << result.error() << std::endl;
//         std::terminate();
//     }

//     while(true) {
//         if(!sock.readable()) {
//             std::cout << "no data to read\n";
//             using namespace std::chrono_literals;
//             std::this_thread::sleep_for(5000ms);
//             continue;
//         }
//         else {
//             std::cout << "data to read!\n";
//             const auto received = sock.receiveFrom();
//             if(received.has_value()) {
//                 const ReceiveTransportBytes& success = received.value();
//                 std::cout << "Message recieved from "
//                     << success.addr.ipv4Address()
//                     << ':'
//                     << success.addr.port()
//                     << " | " << reinterpret_cast<const char*>(success.bytes) << std::endl;
//                 std::cout << "\tas bytes [";
//                 for(int i = 0; i < success.len; i++) {
//                     std::cout << (int)success.bytes[i];
//                     if(i != (success.len - 1)) {
//                         std::cout << ", ";
//                     } else {
//                         std::cout << "]";
//                     }
//                 }
//                 std::cout << std::endl;
//             } else {
//                 std::cerr << "Error receiving from client: " << received.error() << std::endl;
//                 continue;
//             }
//         }

//     }
// }

// int main() {
//     TcpSocket sock = TcpSocket::create();
//     TransportAddress listenAddr = TransportAddress::fromPortAnyAddress(54000);
//     auto bindResult = sock.bindAndListen(listenAddr, 10);
//     if(bindResult.has_value() == false) {
//         std::cerr << "Failed to bind and listen tcp socket: " << bindResult.error() << std::endl;
//         std::terminate();
//     }

//     while(true) {
//         std::cout << "Listening for connection on port: " << listenAddr.port() << std::endl;
//         auto acceptResult = sock.accept();
//         if(acceptResult.has_value() == false) {
//             std::cerr << "Failed to accept on socket: " << acceptResult.error() << std::endl;
//             std::terminate();
//         }

//         TcpSocket::AcceptedConnection& conn = acceptResult.value();
//         std::cout << "Accepted connection: " << conn.address().ipv4Address() << ':' << conn.address().port() <<
//         std::endl;

//         auto readResult = conn.read();
//         if(readResult.has_value() == false) {
//             std::cerr << "Failed to read: " << readResult.error() << std::endl;
//             std::terminate();
//         }
//         else {
//             auto& bytes = readResult.value();
//             std::cout << "read in:\n";
//             for(int i = 0; i < bytes.len; i++) {
//                 std::cout << static_cast<int>(bytes.bytes[i]) << ',';
//             }
//             std::cout << '\n';
//         }

//         const char* buf = "HTTP/1.0 200 OK\r\n\r\nHello";
//         auto writeResult = conn.write(reinterpret_cast<const uint8_t*>(buf), static_cast<uint16_t>(strlen(buf)));
//         if(writeResult.has_value() == false) {
//             std::cerr << "Failed to write to socket: " << writeResult.error() << std::endl;
//             std::terminate();
//         }
//     }
// }

#include <iostream>

int main() {}
