#ifndef COMPAT_H
#define COMPAT_H

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <io.h>

  #define platform_socket_close(s) closesocket(s)
  #define SHUT_WR SD_SEND
  #ifndef STDIN_FILENO
  #define STDIN_FILENO 0
  #endif
  #ifndef STDOUT_FILENO
  #define STDOUT_FILENO 1
  #endif
  #ifndef STDERR_FILENO
  #define STDERR_FILENO 2
  #endif

  #ifndef MSG_NOSIGNAL
  #define MSG_NOSIGNAL 0
  #endif

  typedef WSAPOLLFD pollfd_t;
  #define poll_sockets(fds, nfds, timeout) WSAPoll(fds, nfds, timeout)
#else
  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/socket.h>
  #include <netdb.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <poll.h>
  #include <errno.h>

  #ifndef MSG_NOSIGNAL
  #define MSG_NOSIGNAL 0
  #endif

  #define platform_socket_close(s) close(s)
  typedef struct pollfd pollfd_t;
  #define poll_sockets(fds, nfds, timeout) poll(fds, (nfds_t)(nfds), timeout)
#endif

int platform_init(void);
void platform_cleanup(void);
int set_socket_nonblocking(int sockfd);
int get_last_socket_error(void);
bool is_socket_wouldblock(int err);
bool is_socket_refused(int err);
bool is_socket_timeout(int err);

#endif /* COMPAT_H */
