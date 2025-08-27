#include "udp.h"
#include <iostream>
#include <cstring>

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

std::expected<UdpSocket::ReceiveBytes, std::string> UdpSocket::receiveFrom()
{
    constexpr size_t MAX_IPV4_UDP_SIZE = 65507;
    static char MAX_BUF[MAX_IPV4_UDP_SIZE];

    sockaddr_in receiveAddr;
    int receiveLength = sizeof(receiveAddr);
    ZeroMemory(&receiveAddr, sizeof(receiveAddr));

    int bytesIn = recvfrom(
        static_cast<SOCKET>(*this), 
        MAX_BUF, 
        sizeof(MAX_BUF),
        0,
        reinterpret_cast<sockaddr*>(&receiveAddr),
        &receiveLength
    );
    if(bytesIn == SOCKET_ERROR) {
        char buf[256];
        winsockErrorToStr(buf, sizeof(buf), WSAGetLastError());
        return std::unexpected(std::string(buf));
    }

    uint8_t* bytes = new uint8_t[bytesIn];
    memcpy(bytes, MAX_BUF, bytesIn);
    ReceiveBytes out{UdpTransportAddress(receiveAddr), bytes, bytesIn};
    return out;
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

std::expected<void, std::string> UdpSocket::bind(const UdpTransportAddress &addr)
{
    if(::bind(*this, (sockaddr*)&addr.addr_, sizeof(addr.addr_)) == SOCKET_ERROR) {
        char buf[256];
        winsockErrorToStr(buf, sizeof(buf), WSAGetLastError());
        return std::unexpected(std::string(buf));
    }
    return {};
}

#endif // win32

UdpTransportAddress::UdpTransportAddress(const char *ipv4Addr, unsigned short port)
{
    this->addr_.sin_family = AF_INET; // ipv4
    this->addr_.sin_port = htons(port); // convert little to big endian
    // https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-inet_pton
    if(inet_pton(AF_INET, ipv4Addr, &this->addr_.sin_addr) == -1) {
        try {
            char buf[256];
            winsockErrorToStr(buf, sizeof(buf), WSAGetLastError());
            std::cerr << "Failed to convert ipv4 address: " << ipv4Addr << "] " << buf << std::endl;
        } catch(...) {}
        std::terminate();
    }
}

UdpTransportAddress::UdpTransportAddress(unsigned short port)
{
    this->addr_.sin_addr.S_un.S_addr = ADDR_ANY; // just use any one?
    this->addr_.sin_family = AF_INET; // ipv4
    this->addr_.sin_port = htons(port); // convert little to big endian
}

std::string UdpTransportAddress::ipv4Address() const
{
    char clientIp[64];
    ZeroMemory(&clientIp, sizeof(clientIp));
    inet_ntop(AF_INET, &this->addr_.sin_addr, clientIp, sizeof(clientIp));
    return std::string(clientIp);
}

unsigned short UdpTransportAddress::port() const
{
    return this->addr_.sin_port;
}

UdpSocket::ReceiveBytes::~ReceiveBytes() noexcept
{
    if(this->bytes == nullptr) return;
    delete[] this->bytes;
    this->bytes = nullptr;
}

UdpSocket::ReceiveBytes::ReceiveBytes(ReceiveBytes &&other) noexcept
    : addr(other.addr), bytes(other.bytes), len(other.len)
{
    other.bytes = nullptr;
    other.len = 0;
}
