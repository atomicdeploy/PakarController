#ifndef PORTABLE_SHELL_H
#define PORTABLE_SHELL_H

#include <stdint.h>
#include <stddef.h>

/* Arduino compatibility */
#ifdef ARDUINO
    /* Arduino has its own boolean type */
    #ifndef bool
        #define bool boolean
    #endif
    #ifndef true
        #define true HIGH
    #endif
    #ifndef false
        #define false LOW
    #endif
#else
    #include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration constants */
#define PS_MAX_CMD_LENGTH 256
#define PS_MAX_ARGS 16
#define PS_PROMPT_MAX_LENGTH 32

/* Forward declarations */
typedef struct PortableShell PortableShell;
typedef struct PSCommand PSCommand;
typedef struct PSParameter PSParameter;

/* I/O callback types */
typedef int (*PSReadFunc)(void* ctx);
typedef int (*PSWriteFunc)(void* ctx, char c);
typedef int (*PSWriteStrFunc)(void* ctx, const char* str);

/* Command callback type */
typedef int (*PSCommandFunc)(PortableShell* shell, int argc, char** argv);

/* Parameter types */
typedef enum {
    PS_PARAM_INT,
    PS_PARAM_FLOAT,
    PS_PARAM_STRING,
    PS_PARAM_BOOL
} PSParamType;

/* Parameter structure */
struct PSParameter {
    const char* name;
    PSParamType type;
    void* value_ptr;
    size_t max_size;  /* For strings */
    PSParameter* next;
};

/* Command structure */
struct PSCommand {
    const char* name;
    const char* help;
    PSCommandFunc func;
    PSCommand* next;
};

/* Shell configuration */
typedef struct {
    PSReadFunc read_char;
    PSWriteFunc write_char;
    PSWriteStrFunc write_str;
    void* io_ctx;
    uint16_t history_size;
    bool vt100_enabled;
    const char* prompt;
} PSConfig;

/* Shell instance */
struct PortableShell {
    PSConfig config;
    
    /* Input buffer */
    char cmd_buffer[PS_MAX_CMD_LENGTH];
    uint16_t cmd_pos;
    uint16_t cmd_len;
    
    /* History */
    char** history;
    uint16_t history_size;
    uint16_t history_count;
    uint16_t history_pos;
    
    /* Commands */
    PSCommand* commands;
    
    /* Parameters */
    PSParameter* parameters;
    
    /* State */
    bool running;
    bool tui_mode;
};

/* Core API */
PortableShell* ps_create(PSConfig* config);
void ps_destroy(PortableShell* shell);
void ps_run(PortableShell* shell);
void ps_stop(PortableShell* shell);
int ps_process_char(PortableShell* shell, char c);

/* Command API */
int ps_register_command(PortableShell* shell, const char* name, const char* help, PSCommandFunc func);
int ps_unregister_command(PortableShell* shell, const char* name);

/* Parameter API */
int ps_register_parameter(PortableShell* shell, const char* name, PSParamType type, void* value_ptr, size_t max_size);
int ps_unregister_parameter(PortableShell* shell, const char* name);
PSParameter* ps_get_parameter(PortableShell* shell, const char* name);

/* Built-in commands */
int ps_cmd_help(PortableShell* shell, int argc, char** argv);
int ps_cmd_clear(PortableShell* shell, int argc, char** argv);
int ps_cmd_get(PortableShell* shell, int argc, char** argv);
int ps_cmd_set(PortableShell* shell, int argc, char** argv);
int ps_cmd_edit(PortableShell* shell, int argc, char** argv);

/* Utility functions */
void ps_print(PortableShell* shell, const char* str);
void ps_println(PortableShell* shell, const char* str);
void ps_print_prompt(PortableShell* shell);

/* VT-100/ANSI functions */
void ps_vt100_clear_screen(PortableShell* shell);
void ps_vt100_clear_line(PortableShell* shell);
void ps_vt100_cursor_home(PortableShell* shell);
void ps_vt100_cursor_move(PortableShell* shell, int col);
void ps_vt100_cursor_left(PortableShell* shell, int n);
void ps_vt100_cursor_right(PortableShell* shell, int n);
void ps_vt100_color(PortableShell* shell, int fg, int bg);
void ps_vt100_reset(PortableShell* shell);
void ps_bell(PortableShell* shell);

/* Key codes */
#define PS_KEY_CTRL_C 0x03
#define PS_KEY_CTRL_L 0x0C
#define PS_KEY_BACKSPACE 0x7F
#define PS_KEY_DELETE 0x7E
#define PS_KEY_ENTER 0x0D
#define PS_KEY_TAB 0x09
#define PS_KEY_ESC 0x1B

#ifdef __cplusplus
}
#endif

#endif /* PORTABLE_SHELL_H */
