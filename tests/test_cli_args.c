#include "test_framework.h"
#include "cli_args.h"

void test_parse_positional_args(void) {
    char *argv[] = {"tcp-client", "127.0.0.1", "8080"};
    int argc = 3;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(0, rc);
    ASSERT_STREQ("127.0.0.1", config.host);
    ASSERT_EQ_INT(8080, config.port);
    ASSERT_EQ_INT(5000, config.timeout_ms);
    ASSERT_TRUE(!config.force_interactive);
    ASSERT_TRUE(!config.verbose);
}

void test_parse_flags_short(void) {
    char *argv[] = {"tcp-client", "-h", "localhost", "-p", "9000", "-t", "3000", "-i", "-v"};
    int argc = 9;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(0, rc);
    ASSERT_STREQ("localhost", config.host);
    ASSERT_EQ_INT(9000, config.port);
    ASSERT_EQ_INT(3000, config.timeout_ms);
    ASSERT_TRUE(config.force_interactive);
    ASSERT_TRUE(config.verbose);
    ASSERT_EQ_INT(MODE_INTERACTIVE, config.mode);
}

void test_parse_flags_long(void) {
    char *argv[] = {"tcp-client", "--host", "example.com", "--port", "443", "--timeout", "2000", "--interactive", "--verbose"};
    int argc = 9;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(0, rc);
    ASSERT_STREQ("example.com", config.host);
    ASSERT_EQ_INT(443, config.port);
    ASSERT_EQ_INT(2000, config.timeout_ms);
    ASSERT_TRUE(config.force_interactive);
    ASSERT_TRUE(config.verbose);
}

void test_invalid_port_range(void) {
    char *argv[] = {"tcp-client", "127.0.0.1", "70000"};
    int argc = 3;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(1, rc);
}

void test_invalid_port_zero(void) {
    char *argv[] = {"tcp-client", "127.0.0.1", "0"};
    int argc = 3;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(1, rc);
}

void test_invalid_port_alpha(void) {
    char *argv[] = {"tcp-client", "127.0.0.1", "abc"};
    int argc = 3;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(1, rc);
}

void test_invalid_timeout(void) {
    char *argv[] = {"tcp-client", "-t", "-500", "127.0.0.1", "8080"};
    int argc = 5;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(1, rc);
}

void test_missing_required_args(void) {
    char *argv1[] = {"tcp-client"};
    cli_config_t config1;
    ASSERT_EQ_INT(1, parse_cli_args(1, argv1, &config1));

    char *argv2[] = {"tcp-client", "127.0.0.1"};
    cli_config_t config2;
    ASSERT_EQ_INT(1, parse_cli_args(2, argv2, &config2));
}

void test_help_flag(void) {
    char *argv[] = {"tcp-client", "--help"};
    int argc = 2;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(0, rc);
    ASSERT_TRUE(config.show_help);
}

void test_version_flag(void) {
    char *argv[] = {"tcp-client", "-V"};
    int argc = 2;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(0, rc);
    ASSERT_TRUE(config.show_version);
}

void test_extra_positional(void) {
    char *argv[] = {"tcp-client", "127.0.0.1", "8080", "extra"};
    int argc = 4;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(1, rc);
}

void test_boundary_port_1(void) {
    char *argv[] = {"tcp-client", "127.0.0.1", "1"};
    int argc = 3;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(0, rc);
    ASSERT_EQ_INT(1, config.port);
}

void test_boundary_port_65535(void) {
    char *argv[] = {"tcp-client", "127.0.0.1", "65535"};
    int argc = 3;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(0, rc);
    ASSERT_EQ_INT(65535, config.port);
}

void test_boundary_port_65536(void) {
    char *argv[] = {"tcp-client", "127.0.0.1", "65536"};
    int argc = 3;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(1, rc);
}

void test_boundary_port_negative(void) {
    char *argv[] = {"tcp-client", "127.0.0.1", "-1"};
    int argc = 3;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(1, rc);
}

void test_boundary_port_float(void) {
    char *argv[] = {"tcp-client", "127.0.0.1", "12.34"};
    int argc = 3;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(1, rc);
}

void test_boundary_port_trailing_garbage(void) {
    char *argv[] = {"tcp-client", "127.0.0.1", "8080abc"};
    int argc = 3;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(1, rc);
}

void test_boundary_port_overflow(void) {
    char *argv[] = {"tcp-client", "127.0.0.1", "999999999999999999999999"};
    int argc = 3;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(1, rc);
}

void test_boundary_timeout_zero(void) {
    char *argv[] = {"tcp-client", "-t", "0", "127.0.0.1", "8080"};
    int argc = 5;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(0, rc);
    ASSERT_EQ_INT(0, config.timeout_ms);
}

void test_boundary_timeout_large(void) {
    char *argv[] = {"tcp-client", "-t", "99999", "127.0.0.1", "8080"};
    int argc = 5;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(0, rc);
    ASSERT_EQ_INT(99999, config.timeout_ms);
}

void test_boundary_timeout_negative(void) {
    char *argv[] = {"tcp-client", "-t", "-100", "127.0.0.1", "8080"};
    int argc = 5;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(1, rc);
}

void test_boundary_timeout_alpha(void) {
    char *argv[] = {"tcp-client", "-t", "invalid", "127.0.0.1", "8080"};
    int argc = 5;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(1, rc);
}

void test_boundary_timeout_trailing_garbage(void) {
    char *argv[] = {"tcp-client", "-t", "1000ms", "127.0.0.1", "8080"};
    int argc = 5;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(1, rc);
}

void test_boundary_timeout_overflow(void) {
    char *argv[] = {"tcp-client", "-t", "999999999999999999999999", "127.0.0.1", "8080"};
    int argc = 5;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(1, rc);
}

void test_mixed_flags_and_positionals(void) {
    char *argv1[] = {"tcp-client", "--host", "example.com", "8080"};
    cli_config_t config1;
    ASSERT_EQ_INT(0, parse_cli_args(4, argv1, &config1));
    ASSERT_STREQ("example.com", config1.host);
    ASSERT_EQ_INT(8080, config1.port);

    char *argv2[] = {"tcp-client", "--port", "9090", "localhost"};
    cli_config_t config2;
    ASSERT_EQ_INT(0, parse_cli_args(4, argv2, &config2));
    ASSERT_STREQ("localhost", config2.host);
    ASSERT_EQ_INT(9090, config2.port);
}

void test_unrecognized_option(void) {
    char *argv[] = {"tcp-client", "--unknown-flag", "127.0.0.1", "8080"};
    int argc = 4;
    cli_config_t config;

    int rc = parse_cli_args(argc, argv, &config);
    ASSERT_EQ_INT(1, rc);
}

void run_cli_args_tests(void) {
    RUN_TEST(test_parse_positional_args);
    RUN_TEST(test_parse_flags_short);
    RUN_TEST(test_parse_flags_long);
    RUN_TEST(test_invalid_port_range);
    RUN_TEST(test_invalid_port_zero);
    RUN_TEST(test_invalid_port_alpha);
    RUN_TEST(test_invalid_timeout);
    RUN_TEST(test_missing_required_args);
    RUN_TEST(test_help_flag);
    RUN_TEST(test_version_flag);
    RUN_TEST(test_extra_positional);
    RUN_TEST(test_boundary_port_1);
    RUN_TEST(test_boundary_port_65535);
    RUN_TEST(test_boundary_port_65536);
    RUN_TEST(test_boundary_port_negative);
    RUN_TEST(test_boundary_port_float);
    RUN_TEST(test_boundary_port_trailing_garbage);
    RUN_TEST(test_boundary_port_overflow);
    RUN_TEST(test_boundary_timeout_zero);
    RUN_TEST(test_boundary_timeout_large);
    RUN_TEST(test_boundary_timeout_negative);
    RUN_TEST(test_boundary_timeout_alpha);
    RUN_TEST(test_boundary_timeout_trailing_garbage);
    RUN_TEST(test_boundary_timeout_overflow);
    RUN_TEST(test_mixed_flags_and_positionals);
    RUN_TEST(test_unrecognized_option);
}
