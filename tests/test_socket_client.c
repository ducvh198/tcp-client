#include "test_framework.h"
#include "socket_client.h"

void test_socket_invalid_args(void) {
    ASSERT_EQ_INT(SOCKET_ERR_IO, socket_connect(NULL, 8080, 1000, false));
    ASSERT_EQ_INT(SOCKET_ERR_IO, socket_connect("", 8080, 1000, false));
    ASSERT_EQ_INT(SOCKET_ERR_IO, socket_connect("127.0.0.1", 0, 1000, false));
    ASSERT_EQ_INT(SOCKET_ERR_IO, socket_connect("127.0.0.1", 70000, 1000, false));
}

void test_socket_invalid_host_dns(void) {
    int res = socket_connect("nonexistent.invalid.domain.example", 80, 1000, false);
    ASSERT_EQ_INT(SOCKET_ERR_DNS, res);
}

void test_socket_connection_refused(void) {
    int res = socket_connect("127.0.0.1", 59999, 1000, false);
    ASSERT_EQ_INT(SOCKET_ERR_REFUSED, res);
}

void test_socket_connection_timeout(void) {
    int res = socket_connect("10.255.255.1", 80, 200, false);
    ASSERT_EQ_INT(SOCKET_ERR_TIMEOUT, res);
}

void test_socket_invalid_fd_io(void) {
    char buf[16];
    ASSERT_EQ_INT(SOCKET_ERR_IO, socket_write_all(-1, "test", 4, 1000));
    ASSERT_EQ_INT(SOCKET_ERR_IO, socket_read(-1, buf, sizeof(buf), 1000));
    ASSERT_EQ_INT(SOCKET_ERR_IO, socket_read_exact(-1, buf, sizeof(buf), 1000));
    ASSERT_EQ_INT(SOCKET_ERR_IO, socket_read_framed(-1, buf, sizeof(buf), 1000));
    ASSERT_EQ_INT(SOCKET_ERR_IO, socket_read_exact(1, NULL, 10, 1000));
    ASSERT_EQ_INT(SOCKET_ERR_IO, socket_read_framed(1, NULL, 10, 1000));
    ASSERT_EQ_INT(SOCKET_ERR_IO, socket_read_framed(1, buf, 1, 1000));
}

void run_socket_client_tests(void) {
    RUN_TEST(test_socket_invalid_args);
    RUN_TEST(test_socket_invalid_host_dns);
    RUN_TEST(test_socket_connection_refused);
    RUN_TEST(test_socket_connection_timeout);
    RUN_TEST(test_socket_invalid_fd_io);
}
