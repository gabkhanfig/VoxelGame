#include "udp.h"
#include <iostream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment (lib, "ws2_32.lib")

static_assert(sizeof(void*) == sizeof(SOCKET));
static_assert(alignof(void*) == alignof(SOCKET));

extern void winsockErrorToStr(char* out, size_t len, const int err);

UdpSocket UdpSocket::create() {
    return UdpSocket();
}

UdpSocket::UdpSocket() noexcept
{
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(int err = WSAGetLastError(); err != 0) {
        try {
            char buf[256];
            winsockErrorToStr(buf, sizeof(buf), err);
            std::cerr << "Failed to create socket: " << buf << std::endl;
        } catch(...) {}
        std::terminate();
    }

    this->socket_ = std::bit_cast<void*, SOCKET>(sock);
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
    : socket_(other.socket_)
{
    other.socket_ = nullptr;
}

UdpSocket::~UdpSocket() noexcept
{
    if(this->socket_ == nullptr) return;

    SOCKET sock = std::bit_cast<SOCKET, void*>(this->socket_);
    const int result = closesocket(sock);
    if(result == SOCKET_ERROR) {
        try {
            char buf[256];
            winsockErrorToStr(buf, sizeof(buf), WSAGetLastError());
            std::cerr << "Failed to close socket: " << buf << std::endl;
        } catch(...) {}      
        std::terminate();
    }
}

static_assert(sizeof(void*) == sizeof(uint64_t));
static_assert(alignof(void*) == alignof(uint64_t));

UdpSocket::operator SOCKET() const
{
    return std::bit_cast<SOCKET, void*>(this->socket_);
}

#endif // win32
