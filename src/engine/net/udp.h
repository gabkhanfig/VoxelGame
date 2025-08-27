#pragma once

#include <cstdint>
#include <expected>
#include <vector>
#include <string>
#include <atomic>

#if defined(_WIN32)
#include <WinSock2.h>
#endif

class UdpTransportAddress {
public:

    UdpTransportAddress(const char* ipv4Addr, unsigned short port);

    UdpTransportAddress(unsigned short port);

    UdpTransportAddress(sockaddr_in addr) : addr_(addr) {}

    std::string ipv4Address() const;

    unsigned short port() const;

    sockaddr_in addr_{};
};


class UdpSocket {
public:
    static constexpr size_t MAX_SAFE_PAYLOAD_SIZE = 508;
    static constexpr size_t MAX_IPV4_UDP_SIZE = 65507;

    static UdpSocket create();

    UdpSocket(const UdpSocket&) = delete;

    UdpSocket(UdpSocket&& other) noexcept;

    UdpSocket& operator=(const UdpSocket&) = delete;

    UdpSocket& operator=(UdpSocket&& other) = delete;

    ~UdpSocket() noexcept;

    #if defined(_WIN32)
    operator SOCKET() const;
    #endif

    bool readable(long timeoutMicroseconds = 5) const;

    bool writeable(long timeoutMicroseconds = 5) const;

    std::expected<void, std::string> bind(const UdpTransportAddress& addr);

    class ReceiveBytes {
    public:
        UdpTransportAddress addr;
        uint8_t* bytes;
        int len;

        ReceiveBytes(UdpTransportAddress inAddr, uint8_t* inBytes, int inLen)
            : addr(inAddr), bytes(inBytes), len(inLen)
        {}

        ~ReceiveBytes() noexcept;

        ReceiveBytes(ReceiveBytes&& other) noexcept;

        ReceiveBytes(const ReceiveBytes&) noexcept = delete;
        ReceiveBytes& operator=(const ReceiveBytes&) noexcept = delete;
        ReceiveBytes& operator=(ReceiveBytes&&) noexcept = delete;
    };

    std::expected<ReceiveBytes, std::string> receiveFrom();

    std::expected<void, std::string> sendTo(const uint8_t* bytes, uint16_t len, const UdpTransportAddress& to);

private:
    UdpSocket() noexcept;
private:
    void* socket_;
    char* maxBuf_;
    std::atomic<bool> receiverInUse_;
};

