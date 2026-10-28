#include "ps_io.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

/* TCP/IP Socket I/O implementation */

static int socket_read_char(void* ctx) {
    PSSocketContext* sctx = (PSSocketContext*)ctx;
    
    /* If buffer is empty, read more data */
    if (sctx->read_pos >= sctx->read_len) {
        sctx->read_len = recv(sctx->socket_fd, sctx->read_buffer, sizeof(sctx->read_buffer), 0);
        if (sctx->read_len <= 0) {
            return -1;
        }
        sctx->read_pos = 0;
    }
    
    return (unsigned char)sctx->read_buffer[sctx->read_pos++];
}

static int socket_write_char(void* ctx, char c) {
    PSSocketContext* sctx = (PSSocketContext*)ctx;
    return send(sctx->socket_fd, &c, 1, 0) == 1 ? 0 : -1;
}

static int socket_write_str(void* ctx, const char* str) {
    PSSocketContext* sctx = (PSSocketContext*)ctx;
    int len = strlen(str);
    return send(sctx->socket_fd, str, len, 0) == len ? 0 : -1;
}

int ps_io_socket_init(PSConfig* config, PSSocketContext* ctx, const char* host, int port) {
    if (!config || !ctx || !host) return -1;
    
    /* Create socket */
    ctx->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx->socket_fd < 0) {
        return -1;
    }
    
    /* Connect to server */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        close(ctx->socket_fd);
        return -1;
    }
    
    if (connect(ctx->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(ctx->socket_fd);
        return -1;
    }
    
    /* Initialize context */
    ctx->read_pos = 0;
    ctx->read_len = 0;
    
    /* Set config callbacks */
    config->read_char = socket_read_char;
    config->write_char = socket_write_char;
    config->write_str = socket_write_str;
    config->io_ctx = ctx;
    
    return 0;
}

void ps_io_socket_close(PSSocketContext* ctx) {
    if (ctx && ctx->socket_fd >= 0) {
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
    }
}

/* UDP Socket I/O implementation */

static int udp_read_char(void* ctx) {
    PSUDPContext* uctx = (PSUDPContext*)ctx;
    
    /* If buffer is empty, read more data */
    if (uctx->read_pos >= uctx->read_len) {
        uctx->read_len = recv(uctx->socket_fd, uctx->read_buffer, sizeof(uctx->read_buffer), 0);
        if (uctx->read_len <= 0) {
            return -1;
        }
        uctx->read_pos = 0;
    }
    
    return (unsigned char)uctx->read_buffer[uctx->read_pos++];
}

static int udp_write_char(void* ctx, char c) {
    PSUDPContext* uctx = (PSUDPContext*)ctx;
    return send(uctx->socket_fd, &c, 1, 0) == 1 ? 0 : -1;
}

static int udp_write_str(void* ctx, const char* str) {
    PSUDPContext* uctx = (PSUDPContext*)ctx;
    int len = strlen(str);
    return send(uctx->socket_fd, str, len, 0) == len ? 0 : -1;
}

int ps_io_udp_init(PSConfig* config, PSUDPContext* ctx, int port) {
    if (!config || !ctx) return -1;
    
    /* Create socket */
    ctx->socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->socket_fd < 0) {
        return -1;
    }
    
    /* Bind to port */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(ctx->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(ctx->socket_fd);
        return -1;
    }
    
    /* Initialize context */
    ctx->read_pos = 0;
    ctx->read_len = 0;
    
    /* Set config callbacks */
    config->read_char = udp_read_char;
    config->write_char = udp_write_char;
    config->write_str = udp_write_str;
    config->io_ctx = ctx;
    
    return 0;
}

void ps_io_udp_close(PSUDPContext* ctx) {
    if (ctx && ctx->socket_fd >= 0) {
        close(ctx->socket_fd);
        ctx->socket_fd = -1;
    }
}

/* Serial port I/O implementation stub for embedded platforms */
/* This would need platform-specific implementation */

int ps_io_serial_init(PSConfig* config, PSSerialContext* ctx, const char* port, int baud_rate) {
    /* Platform-specific serial port initialization would go here */
    /* For Arduino: Serial.begin(baud_rate) */
    /* For Linux: open /dev/ttyUSB0, set termios attributes */
    return -1;  /* Not implemented in this stub */
}

void ps_io_serial_close(PSSerialContext* ctx) {
    /* Platform-specific serial port cleanup */
}
