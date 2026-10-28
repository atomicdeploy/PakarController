# Portable Shell Library

A lightweight, portable shell/console/terminal library written in C for embedded devices and mini programs. Designed to work on Arduino, ESP8266, Linux, Windows, and other platforms with minimal dependencies.

## Features

- **Character-by-character I/O**: Works with Serial, stdin/stdout, TCP/IP, UDP sockets
- **Command History**: Configurable history buffer with up/down arrow navigation
- **Advanced Key Support**: Ctrl-C, Ctrl-L, Home, End, arrows (←/→/↑/↓), Delete, Backspace, Enter, Tab
- **VT-100/ANSI Support**: Full terminal escape codes support with colorization
- **Built-in Commands**: help, clear/cls, get, set
- **TUI Mode**: Interactive parameter editor with `edit` command (alternate screen mode)
- **Parameter Binding**: Bind device parameters to variables for easy management
- **Multi-instance Support**: Create multiple shell instances on different I/O channels
- **Memory Efficient**: Designed for resource-constrained embedded systems

## Project Structure

```
portable-shell/
├── include/
│   ├── portable_shell.h    # Main API header
│   └── ps_io.h             # I/O abstraction layer
├── src/
│   ├── portable_shell.c    # Core implementation
│   ├── vt100.c             # VT-100/ANSI codes
│   └── ps_io_socket.c      # Socket I/O implementations
├── examples/
│   ├── linux_example.c     # Linux/Unix example
│   ├── telnet_server.c     # Network example
│   └── arduino_example.ino # Arduino example
├── Makefile                # Build system
└── README.md               # This file
```

## Building

### Linux/Unix

```bash
make clean
make
```

This will create:
- `build/libportableshell.a` - Static library
- `build/linux_example` - Linux example application
- `build/telnet_server` - Network server example

### Installation

```bash
sudo make install
```

This installs the library to `/usr/local/lib` and headers to `/usr/local/include`.

## Usage

### Basic Example

```c
#include "portable_shell.h"

int main() {
    // Create configuration with defaults
    PSConfig config = {0};
    config.history_size = 10;
    config.vt100_enabled = true;
    config.prompt = "$ ";
    
    // Create shell instance
    PortableShell* shell = ps_create(&config);
    
    // Register custom command
    ps_register_command(shell, "mycommand", "My command help", my_cmd_func);
    
    // Register parameter
    int my_value = 42;
    ps_register_parameter(shell, "myvalue", PS_PARAM_INT, &my_value, 0);
    
    // Run shell
    ps_run(shell);
    
    // Cleanup
    ps_destroy(shell);
    
    return 0;
}
```

### Custom Command Example

```c
int my_cmd_func(PortableShell* shell, int argc, char** argv) {
    ps_println(shell, "Hello from custom command!");
    
    if (argc > 1) {
        ps_print(shell, "Arguments: ");
        for (int i = 1; i < argc; i++) {
            ps_print(shell, argv[i]);
            ps_print(shell, " ");
        }
        ps_println(shell, "");
    }
    
    return 0;
}
```

### Parameter Types

The library supports four parameter types:

- `PS_PARAM_INT` - Integer values
- `PS_PARAM_FLOAT` - Floating point values
- `PS_PARAM_STRING` - String values (requires max_size)
- `PS_PARAM_BOOL` - Boolean values

```c
int counter = 0;
float temperature = 25.5;
char name[64] = "Device";
bool enabled = true;

ps_register_parameter(shell, "counter", PS_PARAM_INT, &counter, 0);
ps_register_parameter(shell, "temperature", PS_PARAM_FLOAT, &temperature, 0);
ps_register_parameter(shell, "name", PS_PARAM_STRING, name, sizeof(name));
ps_register_parameter(shell, "enabled", PS_PARAM_BOOL, &enabled, 0);
```

Users can then interact with these parameters:

```
$ get counter
counter = 0
$ set counter 42
Set counter = 42
$ edit
[Opens TUI parameter editor]
```

## Platform-Specific Examples

### Arduino

See `examples/arduino_example.ino` for a complete Arduino example using the Serial interface.

Key points:
- Limit history size (e.g., 5) due to limited RAM
- Use `ps_process_char()` in the `loop()` function
- Implement custom I/O callbacks for Serial interface

### ESP8266/ESP32

Similar to Arduino, but can support more features due to more available RAM.

### Linux/Unix

See `examples/linux_example.c` for a full-featured terminal application.

- Enable raw terminal mode for proper key handling
- Support for VT-100 escape sequences
- Larger history buffer

### TCP/IP Sockets

See `examples/telnet_server.c` for network-based shell access.

- Create server socket and accept connections
- Spawn shell instance for each connection
- Use socket I/O callbacks from `ps_io.h`

## Built-in Commands

- `help` - Display available commands
- `clear` / `cls` - Clear screen
- `get <param>` - Get parameter value
- `set <param> <value>` - Set parameter value
- `edit` - Open TUI parameter editor

## Key Bindings

- `Ctrl-C` - Cancel current line
- `Ctrl-L` - Clear screen
- `Enter` - Execute command
- `Backspace` / `Delete` - Delete characters
- `←` / `→` - Navigate cursor
- `↑` / `↓` - Navigate command history
- `Home` / `End` - Jump to start/end of line
- `Tab` - Tab completion (beeps for now)

## API Reference

### Core Functions

- `PortableShell* ps_create(PSConfig* config)` - Create shell instance
- `void ps_destroy(PortableShell* shell)` - Destroy shell instance
- `void ps_run(PortableShell* shell)` - Run main loop
- `void ps_stop(PortableShell* shell)` - Stop shell
- `int ps_process_char(PortableShell* shell, char c)` - Process single character

### Command Management

- `int ps_register_command(shell, name, help, func)` - Register command
- `int ps_unregister_command(shell, name)` - Unregister command

### Parameter Management

- `int ps_register_parameter(shell, name, type, value_ptr, max_size)` - Register parameter
- `int ps_unregister_parameter(shell, name)` - Unregister parameter
- `PSParameter* ps_get_parameter(shell, name)` - Get parameter

### Utility Functions

- `void ps_print(shell, str)` - Print string
- `void ps_println(shell, str)` - Print string with newline
- `void ps_print_prompt(shell)` - Print prompt

### VT-100 Functions

- `void ps_vt100_clear_screen(shell)` - Clear screen
- `void ps_vt100_cursor_home(shell)` - Move cursor to home
- `void ps_vt100_color(shell, fg, bg)` - Set text color
- `void ps_bell(shell)` - Ring bell

## Memory Usage

Typical memory footprint (approximate):
- Shell instance: ~500 bytes + history buffer
- History buffer: `history_size * avg_cmd_length`
- Command buffer: 256 bytes
- Each registered command: ~16 bytes
- Each registered parameter: ~24 bytes

For embedded systems with limited RAM (e.g., Arduino Uno with 2KB RAM):
- Recommended history_size: 5-10
- Limit number of commands and parameters
- Use shorter command strings

## Design Principles

1. **Zero Dependencies**: No OS-specific libraries required (except for examples)
2. **Portable**: Works on 8-bit microcontrollers to desktop systems
3. **Flexible I/O**: Pluggable I/O backend via callbacks
4. **Memory Efficient**: Static buffers, minimal heap allocations
5. **Reentrant**: Multiple instances can coexist

## Contributing

Contributions are welcome! Please ensure:
- Code compiles without warnings
- Follows existing code style
- Maintains portability
- Includes examples for new features

## License

[Specify your license here]

## Author

[Specify author information here]

## Acknowledgments

Built from scratch to provide a shell-like interface for embedded devices and mini programs.
