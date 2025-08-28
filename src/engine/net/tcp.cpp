#include "tcp.h"
#include "_internal.h"
#include <thread>

using net::TcpSocket;
using net::ReceiveBytes;

TcpSocket::TcpSocket(TcpSocket &&other) noexcept
    : socket_(other.socket_), maxBuf_(other.maxBuf_)
{
    receiverInUse_.store(other.receiverInUse_.load());
    other.socket_ = 0;
    other.maxBuf_ = nullptr;
    other.receiverInUse_.store(false);
}

TcpSocket::~TcpSocket() noexcept
{
    if(this->socket_ == 0) return;

    #if defined(_WIN32)
    SOCKET sock = this->socket_;
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
    this->socket_ = 0;
    
    delete[] this->maxBuf_;
    this->maxBuf_ = nullptr;
}

std::expected<void, std::string> net::TcpSocket::connect(const TransportAddress& addr)
{
    if(::connect(*this, (sockaddr*)&addr.addr_, sizeof(addr.addr_)) == SOCKET_ERROR) {
        return std::unexpected(errToStr(std::nullopt));
    }
    return {};
}

std::expected<void, std::string> TcpSocket::bindAndListen(const TransportAddress &addr, int backlog)
{
    if(::bind(*this, (sockaddr*)&addr.addr_, sizeof(addr.addr_)) == SOCKET_ERROR) {
        return std::unexpected(errToStr(std::nullopt));
    }
    if(::listen(*this, backlog) == SOCKET_ERROR) {
        return std::unexpected(errToStr(std::nullopt));
    }
    return {};
}

std::expected<void, std::string> TcpSocket::send(const uint8_t *bytes, uint16_t len)
{
    const int sendOk = ::send(
        *this,
        reinterpret_cast<const char*>(bytes),
        static_cast<int>(len),
        0
    );
    if(sendOk == SOCKET_ERROR) {
        return std::unexpected(errToStr(std::nullopt));
    }
    return {};
}

std::expected<ReceiveBytes, std::string> TcpSocket::receive()
{
    this->setReceiverInUse();

    int bytesIn = recv(
        *this, 
        this->maxBuf_, 
        MAX_IPV4_UDP_SIZE,
        0
    );
    if(bytesIn == SOCKET_ERROR) {
        return std::unexpected(errToStr(std::nullopt));
    }

    uint8_t* bytes = new uint8_t[bytesIn];
    memcpy(bytes, this->maxBuf_, bytesIn);
    ReceiveBytes out{bytes, bytesIn};

    this->receiverInUse_.store(false);

    return out;
}

std::expected<TcpSocket::AcceptedConnection, std::string> TcpSocket::accept()
{
    sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    auto conn = ::accept(*this, reinterpret_cast<sockaddr*>(&addr), &addr_len);
    #if defined(_WIN32)
    if(conn == INVALID_SOCKET)
    #elif defined(__GNUC__) || defined(__clang__)
    if(conn == -1)
    #endif
    {
        return std::unexpected(errToStr(std::nullopt));
    }

    AcceptedConnection accepted(addr);
    accepted.socket_ = conn;
    accepted.ownedSocket_ = this;
    return accepted;
}

TcpSocket::TcpSocket() noexcept
    : receiverInUse_(false)
{
    auto sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    #if defined(_WIN32)
    if(int err = WSAGetLastError(); err != 0) {
        try {
            std::cerr << "Failed to create TCP socket: " << errToStr(err) << std::endl;
        } catch(...) {}
        std::terminate();
    }
    #elif defined(__GNUC__) || defined(__clang__)
    if(sock == -1) {
        try {
            std::cerr << "Failed to create TCP socket: " << errToStr(std::nullopt) << std::endl;
        } catch(...) {}
        std::terminate();
    }
    #endif

    this->socket_ = sock;
    this->maxBuf_ = new char[MAX_IPV4_UDP_SIZE];
}

void TcpSocket::setReceiverInUse()
{
    bool previous = this->receiverInUse_.exchange(true);
    if(previous == true) {
        try {
            std::cerr << "Socket receiver already in use by another thread. Incorrectly called from thread " 
                << std::this_thread::get_id()
                << std::endl;
        } catch(...) {}
        std::terminate();
    }
}

net::TcpSocket::AcceptedConnection::~AcceptedConnection() noexcept
{
    if(this->socket_ == 0) return;

    #if defined(_WIN32)
    SOCKET sock = this->socket_;
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
    this->socket_ = 0;
}

std::expected<ReceiveBytes, std::string> TcpSocket::AcceptedConnection::read()
{
    this->ownedSocket_->setReceiverInUse();

    int bytesIn = ::recv(
        this->socket_, 
        this->ownedSocket_->maxBuf_, 
        MAX_IPV4_UDP_SIZE,
        0
    );
    if(bytesIn == SOCKET_ERROR) {
        return std::unexpected(errToStr(std::nullopt));
    }

    uint8_t* bytes = new uint8_t[bytesIn];
    memcpy(bytes, this->ownedSocket_->maxBuf_, bytesIn);
    ReceiveBytes out{bytes, bytesIn};

    this->ownedSocket_->receiverInUse_.store(false);

    return out;
}

std::expected<void, std::string> TcpSocket::AcceptedConnection::write(const uint8_t* bytes, uint16_t len)
{
    const int sendOk = ::send(
        *this,
        reinterpret_cast<const char*>(bytes),
        static_cast<int>(len),
        0
    );
    if(sendOk == SOCKET_ERROR) {
        return std::unexpected(errToStr(std::nullopt));
    }
    return {};
}
