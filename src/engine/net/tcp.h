#pragma once

#include "transport.h"
#include <expected>
#include <string>
#include <cstdint>
#include <atomic>

#if defined(_WIN32)
#include <WinSock2.h>
#elif defined(__clang__)
#include <sys/socket.h>
#include <netinet/in.h>
#elif defined(__GNUC__)
#include <sys/socket.h>
#endif

namespace net {
    class TcpSocket {
    public:
        static TcpSocket create() { return TcpSocket(); }
        
        TcpSocket(const TcpSocket&) = delete;

        TcpSocket(TcpSocket&& other) noexcept;

        TcpSocket& operator=(const TcpSocket&) = delete;

        TcpSocket& operator=(TcpSocket&& other) = delete;

        ~TcpSocket() noexcept;

        #if defined(_WIN32)
        /// @return Get the socket as the raw socket handle for win32.
        operator SOCKET() const { return this->socket_; }
        #elif defined(__GNUC__) || defined(__clang__)
        /// @return Get the socket as the raw socket file descriptor for posix.
        operator int() const { return this->socket_; }
        #endif

        /// @brief Connects the socket to an address. Generally this is to a
        /// remote address.
        /// @param addr The address and port to bind to.
        /// @return Nothing on success, or a string indicating an error message.
        ///
        /// https://man7.org/linux/man-pages/man2/connect.2.html
        std::expected<void, std::string> connect(const TransportAddress& addr);

        /// @brief Binds an address and port to the socket. It is required to
        /// assign a local address to the socket in order for it to receive
        /// connections.
        /// @param addr The address and port to bind to.
        /// @return Nothing on success, or a string indicating an error message.
        ///
        /// https://man7.org/linux/man-pages/man2/bind.2.html
        /// https://man7.org/linux/man-pages/man2/listen.2.html
        std::expected<void, std::string> bindAndListen(const TransportAddress& addr, int backlog);

        /// @brief Writes a packet in the form of bytes to the socket. This is a
        /// blocking operation. Use with `TcpSocket::writeable()` to make
        /// non-blocking IO.
        /// @param bytes The bytes to write into the socket. Does not need to be 0 
        /// terminated.
        /// @param len The amount of bytes to write. See 
        /// `net::MAX_SAFE_PAYLOAD_SIZE` and `net::MAX_IPV4_UDP_SIZE`.
        /// @param to The destination address and port to send to.
        /// @return Nothing on success, or a string indicating an error message.
        ///
        /// https://linux.die.net/man/2/send
        std::expected<void, std::string> send(const uint8_t* bytes, uint16_t len);

        /// @brief Reads a packet in the form of bytes from the socket. This is a
        /// blocking operation. Use with `TcpSocket::readable()` to make
        /// non-blocking IO.
        /// @return Either the bytes and the received ipv4 address and port, or
        /// a string indicating an error message.
        ///
        /// https://linux.die.net/man/2/recv
        ///
        /// # Fatal Error
        ///
        /// Only one thread may be receiving from `this` socket at a time.
        /// Should two threads try to receive from the same socket at the same
        /// time, the program will terminate.
        std::expected<ReceiveBytes, std::string> receive();

        class AcceptedConnection {
        public:

            ~AcceptedConnection() noexcept;

            AcceptedConnection(AcceptedConnection&& other);

            AcceptedConnection(const AcceptedConnection& other) = delete;
            AcceptedConnection& operator=(const AcceptedConnection& other) = delete;
            AcceptedConnection& operator=(AcceptedConnection&& other) = delete;

            /// @brief Reads a packet in the form of bytes from the socket. This is a
            /// blocking operation. Use with `TcpSocket::readable()` to make
            /// non-blocking IO.
            /// @return Either the bytes and the received ipv4 address and port, or
            /// a string indicating an error message.
            ///
            /// https://linux.die.net/man/2/recv
            ///
            /// # Fatal Error
            ///
            /// Only one thread may be receiving from `this` socket at a time.
            /// Should two threads try to receive from the same socket at the same
            /// time, the program will terminate.
            std::expected<ReceiveBytes, std::string> read();

            /// @brief Writes a packet in the form of bytes to the socket. This is a
            /// blocking operation. Use with `TcpSocket::writeable()` to make
            /// non-blocking IO.
            /// @param bytes The bytes to write into the socket. Does not need to be 0 
            /// terminated.
            /// @param len The amount of bytes to write. See 
            /// `net::MAX_SAFE_PAYLOAD_SIZE` and `net::MAX_IPV4_UDP_SIZE`.
            /// @param to The destination address and port to send to.
            /// @return Nothing on success, or a string indicating an error message.
            ///
            /// https://linux.die.net/man/2/send
            std::expected<void, std::string> write(const uint8_t* bytes, uint16_t len);

            #if defined(_WIN32)
            /// @return Get the socket as the raw socket handle for win32.
            operator SOCKET() const { return this->socket_; }
            #elif defined(__GNUC__) || defined(__clang__)
            /// @return Get the socket as the raw socket file descriptor for posix.
            operator int() const { return this->socket_; }
            #endif
            
            TransportAddress address() const { return addr_; }

        private:

            friend class TcpSocket;

            AcceptedConnection(TransportAddress inAddr) 
                : addr_(inAddr)
            {}

            #if defined(_WIN32)
            SOCKET socket_{};
            #elif defined(__GNUC__) || defined(__clang__)
            int socket_{};
            #endif
            TransportAddress addr_;
            TcpSocket* ownedSocket_ = nullptr;
        };

        std::expected<AcceptedConnection, std::string> accept();

    private:

        TcpSocket() noexcept;

        void setReceiverInUse();

        friend class AcceptedConnection;
        // friend class RequestedConnection;

    private:

        #if defined(_WIN32)
        SOCKET socket_;
        #elif defined(__GNUC__) || defined(__clang__)
        int socket_;
        #endif  
        char* maxBuf_;
        std::atomic<bool> receiverInUse_;
    };
}