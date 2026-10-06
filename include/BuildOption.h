#pragma once


#if defined(_WIN32) || defined(_WIN64)

    #define SOCKET_CLASS NormalSocket
    #define SOCKET_HEADER "NormalSocket.h"

#else

    #define SOCKET_CLASS LinuxSocket
    #define SOCKET_HEADER "LinuxSocket.h"

#endif

#define IOCP_CLASS NormalIOCP
#define IOCP_HEADER "NormalIOCP.h"