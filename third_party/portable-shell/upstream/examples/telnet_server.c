#include "portable_shell.h"
#include "ps_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

static PortableShell* g_shell = NULL;

void signal_handler(int sig) {
    if (g_shell) {
        ps_stop(g_shell);
    }
}

int main(int argc, char** argv) {
    int port = 23;  /* Default telnet port */
    
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    
    printf("Starting telnet server on port %d\n", port);
    printf("Connect with: telnet localhost %d\n", port);
    
    /* Setup signal handler */
    signal(SIGINT, signal_handler);
    
    /* Create socket context */
    PSSocketContext socket_ctx;
    
    /* Create shell configuration with socket I/O */
    PSConfig config = {0};
    config.history_size = 50;
    config.vt100_enabled = true;
    config.prompt = "telnet> ";
    
    /* Initialize socket I/O - this would be a server socket in production */
    /* For this example, we'll use a simplified approach */
    
    printf("Note: This is a simplified example.\n");
    printf("A full implementation would create a server socket,\n");
    printf("accept connections, and spawn a shell for each connection.\n");
    
    return 0;
}
