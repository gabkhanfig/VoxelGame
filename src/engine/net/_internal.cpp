#include "_internal.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <iostream>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

static void winsockErrorToStr(char* out, size_t len, const int err) {
    // https://stackoverflow.com/a/46104456
    (void)FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, // flags
                         NULL,                                                       // lpsource
                         err,                                                        // message id
                         MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),                  // languageid
                         out,                                                        // output buffer
                         static_cast<DWORD>(len),                                    // size of msgbuf, bytes
                         NULL);                                                      // va_list of arguments
}

// https://www.youtube.com/watch?v=uIanSvWou1M&list=PLI2Dn-ewQf7aSQ6A3etSPUO6SF9qGdilN

struct RAIIWinSock {
    RAIIWinSock();

    ~RAIIWinSock() noexcept;
};

namespace {
RAIIWinSock _globalWinSockCleanup{}; // forcibly call constructor
}

RAIIWinSock::RAIIWinSock() {
    WSADATA data;
    WORD version = MAKEWORD(2, 2); // winsock 2?
    int wsok = WSAStartup(version, &data);
    if (wsok != 0) {
        try {
            std::cerr << "Can't start Winsock! " << wsok << std::endl;
        } catch (...) {
        }
        std::terminate();
    }
}

RAIIWinSock::~RAIIWinSock() noexcept {
    if (WSACleanup() == SOCKET_ERROR) {
        // https://stackoverflow.com/a/46104456

        const int err = WSAGetLastError();

        char msgbuf[256]; // for a message up to 255 bytes.
        ZeroMemory(msgbuf, 256);

        winsockErrorToStr(msgbuf, sizeof(msgbuf), err);

        try {
            std::cerr << "Failed to cleanup WinSock: " << msgbuf << std::endl;
        } catch (...) {
        }
    }
}

#endif // win32

std::string net::errToStr(std::optional<int> optErr) {
    int err;
    if (optErr.has_value()) {
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
    winsockErrorToStr(buf, sizeof(buf), err);
    return std::string(buf);
#elif defined(__GNUC__) || defined(__clang__)
    return std::string(strerror(err));
#endif
}
