#include "portable_shell.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;

/* Test parameters */
static int test_int = 0;
static float test_float = 0.0f;
static char test_string[64] = "";
static bool test_bool = false;

/* Custom test command */
static int test_cmd_called = 0;
static int cmd_custom(PortableShell* shell, int argc, char** argv) {
    (void)shell;
    (void)argc;
    (void)argv;
    test_cmd_called++;
    ps_println(shell, "Custom command");
    return 0;
}

/* Helper to simulate typing a command */
static void type_command(PortableShell* shell, const char* cmd) {
    for (size_t i = 0; cmd[i]; i++) {
        ps_process_char(shell, cmd[i]);
    }
    ps_process_char(shell, '\n');
}

/* Test: Shell creation and destruction */
void test_shell_creation() {
    tests_run++;
    printf("Test: Shell creation and destruction... ");
    
    PortableShell* shell = ps_create(NULL);
    assert(shell != NULL);
    
    ps_destroy(shell);
    
    printf("PASSED\n");
    tests_passed++;
}

/* Test: Shell creation with custom config */
void test_shell_custom_config() {
    tests_run++;
    printf("Test: Shell with custom config... ");
    
    PSConfig config = {0};
    config.history_size = 5;
    config.vt100_enabled = false;
    config.prompt = "custom> ";
    
    PortableShell* shell = ps_create(&config);
    assert(shell != NULL);
    
    ps_destroy(shell);
    
    printf("PASSED\n");
    tests_passed++;
}

/* Test: Register and execute custom command */
void test_custom_command() {
    tests_run++;
    printf("Test: Custom command registration... ");
    
    PortableShell* shell = ps_create(NULL);
    assert(shell != NULL);
    
    test_cmd_called = 0;
    ps_register_command(shell, "custom", "Custom test command", cmd_custom);
    
    type_command(shell, "custom");
    assert(test_cmd_called == 1);
    
    ps_destroy(shell);
    
    printf("PASSED\n");
    tests_passed++;
}

/* Test: Parameter registration and get/set */
void test_parameters() {
    tests_run++;
    printf("Test: Parameter registration and operations... ");
    
    PortableShell* shell = ps_create(NULL);
    assert(shell != NULL);
    
    test_int = 10;
    test_float = 3.14f;
    strcpy(test_string, "hello");
    test_bool = true;
    
    ps_register_parameter(shell, "test_int", PS_PARAM_INT, &test_int, 0);
    ps_register_parameter(shell, "test_float", PS_PARAM_FLOAT, &test_float, 0);
    ps_register_parameter(shell, "test_string", PS_PARAM_STRING, test_string, sizeof(test_string));
    ps_register_parameter(shell, "test_bool", PS_PARAM_BOOL, &test_bool, 0);
    
    /* Test get */
    PSParameter* param = ps_get_parameter(shell, "test_int");
    assert(param != NULL);
    assert(*(int*)param->value_ptr == 10);
    
    /* Test set via command */
    type_command(shell, "set test_int 42");
    assert(test_int == 42);
    
    type_command(shell, "set test_float 2.71");
    assert(test_float > 2.7f && test_float < 2.72f);
    
    type_command(shell, "set test_string world");
    assert(strcmp(test_string, "world") == 0);
    
    type_command(shell, "set test_bool false");
    assert(test_bool == false);
    
    ps_destroy(shell);
    
    printf("PASSED\n");
    tests_passed++;
}

/* Test: Command buffer and editing */
void test_command_buffer() {
    tests_run++;
    printf("Test: Command buffer operations... ");
    
    PortableShell* shell = ps_create(NULL);
    assert(shell != NULL);
    
    /* Type some characters */
    ps_process_char(shell, 't');
    ps_process_char(shell, 'e');
    ps_process_char(shell, 's');
    ps_process_char(shell, 't');
    
    /* Backspace */
    ps_process_char(shell, PS_KEY_BACKSPACE);
    
    /* Add more */
    ps_process_char(shell, 't');
    
    /* Ctrl-C to cancel */
    ps_process_char(shell, PS_KEY_CTRL_C);
    
    ps_destroy(shell);
    
    printf("PASSED\n");
    tests_passed++;
}

/* Test: Built-in commands */
void test_builtin_commands() {
    tests_run++;
    printf("Test: Built-in commands... ");
    
    PortableShell* shell = ps_create(NULL);
    assert(shell != NULL);
    
    /* Test help command */
    type_command(shell, "help");
    
    /* Test clear command */
    type_command(shell, "clear");
    type_command(shell, "cls");
    
    ps_destroy(shell);
    
    printf("PASSED\n");
    tests_passed++;
}

/* Test: VT-100 functions */
void test_vt100() {
    tests_run++;
    printf("Test: VT-100 functions... ");
    
    PSConfig config = {0};
    config.vt100_enabled = true;
    
    PortableShell* shell = ps_create(&config);
    assert(shell != NULL);
    
    /* These should not crash */
    ps_vt100_clear_screen(shell);
    ps_vt100_clear_line(shell);
    ps_vt100_cursor_home(shell);
    ps_vt100_cursor_move(shell, 10);
    ps_vt100_cursor_left(shell, 1);
    ps_vt100_cursor_right(shell, 1);
    ps_vt100_color(shell, 31, 40);
    ps_vt100_reset(shell);
    ps_bell(shell);
    
    ps_destroy(shell);
    
    printf("PASSED\n");
    tests_passed++;
}

/* Test: Print functions */
void test_print_functions() {
    tests_run++;
    printf("Test: Print functions... ");
    
    PortableShell* shell = ps_create(NULL);
    assert(shell != NULL);
    
    ps_print(shell, "Test string");
    ps_println(shell, "Test line");
    ps_print_prompt(shell);
    
    ps_destroy(shell);
    
    printf("PASSED\n");
    tests_passed++;
}

/* Main test runner */
int main() {
    printf("========================================\n");
    printf("Portable Shell Library - Test Suite\n");
    printf("========================================\n\n");
    
    test_shell_creation();
    test_shell_custom_config();
    test_custom_command();
    test_parameters();
    test_command_buffer();
    test_builtin_commands();
    test_vt100();
    test_print_functions();
    
    printf("\n========================================\n");
    printf("Test Results: %d/%d passed\n", tests_passed, tests_run);
    printf("========================================\n");
    
    if (tests_passed == tests_run) {
        printf("✓ All tests passed!\n");
        return 0;
    } else {
        printf("✗ Some tests failed!\n");
        return 1;
    }
}
