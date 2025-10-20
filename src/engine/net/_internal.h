#pragma once

#include <optional>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <iostream>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")
#elif defined(__GNUC__) || defined(__clang__)
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#define SOCKET_ERROR (-1)
#endif

namespace net {
std::string errToStr(std::optional<int> optErr);

}