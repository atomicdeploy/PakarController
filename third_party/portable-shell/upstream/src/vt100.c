#include "portable_shell.h"
#include <stdio.h>

/* VT-100 / ANSI escape codes implementation */

void ps_vt100_clear_screen(PortableShell* shell) {
    if (!shell || !shell->config.vt100_enabled) return;
    ps_print(shell, "\x1b[2J");
}

void ps_vt100_clear_line(PortableShell* shell) {
    if (!shell || !shell->config.vt100_enabled) return;
    ps_print(shell, "\x1b[2K");
}

void ps_vt100_cursor_home(PortableShell* shell) {
    if (!shell || !shell->config.vt100_enabled) return;
    ps_print(shell, "\x1b[H");
}

void ps_vt100_cursor_move(PortableShell* shell, int col) {
    if (!shell || !shell->config.vt100_enabled) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "\x1b[%dG", col + 1);
    ps_print(shell, buf);
}

void ps_vt100_cursor_left(PortableShell* shell, int n) {
    if (!shell || !shell->config.vt100_enabled) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "\x1b[%dD", n);
    ps_print(shell, buf);
}

void ps_vt100_cursor_right(PortableShell* shell, int n) {
    if (!shell || !shell->config.vt100_enabled) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "\x1b[%dC", n);
    ps_print(shell, buf);
}

void ps_vt100_color(PortableShell* shell, int fg, int bg) {
    if (!shell || !shell->config.vt100_enabled) return;
    char buf[32];
    if (bg >= 0) {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dm", fg, bg);
    } else {
        snprintf(buf, sizeof(buf), "\x1b[%dm", fg);
    }
    ps_print(shell, buf);
}

void ps_vt100_reset(PortableShell* shell) {
    if (!shell || !shell->config.vt100_enabled) return;
    ps_print(shell, "\x1b[0m");
}

void ps_bell(PortableShell* shell) {
    if (!shell) return;
    shell->config.write_char(shell->config.io_ctx, '\a');
}
