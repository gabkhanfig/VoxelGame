#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>

#pragma comment (lib, "ws2_32.lib")

void winsockErrorToStr(char* out, size_t len, const int err) {
    // https://stackoverflow.com/a/46104456
    (void)FormatMessageA (FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,   // flags
                    NULL,                // lpsource
                    err,                 // message id
                    MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),    // languageid
                    out,              // output buffer
                    static_cast<DWORD>(len),     // size of msgbuf, bytes
                    NULL);               // va_list of arguments
}

struct RAIIWinSock {
    RAIIWinSock();

    ~RAIIWinSock() noexcept;
};

namespace {
    RAIIWinSock _globalWinSockCleanup{}; // forcibly call constructor
}

RAIIWinSock::RAIIWinSock()
{
    WSADATA data;
    WORD version = MAKEWORD(2, 2); // winsock 2?
    int wsok = WSAStartup(version, &data);
    if(wsok != 0) {
        try {
            std::cerr << "Can't start Winsock! " << wsok << std::endl;
        } catch(...) {}
        std::terminate();
    }

    try {
        std::cout << "Started WinSock " << std::endl;
    } catch(...) {}
}

RAIIWinSock::~RAIIWinSock() noexcept
{
    if(WSACleanup() == SOCKET_ERROR) {
        // https://stackoverflow.com/a/46104456

        const int err = WSAGetLastError();

        char msgbuf [256];   // for a message up to 255 bytes.
        ZeroMemory(msgbuf, 256);

        winsockErrorToStr(msgbuf, sizeof(msgbuf), err);

        try {
            std::cerr << "Failed to cleanup WinSock: " << msgbuf << std::endl;
        } catch(...) {}
    }

    try {
        std::cout << "Cleaned up WinSock " << std::endl;
    } catch(...) {}
}



#endif
