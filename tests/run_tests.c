#include <stdio.h>
#include <stdlib.h>
#include "test_framework.h"

void run_cli_args_tests(void);
void run_socket_client_tests(void);

int main(void) {
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

    return (g_tests_failed == 0) ? 0 : 1;
}
