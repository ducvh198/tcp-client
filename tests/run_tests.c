#include <stdio.h>
#include <stdlib.h>
#include "test_framework.h"
#include "compat.h"

int g_tests_run = 0;
int g_tests_passed = 0;
int g_tests_failed = 0;

void run_cli_args_tests(void);
void run_socket_client_tests(void);

int main(void) {
    if (platform_init() != 0) {
        fprintf(stderr, "Failed to initialize platform networking.\n");
        return 1;
    }

    printf("=========================================\n");
    printf("Running TCP Client CLI Unit Test Suite\n");
    printf("=========================================\n\n");

    printf("--- CLI Argument Parser Tests ---\n");
    run_cli_args_tests();

    printf("\n--- Socket Engine Tests ---\n");
    run_socket_client_tests();

    printf("\n=========================================\n");
    printf("Test Results: %d Passed, %d Failed (Total: %d)\n",
           g_tests_passed, g_tests_failed, g_tests_run);
    printf("=========================================\n");

    platform_cleanup();
    return (g_tests_failed == 0) ? 0 : 1;
}
