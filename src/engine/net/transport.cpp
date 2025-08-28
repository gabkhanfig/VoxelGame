#include "transport.h"
#include "_internal.h"

using net::TransportAddress;
using net::ReceiveBytes;
using net::ReceiveTransportBytes;

TransportAddress::TransportAddress(const char *ipv4Addr, unsigned short port)
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

TransportAddress::TransportAddress(unsigned short port)
{
    #if defined(_WIN32)
    this->addr_.sin_addr.S_un.S_addr = ADDR_ANY; // just use any one?
    #elif defined(__GNUC__) || defined(__clang__)
    this->addr_.sin_addr.s_addr = INADDR_ANY; // just use any one?
    #endif
    this->addr_.sin_family = AF_INET; // ipv4
    this->addr_.sin_port = htons(port); // convert little to big endian
}

std::string TransportAddress::ipv4Address() const
{
    char clientIp[64];
    memset(&clientIp, 0, sizeof(clientIp));
    inet_ntop(AF_INET, &this->addr_.sin_addr, clientIp, sizeof(clientIp));
    return std::string(clientIp);
}

unsigned short TransportAddress::port() const
{
    return ntohs(this->addr_.sin_port);
}

ReceiveBytes::~ReceiveBytes() noexcept
{
    if(this->bytes == nullptr) return;
    delete[] this->bytes;
    this->bytes = nullptr;
}

ReceiveBytes::ReceiveBytes(ReceiveBytes &&other) noexcept
    : bytes(other.bytes), len(other.len)
{
    other.bytes = nullptr;
    other.len = 0;
}
