#pragma once

#include <string>
#include <optional>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iostream>

#pragma comment (lib, "ws2_32.lib")
#elif defined(__GNUC__) || defined(__clang__)
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#define SOCKET_ERROR (-1)
#endif

namespace net {
    std::string errToStr(std::optional<int> optErr);
}