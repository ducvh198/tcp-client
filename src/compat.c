#include "compat.h"
#include <stdio.h>

#ifdef _WIN32
int platform_init(void) {
    WSADATA wsa;
    int res = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (res != 0) {
        fprintf(stderr, "Error: WSAStartup failed with code %d\n", res);
        return -1;
    }
    return 0;
}

void platform_cleanup(void) {
    WSACleanup();
}

int set_socket_nonblocking(int sockfd) {
    u_long mode = 1;
    return ioctlsocket(sockfd, FIONBIO, &mode);
}

int get_last_socket_error(void) {
    return WSAGetLastError();
}

bool is_socket_wouldblock(int err) {
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS;
}

bool is_socket_refused(int err) {
    return err == WSAECONNREFUSED || err == WSAEHOSTUNREACH || err == WSAENETUNREACH;
}

bool is_socket_timeout(int err) {
    return err == WSAETIMEDOUT;
}
#else
int platform_init(void) {
    return 0;
}

void platform_cleanup(void) {
}

int set_socket_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) flags = 0;
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

int get_last_socket_error(void) {
    return errno;
}

bool is_socket_wouldblock(int err) {
    return err == EAGAIN || err == EWOULDBLOCK || err == EINPROGRESS;
}

bool is_socket_refused(int err) {
    return err == ECONNREFUSED || err == EHOSTUNREACH || err == ENETUNREACH;
}

bool is_socket_timeout(int err) {
    return err == ETIMEDOUT;
}
#endif
