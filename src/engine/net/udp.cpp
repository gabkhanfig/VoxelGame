#include "udp.h"
#include "_internal.h"
#include <cstring>
#include <iostream>
#include <optional>
#include <thread>

using net::ReceiveTransportBytes;
using net::TransportAddress;
using net::UdpSocket;

UdpSocket UdpSocket::create() { return UdpSocket(); }

std::expected<ReceiveTransportBytes, std::string> UdpSocket::receiveFrom() {
    auto previous = this->receiverInUse_.exchange(true);
    if (previous == true) {
        try {
            std::cerr << "Socket receiver already in use by another thread. Incorrectly called from thread "
                      << std::this_thread::get_id() << std::endl;
        } catch (...) {
        }
        std::terminate();
    }

    sockaddr_in receiveAddr;
#if defined(_WIN32)
    int receiveLength = sizeof(receiveAddr);
#elif defined(__GNUC__) || defined(__clang__)
    socklen_t receiveLength = sizeof(receiveAddr);
#endif
    memset(&receiveAddr, 0, sizeof(receiveAddr));

    int bytesIn =
        recvfrom(*this, this->maxBuf_, MAX_IPV4_UDP_SIZE, 0, reinterpret_cast<sockaddr*>(&receiveAddr), &receiveLength);
    if (bytesIn == SOCKET_ERROR) {
        return std::unexpected(errToStr(std::nullopt));
    }

    uint8_t* bytes = new uint8_t[bytesIn];
    memcpy(bytes, this->maxBuf_, bytesIn);
    ReceiveTransportBytes out{TransportAddress(receiveAddr), bytes, bytesIn};

    this->receiverInUse_.store(false);

    return out;
}

std::expected<void, std::string> UdpSocket::sendTo(const uint8_t* bytes, uint16_t len, const TransportAddress& to) {
    const int sendOk = sendto(*this, reinterpret_cast<const char*>(bytes), static_cast<int>(len), 0,
                              reinterpret_cast<const sockaddr*>(&to.addr_), sizeof(to.addr_));
    if (sendOk == SOCKET_ERROR) {
        return std::unexpected(errToStr(std::nullopt));
    }
    return {};
}

UdpSocket::UdpSocket() noexcept : receiverInUse_(false) {
    auto sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#if defined(_WIN32)
    if (int err = WSAGetLastError(); err != 0) {
        try {
            std::cerr << "Failed to create UDP socket: " << errToStr(err) << std::endl;
        } catch (...) {
        }
        std::terminate();
    }
#elif defined(__GNUC__) || defined(__clang__)
    if (sock == -1) {
        try {
            std::cerr << "Failed to create UDP socket: " << errToStr(std::nullopt) << std::endl;
        } catch (...) {
        }
        std::terminate();
    }
#endif

    this->socket_ = sock;
    this->maxBuf_ = new char[MAX_IPV4_UDP_SIZE];
}

UdpSocket::UdpSocket(UdpSocket&& other) noexcept : socket_(other.socket_), maxBuf_(other.maxBuf_) {
    receiverInUse_.store(other.receiverInUse_.load());
    other.socket_ = 0;
    other.maxBuf_ = nullptr;
    other.receiverInUse_.store(false);
}

UdpSocket::~UdpSocket() noexcept {
    if (this->socket_ == 0)
        return;

#if defined(_WIN32)
    SOCKET sock = this->socket_;
    const int result = closesocket(sock);
#elif defined(__GNUC__) || defined(__clang__)
    int sock = this->socket_;
    const int result = close(sock);
#endif
    if (result == SOCKET_ERROR) {
        try {
            std::cerr << "Failed to close socket: " << errToStr(std::nullopt) << std::endl;
        } catch (...) {
        }
        std::terminate();
    }
    this->socket_ = 0;

    delete[] this->maxBuf_;
    this->maxBuf_ = nullptr;
}

bool UdpSocket::readable(long timeoutMicroseconds) const {
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
    if (retval == SOCKET_ERROR) {
        try {
            std::cerr << "Failed to check if socket is readable: " << errToStr(std::nullopt) << std::endl;
        } catch (...) {
        }
        std::terminate();
    }
    if (retval == 0) {
        return false;
    } else {
        return true;
    }
}

bool UdpSocket::writeable(long timeoutMicroseconds) const {
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
    if (retval == SOCKET_ERROR) {
        try {
            std::cerr << "Failed to check if socket is writeable: " << errToStr(std::nullopt) << std::endl;
        } catch (...) {
        }
        std::terminate();
    }
    if (retval == 0) {
        return false;
    } else {
        return true;
    }
}

std::expected<void, std::string> UdpSocket::bind(const TransportAddress& addr) {
    if (::bind(*this, (sockaddr*)&addr.addr_, sizeof(addr.addr_)) == SOCKET_ERROR) {
        return std::unexpected(errToStr(std::nullopt));
    }
    return {};
}

#ifndef NO_TESTS

#include <doctest.h>

TEST_SUITE("UDP") {
    TEST_CASE("create socket") { UdpSocket sock = UdpSocket::create(); }

    TEST_CASE("bind socket") {
        UdpSocket sock = UdpSocket::create();
        TransportAddress serverHint(54000);
        const auto result = sock.bind(serverHint);
        REQUIRE(result.has_value());
    }
}

#endif