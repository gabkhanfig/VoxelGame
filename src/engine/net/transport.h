#pragma once

#if defined(_WIN32)
#include <WinSock2.h>
#elif defined(__clang__)
#include <sys/socket.h>
#include <netinet/in.h>
#elif defined(__GNUC__)
#include <sys/socket.h>
#endif
#include <string>

namespace net {
    static constexpr size_t MAX_SAFE_PAYLOAD_SIZE = 508;
    static constexpr size_t MAX_IPV4_UDP_SIZE = 65507;

    class TransportAddress {
    public:

        static TransportAddress fromIpv4AndPort(const char* ipv4Addr, unsigned short port) {
            return TransportAddress(ipv4Addr, port);
        }

        static TransportAddress fromPortAnyAddress(unsigned short port) {
            return TransportAddress(port);
        }

        static TransportAddress fromRaw(sockaddr_in addr) {
            return TransportAddress(addr);
        }

        TransportAddress(const char* ipv4Addr, unsigned short port);

        TransportAddress(unsigned short port);

        TransportAddress(sockaddr_in addr) : addr_(addr) {}

        std::string ipv4Address() const;

        unsigned short port() const;

        sockaddr_in addr_{};
    };

    class ReceiveBytes {
    public:
        uint8_t* bytes;
        int len;

        ReceiveBytes(uint8_t* inBytes, int inLen)
            : bytes(inBytes), len(inLen)
        {}

        ~ReceiveBytes() noexcept;

        ReceiveBytes(ReceiveBytes&& other) noexcept;

        ReceiveBytes(const ReceiveBytes&) noexcept = delete;
        ReceiveBytes& operator=(const ReceiveBytes&) noexcept = delete;
        ReceiveBytes& operator=(ReceiveBytes&&) noexcept = delete;
    };

    class ReceiveTransportBytes : public ReceiveBytes {
    public:
        TransportAddress addr;
        uint8_t* bytes;
        int len;

        ReceiveTransportBytes(TransportAddress inAddr, uint8_t* inBytes, int inLen)
            : addr(inAddr), ReceiveBytes(inBytes, inLen)
        {}

        ~ReceiveTransportBytes() noexcept = default;

        ReceiveTransportBytes(ReceiveTransportBytes&& other) noexcept = default;

        ReceiveTransportBytes(const ReceiveTransportBytes&) noexcept = delete;
        ReceiveTransportBytes& operator=(const ReceiveTransportBytes&) noexcept = delete;
        ReceiveTransportBytes& operator=(ReceiveTransportBytes&&) noexcept = delete;
    };
}
