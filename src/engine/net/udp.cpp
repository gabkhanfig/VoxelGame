#include "udp.h"
#include <iostream>
#include <cstring>
#include <thread>
#include <optional>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>

#pragma comment (lib, "ws2_32.lib")

static_assert(sizeof(void*) == sizeof(SOCKET));
static_assert(alignof(void*) == alignof(SOCKET));

extern void winsockErrorToStr(char* out, size_t len, const int err);

#elif defined(__GNUC__) || defined(__clang__)
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#define SOCKET_ERROR (-1)
#endif // win32 / gnuc / clang

std::string errToStr(std::optional<int> optErr) {
    int err;
    if(optErr.has_value()) {
        err = optErr.value();
    } else {
        #if defined(_WIN32)
        err = WSAGetLastError();
        #elif defined(__GNUC__) || defined(__clang__)
        err = errno;
        #endif
    }


    #if defined(_WIN32)
    char buf[256];
    winsockErrorToStr(buf, sizeof(buf), WSAGetLastError());
    return std::string(buf);
    #elif defined(__GNUC__) || defined(__clang__)
    return std::string(strerror(err));
    #endif
}

UdpSocket UdpSocket::create() {
    return UdpSocket();
}

std::expected<UdpSocket::ReceiveBytes, std::string> UdpSocket::receiveFrom()
{
    auto previous = this->receiverInUse_.exchange(true);
    if(previous == true) {
        try {
            std::cerr << "Socket receiver already in use by another thread. Incorrectly called from thread " 
                << std::this_thread::get_id()
                << std::endl;
        } catch(...) {}
        std::terminate();
    }

    sockaddr_in receiveAddr;
    #if defined(_WIN32)
    int receiveLength = sizeof(receiveAddr);
    #elif defined(__GNUC__) || defined(__clang__)
    socklen_t receiveLength = sizeof(receiveAddr);
    #endif
    memset(&receiveAddr, 0, sizeof(receiveAddr));

    int bytesIn = recvfrom(
        *this, 
        this->maxBuf_, 
        MAX_IPV4_UDP_SIZE,
        0,
        reinterpret_cast<sockaddr*>(&receiveAddr),
        &receiveLength
    );
    if(bytesIn == SOCKET_ERROR) {
        return std::unexpected(errToStr(std::nullopt));
    }

    uint8_t* bytes = new uint8_t[bytesIn];
    memcpy(bytes, this->maxBuf_, bytesIn);
    ReceiveBytes out{UdpTransportAddress(receiveAddr), bytes, bytesIn};

    this->receiverInUse_.store(false);

    return out;
}

std::expected<void, std::string> UdpSocket::sendTo(const uint8_t *bytes, uint16_t len, const UdpTransportAddress &to)
{
    const int sendOk = sendto(
        *this,
        reinterpret_cast<const char*>(bytes),
        static_cast<int>(len),
        0,
        reinterpret_cast<const sockaddr*>(&to.addr_),
        sizeof(to.addr_)
    );
    if(sendOk == SOCKET_ERROR) {
        return std::unexpected(errToStr(std::nullopt));
    }
    return {};
}

UdpSocket::UdpSocket() noexcept
    : receiverInUse_(false)
{
    auto sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    #if defined(_WIN32)
    if(int err = WSAGetLastError(); err != 0) {
        try {
            std::cerr << "Failed to create socket: " << errToStr(err) << std::endl;
        } catch(...) {}
        std::terminate();
    }
    #elif defined(__GNUC__) || defined(__clang__)
    if(sock == -1) {
        try {
            std::cerr << "Failed to create socket: " << errToStr(std::nullopt) << std::endl;
        } catch(...) {}
        std::terminate();
    }
    #endif

    this->socket_ = std::bit_cast<decltype(this->socket_), decltype(sock)>(sock);
    this->maxBuf_ = new char[MAX_IPV4_UDP_SIZE];
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept
    : socket_(other.socket_), maxBuf_(other.maxBuf_)
{
    other.socket_ = reinterpret_cast<decltype(socket_)>(0);
    other.maxBuf_ = nullptr;
}

UdpSocket::~UdpSocket() noexcept
{
    if(this->socket_ == reinterpret_cast<decltype(socket_)>(0)) return;

    #if defined(_WIN32)
    SOCKET sock = std::bit_cast<SOCKET, void*>(this->socket_);
    const int result = closesocket(sock);
    #elif defined(__GNUC__) || defined(__clang__)
    int sock = this->socket_;
    const int result = close(sock);
    #endif 
    if(result == SOCKET_ERROR) {
        try {
            std::cerr << "Failed to close socket: " << errToStr(std::nullopt) << std::endl;
        } catch(...) {}      
        std::terminate();
    }

    delete[] this->maxBuf_;
    this->maxBuf_ = nullptr;
}

#if defined(_WIN32)
static_assert(sizeof(void*) == sizeof(uint64_t));
static_assert(alignof(void*) == alignof(uint64_t));

UdpSocket::operator SOCKET() const
{
    return std::bit_cast<SOCKET, void*>(this->socket_);
}
#endif

bool UdpSocket::readable(long timeoutMicroseconds) const
{ 
    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = timeoutMicroseconds;

    #if defined(_WIN32)
    SOCKET sock = *this;
    #elif defined(__GNUC__) || defined(__clang__)
    int sock = *this;
    #endif

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);

    const int retval = select(static_cast<int>(sock + 1), &rfds, nullptr, nullptr, &tv);
    if(retval == SOCKET_ERROR) {
        try {
            std::cerr << "Failed to check if socket is readable: " << errToStr(std::nullopt) << std::endl;
        } catch(...) {}      
        std::terminate();
    }
    if(retval == 0) {
        return false;
    }
    else {
        return true;
    }
}

bool UdpSocket::writeable(long timeoutMicroseconds) const
{
    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = timeoutMicroseconds;

    #if defined(_WIN32)
    SOCKET sock = *this;
    #elif defined(__GNUC__) || defined(__clang__)
    int sock = *this;
    #endif

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(sock, &wfds);

    const int retval = select(static_cast<int>(sock + 1), nullptr, &wfds, nullptr, &tv);
    if(retval == SOCKET_ERROR) {
        try {
            std::cerr << "Failed to check if socket is writeable: " << errToStr(std::nullopt) << std::endl;
        } catch(...) {}      
        std::terminate();
    }
    if(retval == 0) {
        return false;
    }
    else {
        return true;
    }
}

std::expected<void, std::string> UdpSocket::bind(const UdpTransportAddress &addr)
{
    if(::bind(*this, (sockaddr*)&addr.addr_, sizeof(addr.addr_)) == SOCKET_ERROR) {
        return std::unexpected(errToStr(std::nullopt));
    }
    return {};
}

UdpTransportAddress::UdpTransportAddress(const char *ipv4Addr, unsigned short port)
{
    this->addr_.sin_family = AF_INET; // ipv4
    this->addr_.sin_port = htons(port); // convert little to big endian
    // https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-inet_pton
    if(inet_pton(AF_INET, ipv4Addr, &this->addr_.sin_addr) == -1) {
        try {
            std::cerr << "Failed to convert ipv4 address: [" << ipv4Addr << "] " << errToStr(std::nullopt) << std::endl;
        } catch(...) {}
        std::terminate();
    }
}

UdpTransportAddress::UdpTransportAddress(unsigned short port)
{
    #if defined(_WIN32)
    this->addr_.sin_addr.S_un.S_addr = ADDR_ANY; // just use any one?
    #elif defined(__GNUC__) || defined(__clang__)
    this->addr_.sin_addr.s_addr = INADDR_ANY; // just use any one?
    #endif
    this->addr_.sin_family = AF_INET; // ipv4
    this->addr_.sin_port = htons(port); // convert little to big endian
}

std::string UdpTransportAddress::ipv4Address() const
{
    char clientIp[64];
    memset(&clientIp, 0, sizeof(clientIp));
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
