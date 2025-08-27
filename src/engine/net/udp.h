#pragma once

#include <cstdint>
#include <expected>
#include <vector>
#include <string>
#include <atomic>

#if defined(_WIN32)
#include <WinSock2.h>
#elif defined(__clang__)
#include <sys/socket.h>
#include <netinet/in.h>
#elif defined(__GNUC__)
#include <sys/socket.h>
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

    /// @brief Creates a new UDP socket, allocating any necessary memory.
    /// @return The new UDP socket.
    ///
    /// https://man7.org/linux/man-pages/man2/socket.2.html
    static UdpSocket create();

    UdpSocket(const UdpSocket&) = delete;

    UdpSocket(UdpSocket&& other) noexcept;

    UdpSocket& operator=(const UdpSocket&) = delete;

    UdpSocket& operator=(UdpSocket&& other) = delete;

    ~UdpSocket() noexcept;

    #if defined(_WIN32)
    /// @return Get the socket as the raw socket handle for win32.
    operator SOCKET() const;
    #elif defined(__GNUC__) || defined(__clang__)
    /// @return Get the socket as the raw socket file descriptor for posix.
    operator int() const { return this->socket_; }
    #endif

    /// @brief Checks if this socket has some data to read from without
    /// blocking. Blocks execution on this thread until either the timeout 
    /// expires, or the socket status is ready.
    /// @param timeoutMicroseconds How long to block in microseconds.
    /// @return `true` if there is some data to read, `false` otherwise.
    ///
    /// https://man7.org/linux/man-pages/man2/select.2.html
    bool readable(long timeoutMicroseconds = 5) const;

    /// @brief Checks if this socket can be written to without 
    /// blocking. Blocks execution on this thread until either the timeout
    ///  expires, or the socket status is ready.
    /// @param timeoutMicroseconds How long to block in microseconds.
    /// @return `true` if writing can be done without blocking, `false` 
    /// otherwise.
    ///
    /// https://man7.org/linux/man-pages/man2/select.2.html
    bool writeable(long timeoutMicroseconds = 5) const;

    /// @brief Binds an address and port to the socket. It is required to
    /// assign a local address to the socket in order for it to receive
    /// connections.
    /// @param addr The address and port to bind to.
    /// @return Nothing on success, or a string indicating an error message.
    ///
    /// https://man7.org/linux/man-pages/man2/bind.2.html
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

    /// @brief Reads a packet in the form of bytes from the socket. This is a
    /// blocking operation. Use with `UdpSocket::readable()` to make
    /// non-blocking IO.
    /// @return Either the bytes and the received ipv4 address and port, or
    /// a string indicating an error message.
    ///
    /// https://linux.die.net/man/2/recvfrom
    ///
    /// # Fatal Error
    ///
    /// Only one thread may be receiving from `this` socket at a time.
    /// Should two threads try to receive from the same socket at the same
    /// time, the program will terminate.
    std::expected<ReceiveBytes, std::string> receiveFrom();

    /// @brief Writes a packet in the form of bytes to the socket. This is a
    /// blocking operation. Use with `UdpSocket::writeable()` to make
    /// non-blocking IO.
    /// @param bytes The bytes to write into the socket. Does not need to be 0 
    /// terminated.
    /// @param len The amount of bytes to write. See 
    /// `UdpSocket::MAX_SAFE_PAYLOAD_SIZE` and `UdpSocket::MAX_IPV4_UDP_SIZE`.
    /// @param to The destination address and port to send to.
    /// @return Nothing on success, or a string indicating an error message.
    ///
    /// https://linux.die.net/man/2/sendto
    std::expected<void, std::string> sendTo(const uint8_t* bytes, uint16_t len, const UdpTransportAddress& to);

private:
    UdpSocket() noexcept;
private:
    #if defined(_WIN32)
    void* socket_;
    #elif defined(__GNUC__) || defined(__clang__)
    int socket_;
    #endif
    char* maxBuf_;
    std::atomic<bool> receiverInUse_;
};

