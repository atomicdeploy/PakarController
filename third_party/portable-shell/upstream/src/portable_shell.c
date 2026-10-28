#define _POSIX_C_SOURCE 200809L
#include "portable_shell.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Helper functions */
static void ps_redraw_line(PortableShell* shell);
static int ps_execute_command(PortableShell* shell);
static void ps_add_to_history(PortableShell* shell, const char* cmd);
static void ps_handle_escape_sequence(PortableShell* shell);
static char** ps_parse_args(const char* cmd, int* argc);
static void ps_free_args(char** argv, int argc);

/* Default I/O functions for standard input/output */
static int default_read_char(void* ctx) {
    (void)ctx;
    return getchar();
}

static int default_write_char(void* ctx, char c) {
    (void)ctx;
    return putchar(c);
}

static int default_write_str(void* ctx, const char* str) {
    (void)ctx;
    return fputs(str, stdout);
}

/* Create shell instance */
PortableShell* ps_create(PSConfig* config) {
    PortableShell* shell = (PortableShell*)malloc(sizeof(PortableShell));
    if (!shell) return NULL;
    
    memset(shell, 0, sizeof(PortableShell));
    
    /* Set configuration */
    if (config) {
        shell->config = *config;
        /* Fill in default I/O functions if not provided */
        if (!shell->config.read_char) {
            shell->config.read_char = default_read_char;
        }
        if (!shell->config.write_char) {
            shell->config.write_char = default_write_char;
        }
        if (!shell->config.write_str) {
            shell->config.write_str = default_write_str;
        }
        if (!shell->config.prompt) {
            shell->config.prompt = "$ ";
        }
    } else {
        /* Default configuration */
        shell->config.read_char = default_read_char;
        shell->config.write_char = default_write_char;
        shell->config.write_str = default_write_str;
        shell->config.io_ctx = NULL;
        shell->config.history_size = 10;
        shell->config.vt100_enabled = true;
        shell->config.prompt = "$ ";
    }
    
    /* Allocate history */
    shell->history_size = shell->config.history_size;
    if (shell->history_size > 0) {
        shell->history = (char**)calloc(shell->history_size, sizeof(char*));
        if (!shell->history) {
            free(shell);
            return NULL;
        }
    }
    
    /* Register built-in commands */
    ps_register_command(shell, "help", "Display available commands", ps_cmd_help);
    ps_register_command(shell, "clear", "Clear screen", ps_cmd_clear);
    ps_register_command(shell, "cls", "Clear screen", ps_cmd_clear);
    ps_register_command(shell, "get", "Get parameter value", ps_cmd_get);
    ps_register_command(shell, "set", "Set parameter value", ps_cmd_set);
    ps_register_command(shell, "edit", "Edit parameters in TUI mode", ps_cmd_edit);
    
    shell->running = false;
    shell->tui_mode = false;
    
    return shell;
}

/* Destroy shell instance */
void ps_destroy(PortableShell* shell) {
    if (!shell) return;
    
    /* Free history */
    if (shell->history) {
        for (uint16_t i = 0; i < shell->history_count; i++) {
            free(shell->history[i]);
        }
        free(shell->history);
    }
    
    /* Free commands */
    PSCommand* cmd = shell->commands;
    while (cmd) {
        PSCommand* next = cmd->next;
        free(cmd);
        cmd = next;
    }
    
    /* Free parameters */
    PSParameter* param = shell->parameters;
    while (param) {
        PSParameter* next = param->next;
        free(param);
        param = next;
    }
    
    free(shell);
}

/* Run shell main loop */
void ps_run(PortableShell* shell) {
    if (!shell) return;
    
    shell->running = true;
    ps_print_prompt(shell);
    
    while (shell->running) {
        int c = shell->config.read_char(shell->config.io_ctx);
        if (c < 0) break;
        
        ps_process_char(shell, (char)c);
    }
}

/* Stop shell */
void ps_stop(PortableShell* shell) {
    if (shell) {
        shell->running = false;
    }
}

/* Process a single character */
int ps_process_char(PortableShell* shell, char c) {
    if (!shell) return -1;
    
    /* Handle special keys */
    switch (c) {
        case PS_KEY_CTRL_C:
            /* Cancel current line */
            ps_println(shell, "^C");
            shell->cmd_len = 0;
            shell->cmd_pos = 0;
            shell->cmd_buffer[0] = '\0';
            ps_print_prompt(shell);
            return 0;
            
        case PS_KEY_CTRL_L:
            /* Clear screen */
            ps_vt100_clear_screen(shell);
            ps_print_prompt(shell);
            ps_print(shell, shell->cmd_buffer);
            return 0;
            
        case PS_KEY_ENTER:
        case '\n':
            /* Execute command */
            ps_println(shell, "");
            if (shell->cmd_len > 0) {
                shell->cmd_buffer[shell->cmd_len] = '\0';
                ps_add_to_history(shell, shell->cmd_buffer);
                ps_execute_command(shell);
                shell->cmd_len = 0;
                shell->cmd_pos = 0;
                shell->cmd_buffer[0] = '\0';
            }
            ps_print_prompt(shell);
            return 0;
            
        case PS_KEY_BACKSPACE:
        case 0x08:  /* Backspace */
            if (shell->cmd_pos > 0) {
                /* Remove character before cursor */
                memmove(&shell->cmd_buffer[shell->cmd_pos - 1],
                       &shell->cmd_buffer[shell->cmd_pos],
                       shell->cmd_len - shell->cmd_pos + 1);
                shell->cmd_pos--;
                shell->cmd_len--;
                ps_redraw_line(shell);
            }
            return 0;
            
        case PS_KEY_TAB:
            /* Tab completion - for now just beep */
            ps_bell(shell);
            return 0;
            
        case PS_KEY_ESC:
            /* Escape sequence - handle arrow keys, etc */
            ps_handle_escape_sequence(shell);
            return 0;
            
        default:
            /* Regular character */
            if (c >= 32 && c < 127) {
                if (shell->cmd_len < PS_MAX_CMD_LENGTH - 1) {
                    /* Insert character at cursor position */
                    if (shell->cmd_pos < shell->cmd_len) {
                        memmove(&shell->cmd_buffer[shell->cmd_pos + 1],
                               &shell->cmd_buffer[shell->cmd_pos],
                               shell->cmd_len - shell->cmd_pos);
                    }
                    shell->cmd_buffer[shell->cmd_pos] = c;
                    shell->cmd_pos++;
                    shell->cmd_len++;
                    shell->cmd_buffer[shell->cmd_len] = '\0';
                    ps_redraw_line(shell);
                }
            }
            return 0;
    }
}

/* Handle escape sequences (arrow keys, etc) */
static void ps_handle_escape_sequence(PortableShell* shell) {
    int c1 = shell->config.read_char(shell->config.io_ctx);
    if (c1 != '[') return;
    
    int c2 = shell->config.read_char(shell->config.io_ctx);
    
    switch (c2) {
        case 'A':  /* Up arrow - previous history */
            if (shell->history_count > 0 && shell->history_pos < shell->history_count) {
                shell->history_pos++;
                int idx = shell->history_count - shell->history_pos;
                if (shell->history[idx]) {
                    strncpy(shell->cmd_buffer, shell->history[idx], PS_MAX_CMD_LENGTH - 1);
                    shell->cmd_len = strlen(shell->cmd_buffer);
                    shell->cmd_pos = shell->cmd_len;
                    ps_redraw_line(shell);
                }
            }
            break;
            
        case 'B':  /* Down arrow - next history */
            if (shell->history_pos > 0) {
                shell->history_pos--;
                if (shell->history_pos == 0) {
                    shell->cmd_buffer[0] = '\0';
                    shell->cmd_len = 0;
                    shell->cmd_pos = 0;
                } else {
                    int idx = shell->history_count - shell->history_pos;
                    if (shell->history[idx]) {
                        strncpy(shell->cmd_buffer, shell->history[idx], PS_MAX_CMD_LENGTH - 1);
                        shell->cmd_len = strlen(shell->cmd_buffer);
                        shell->cmd_pos = shell->cmd_len;
                    }
                }
                ps_redraw_line(shell);
            }
            break;
            
        case 'C':  /* Right arrow */
            if (shell->cmd_pos < shell->cmd_len) {
                shell->cmd_pos++;
                ps_vt100_cursor_right(shell, 1);
            }
            break;
            
        case 'D':  /* Left arrow */
            if (shell->cmd_pos > 0) {
                shell->cmd_pos--;
                ps_vt100_cursor_left(shell, 1);
            }
            break;
            
        case 'H':  /* Home */
            shell->cmd_pos = 0;
            ps_vt100_cursor_move(shell, strlen(shell->config.prompt));
            break;
            
        case 'F':  /* End */
            shell->cmd_pos = shell->cmd_len;
            ps_vt100_cursor_move(shell, strlen(shell->config.prompt) + shell->cmd_len);
            break;
            
        case '3':  /* Delete */
            {
                int c3 = shell->config.read_char(shell->config.io_ctx);
                if (c3 == '~' && shell->cmd_pos < shell->cmd_len) {
                    memmove(&shell->cmd_buffer[shell->cmd_pos],
                           &shell->cmd_buffer[shell->cmd_pos + 1],
                           shell->cmd_len - shell->cmd_pos);
                    shell->cmd_len--;
                    ps_redraw_line(shell);
                }
            }
            break;
    }
}

/* Redraw current line */
static void ps_redraw_line(PortableShell* shell) {
    /* Move cursor to beginning of line */
    shell->config.write_char(shell->config.io_ctx, '\r');
    
    /* Clear line */
    ps_vt100_clear_line(shell);
    
    /* Print prompt and command */
    ps_print(shell, shell->config.prompt);
    ps_print(shell, shell->cmd_buffer);
    
    /* Move cursor to correct position */
    int target_col = strlen(shell->config.prompt) + shell->cmd_pos;
    ps_vt100_cursor_move(shell, target_col);
}

/* Add command to history */
static void ps_add_to_history(PortableShell* shell, const char* cmd) {
    if (!shell || !cmd || !shell->history || shell->history_size == 0) return;
    
    /* Don't add empty commands or duplicates */
    if (strlen(cmd) == 0) return;
    if (shell->history_count > 0 && 
        shell->history[shell->history_count - 1] &&
        strcmp(shell->history[shell->history_count - 1], cmd) == 0) {
        return;
    }
    
    /* Free oldest entry if history is full */
    if (shell->history_count >= shell->history_size) {
        free(shell->history[0]);
        memmove(shell->history, shell->history + 1, 
                (shell->history_size - 1) * sizeof(char*));
        shell->history_count--;
    }
    
    /* Add new entry */
    shell->history[shell->history_count] = strdup(cmd);
    shell->history_count++;
    shell->history_pos = 0;
}

/* Parse command into arguments */
static char** ps_parse_args(const char* cmd, int* argc) {
    char** argv = (char**)malloc(PS_MAX_ARGS * sizeof(char*));
    if (!argv) return NULL;
    
    *argc = 0;
    char* cmd_copy = strdup(cmd);
    if (!cmd_copy) {
        free(argv);
        return NULL;
    }
    
    char* token = strtok(cmd_copy, " \t");
    while (token && *argc < PS_MAX_ARGS) {
        argv[*argc] = strdup(token);
        (*argc)++;
        token = strtok(NULL, " \t");
    }
    
    free(cmd_copy);
    return argv;
}

/* Free parsed arguments */
static void ps_free_args(char** argv, int argc) {
    if (!argv) return;
    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
}

/* Execute command */
static int ps_execute_command(PortableShell* shell) {
    int argc;
    char** argv = ps_parse_args(shell->cmd_buffer, &argc);
    
    if (!argv || argc == 0) {
        return -1;
    }
    
    /* Find command */
    PSCommand* cmd = shell->commands;
    while (cmd) {
        if (strcmp(cmd->name, argv[0]) == 0) {
            int result = cmd->func(shell, argc, argv);
            ps_free_args(argv, argc);
            return result;
        }
        cmd = cmd->next;
    }
    
    /* Command not found */
    ps_print(shell, "Command not found: ");
    ps_println(shell, argv[0]);
    ps_free_args(argv, argc);
    return -1;
}

/* Register command */
int ps_register_command(PortableShell* shell, const char* name, const char* help, PSCommandFunc func) {
    if (!shell || !name || !func) return -1;
    
    PSCommand* cmd = (PSCommand*)malloc(sizeof(PSCommand));
    if (!cmd) return -1;
    
    cmd->name = name;
    cmd->help = help;
    cmd->func = func;
    cmd->next = shell->commands;
    shell->commands = cmd;
    
    return 0;
}

/* Unregister command */
int ps_unregister_command(PortableShell* shell, const char* name) {
    if (!shell || !name) return -1;
    
    PSCommand* prev = NULL;
    PSCommand* cmd = shell->commands;
    
    while (cmd) {
        if (strcmp(cmd->name, name) == 0) {
            if (prev) {
                prev->next = cmd->next;
            } else {
                shell->commands = cmd->next;
            }
            free(cmd);
            return 0;
        }
        prev = cmd;
        cmd = cmd->next;
    }
    
    return -1;
}

/* Register parameter */
int ps_register_parameter(PortableShell* shell, const char* name, PSParamType type, void* value_ptr, size_t max_size) {
    if (!shell || !name || !value_ptr) return -1;
    
    PSParameter* param = (PSParameter*)malloc(sizeof(PSParameter));
    if (!param) return -1;
    
    param->name = name;
    param->type = type;
    param->value_ptr = value_ptr;
    param->max_size = max_size;
    param->next = shell->parameters;
    shell->parameters = param;
    
    return 0;
}

/* Unregister parameter */
int ps_unregister_parameter(PortableShell* shell, const char* name) {
    if (!shell || !name) return -1;
    
    PSParameter* prev = NULL;
    PSParameter* param = shell->parameters;
    
    while (param) {
        if (strcmp(param->name, name) == 0) {
            if (prev) {
                prev->next = param->next;
            } else {
                shell->parameters = param->next;
            }
            free(param);
            return 0;
        }
        prev = param;
        param = param->next;
    }
    
    return -1;
}

/* Get parameter */
PSParameter* ps_get_parameter(PortableShell* shell, const char* name) {
    if (!shell || !name) return NULL;
    
    PSParameter* param = shell->parameters;
    while (param) {
        if (strcmp(param->name, name) == 0) {
            return param;
        }
        param = param->next;
    }
    
    return NULL;
}

/* Print string */
void ps_print(PortableShell* shell, const char* str) {
    if (!shell || !str) return;
    
    if (shell->config.write_str) {
        shell->config.write_str(shell->config.io_ctx, str);
    } else {
        while (*str) {
            shell->config.write_char(shell->config.io_ctx, *str++);
        }
    }
}

/* Print string with newline */
void ps_println(PortableShell* shell, const char* str) {
    ps_print(shell, str);
    shell->config.write_char(shell->config.io_ctx, '\n');
}

/* Print prompt */
void ps_print_prompt(PortableShell* shell) {
    if (shell && shell->config.prompt) {
        if (shell->config.vt100_enabled) {
            ps_vt100_color(shell, 32, -1);  /* Green */
            ps_print(shell, shell->config.prompt);
            ps_vt100_reset(shell);
        } else {
            ps_print(shell, shell->config.prompt);
        }
    }
}

/* Built-in command: help */
int ps_cmd_help(PortableShell* shell, int argc, char** argv) {
    (void)argc;
    (void)argv;
    ps_println(shell, "Available commands:");
    
    PSCommand* cmd = shell->commands;
    while (cmd) {
        ps_print(shell, "  ");
        ps_print(shell, cmd->name);
        if (cmd->help) {
            ps_print(shell, " - ");
            ps_print(shell, cmd->help);
        }
        ps_println(shell, "");
        cmd = cmd->next;
    }
    
    return 0;
}

/* Built-in command: clear */
int ps_cmd_clear(PortableShell* shell, int argc, char** argv) {
    (void)argc;
    (void)argv;
    ps_vt100_clear_screen(shell);
    ps_vt100_cursor_home(shell);
    return 0;
}

/* Built-in command: get */
int ps_cmd_get(PortableShell* shell, int argc, char** argv) {
    if (argc < 2) {
        ps_println(shell, "Usage: get <parameter>");
        return -1;
    }
    
    PSParameter* param = ps_get_parameter(shell, argv[1]);
    if (!param) {
        ps_print(shell, "Parameter not found: ");
        ps_println(shell, argv[1]);
        return -1;
    }
    
    char buf[128];
    ps_print(shell, argv[1]);
    ps_print(shell, " = ");
    
    switch (param->type) {
        case PS_PARAM_INT:
            snprintf(buf, sizeof(buf), "%d", *(int*)param->value_ptr);
            ps_println(shell, buf);
            break;
        case PS_PARAM_FLOAT:
            snprintf(buf, sizeof(buf), "%.2f", *(float*)param->value_ptr);
            ps_println(shell, buf);
            break;
        case PS_PARAM_STRING:
            ps_println(shell, (char*)param->value_ptr);
            break;
        case PS_PARAM_BOOL:
            ps_println(shell, *(bool*)param->value_ptr ? "true" : "false");
            break;
    }
    
    return 0;
}

/* Built-in command: set */
int ps_cmd_set(PortableShell* shell, int argc, char** argv) {
    if (argc < 3) {
        ps_println(shell, "Usage: set <parameter> <value>");
        return -1;
    }
    
    PSParameter* param = ps_get_parameter(shell, argv[1]);
    if (!param) {
        ps_print(shell, "Parameter not found: ");
        ps_println(shell, argv[1]);
        return -1;
    }
    
    switch (param->type) {
        case PS_PARAM_INT:
            *(int*)param->value_ptr = atoi(argv[2]);
            break;
        case PS_PARAM_FLOAT:
            *(float*)param->value_ptr = atof(argv[2]);
            break;
        case PS_PARAM_STRING:
            strncpy((char*)param->value_ptr, argv[2], param->max_size - 1);
            ((char*)param->value_ptr)[param->max_size - 1] = '\0';
            break;
        case PS_PARAM_BOOL:
            *(bool*)param->value_ptr = (strcmp(argv[2], "true") == 0 || 
                                        strcmp(argv[2], "1") == 0 ||
                                        strcmp(argv[2], "yes") == 0);
            break;
    }
    
    ps_print(shell, "Set ");
    ps_print(shell, argv[1]);
    ps_print(shell, " = ");
    ps_println(shell, argv[2]);
    
    return 0;
}

/* Built-in command: edit (TUI mode) */
int ps_cmd_edit(PortableShell* shell, int argc, char** argv) {
    (void)argc;
    (void)argv;
    if (!shell->parameters) {
        ps_println(shell, "No parameters to edit");
        return -1;
    }
    
    /* Enter alternate screen */
    ps_print(shell, "\x1b[?1049h");
    ps_vt100_clear_screen(shell);
    
    /* Draw header */
    ps_vt100_color(shell, 37, 44);  /* White on blue */
    ps_print(shell, " Parameter Editor - Use arrow keys to navigate, Enter to edit, 'q' to quit ");
    ps_vt100_reset(shell);
    ps_println(shell, "");
    ps_println(shell, "");
    
    /* List all parameters */
    ps_println(shell, "Parameter Name           Type      Value");
    ps_println(shell, "--------------------------------------------------------");
    
    PSParameter* param = shell->parameters;
    while (param) {
        char line[128];
        char value[64];
        
        switch (param->type) {
            case PS_PARAM_INT:
                snprintf(value, sizeof(value), "%d", *(int*)param->value_ptr);
                break;
            case PS_PARAM_FLOAT:
                snprintf(value, sizeof(value), "%.2f", *(float*)param->value_ptr);
                break;
            case PS_PARAM_STRING:
                snprintf(value, sizeof(value), "%s", (char*)param->value_ptr);
                break;
            case PS_PARAM_BOOL:
                snprintf(value, sizeof(value), "%s", *(bool*)param->value_ptr ? "true" : "false");
                break;
        }
        
        const char* type_str = "";
        switch (param->type) {
            case PS_PARAM_INT: type_str = "int"; break;
            case PS_PARAM_FLOAT: type_str = "float"; break;
            case PS_PARAM_STRING: type_str = "string"; break;
            case PS_PARAM_BOOL: type_str = "bool"; break;
        }
        
        snprintf(line, sizeof(line), "%-23s %-9s %s", param->name, type_str, value);
        ps_println(shell, line);
        
        param = param->next;
    }
    
    ps_println(shell, "");
    ps_println(shell, "Press 'q' to return to shell");
    
    /* Wait for 'q' key */
    while (1) {
        int c = shell->config.read_char(shell->config.io_ctx);
        if (c == 'q' || c == 'Q') break;
    }
    
    /* Leave alternate screen */
    ps_print(shell, "\x1b[?1049l");
    
    return 0;
}
