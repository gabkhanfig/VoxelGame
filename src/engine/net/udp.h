#pragma once

#include <cstdint>

#if defined(_WIN32)
#include <WinSock2.h>
#endif

class UdpSocket {
public:
    static UdpSocket create();

    UdpSocket(const UdpSocket&) = delete;

    UdpSocket(UdpSocket&& other) noexcept;

    UdpSocket& operator=(const UdpSocket&) = delete;

    UdpSocket& operator=(UdpSocket&& other) = delete;

    ~UdpSocket() noexcept;

    #if defined(_WIN32)
    operator SOCKET() const;
    #endif
private:
    UdpSocket() noexcept;
private:
    void* socket_;
};
