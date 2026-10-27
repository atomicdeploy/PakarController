# portable-shell

A lightweight, portable shell/console/terminal library written in C for embedded devices and mini programs. Built from scratch to provide a shell-like CLI/TUI interface for resource-constrained systems.

## Project Overview

This library provides a complete shell implementation in C/C++ that can be used on:
- Embedded devices (Arduino, ESP8266, ESP32)
- Mini Win32 applications
- Linux/Unix programs
- Any platform with a C compiler

## Implementation Status

✅ **Complete** - All features from the original requirements have been implemented:

### Core Features
- ✅ Character-by-character I/O for Serial, stdin/stdout, TCP/IP, UDP sockets
- ✅ Command buffer with history (configurable size)
- ✅ Advanced key support: Ctrl-C, Ctrl-L, Home, End, arrows (←/→/↑/↓), Delete, Backspace, Enter, Tab
- ✅ Full VT-100/ANSI escape code support with colorization
- ✅ Built-in commands: help, clear/cls, get, set
- ✅ TUI mode with `edit` command for parameter management (alternate screen mode)
- ✅ Parameter binding system for device variables
- ✅ Multi-instance support for multiple I/O channels
- ✅ Memory efficient design for embedded systems

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
│   ├── linux_example.c     # Linux/Unix terminal example
│   ├── telnet_server.c     # Network server example
│   └── arduino_example.ino # Arduino Serial example
├── tests/
│   └── test_shell.c        # Test suite
├── Makefile                # Build system
├── DOCUMENTATION.md        # Detailed documentation
└── README.md               # This file
```

## Quick Start

### Building on Linux/Unix

```bash
# Clone the repository
git clone https://github.com/atomicdeploy/portable-shell.git
cd portable-shell

# Build library and examples
make

# Run tests
make test

# Run Linux example
./build/linux_example
```

### Basic Usage

```c
#include "portable_shell.h"

int main() {
    // Create shell with default configuration
    PortableShell* shell = ps_create(NULL);
    
    // Register custom command
    ps_register_command(shell, "hello", "Say hello", my_hello_cmd);
    
    // Register device parameter
    int my_value = 42;
    ps_register_parameter(shell, "value", PS_PARAM_INT, &my_value, 0);
    
    // Run shell (blocking)
    ps_run(shell);
    
    // Cleanup
    ps_destroy(shell);
    return 0;
}
```

## Features in Detail

### Input/Output Abstraction
- Works with any I/O backend through callbacks
- Included implementations: stdio, TCP/IP, UDP
- Easy to add custom I/O (Serial, SPI, etc.)

### Command System
- Register custom commands with callbacks
- Built-in commands: help, clear, get, set, edit
- Argument parsing included

### Parameter Management
- Bind C variables as shell parameters
- Supported types: int, float, string, bool
- Get/set via commands or TUI interface

### Terminal Features
- Command history with up/down arrows
- Line editing with cursor movement
- Ctrl-C to cancel, Ctrl-L to clear
- VT-100 colors and escape sequences
- BEL character support

### TUI Mode
- Full-screen parameter editor
- Navigate and edit device parameters
- Alternate screen mode (preserves terminal)

## Platform Examples

### Arduino
```cpp
// See examples/arduino_example.ino
void setup() {
    Serial.begin(115200);
    shell = ps_create(&config);
    ps_register_command(shell, "led", "Control LED", cmd_led);
}

void loop() {
    if (Serial.available()) {
        ps_process_char(shell, Serial.read());
    }
}
```

### Linux Terminal
```bash
# Compile and run
make
./build/linux_example

# Available commands
$ help
$ get counter
$ set counter 100
$ edit  # Opens full-screen parameter editor
```

### Network Server
```c
// Create shell for each TCP connection
PSSocketContext socket_ctx;
ps_io_socket_init(&config, &socket_ctx, "localhost", 23);
PortableShell* shell = ps_create(&config);
ps_run(shell);
```

## Testing

The project includes a comprehensive test suite:

```bash
make test
```

Tests cover:
- Shell creation/destruction
- Command registration and execution
- Parameter operations (get/set)
- Command buffer editing
- VT-100 functions
- Built-in commands

## Memory Requirements

Typical memory usage:
- **Shell instance**: ~500 bytes + history buffer
- **History buffer**: history_size × average_command_length
- **Command buffer**: 256 bytes
- **Per command**: ~16 bytes
- **Per parameter**: ~24 bytes

**For Arduino Uno (2KB RAM)**:
- Recommended history_size: 5
- Limit registered commands to essential ones

## API Documentation

See [DOCUMENTATION.md](DOCUMENTATION.md) for complete API reference including:
- Core functions
- Command management
- Parameter management
- VT-100 functions
- I/O abstraction
- Platform-specific notes

## Design Principles

1. **Zero Dependencies**: No OS-specific libraries required
2. **Maximum Portability**: 8-bit microcontrollers to desktop systems
3. **Flexible I/O**: Pluggable backend via callbacks
4. **Memory Efficient**: Static buffers, minimal heap usage
5. **Multi-Instance**: Multiple shells on different channels

## Implementation Plan (Completed)

- [x] Core shell engine with command buffer and history
- [x] I/O abstraction layer (Serial, stdio, sockets)
- [x] Key handling (all special keys supported)
- [x] VT-100/ANSI escape codes
- [x] Built-in commands (help, clear, get, set, edit)
- [x] TUI mode for parameter editing
- [x] Parameter binding system
- [x] Platform examples (Arduino, Linux, network)
- [x] Documentation and test suite
- [x] Build system (Makefile)

## Contributing

Contributions welcome! Please ensure:
- Code compiles without errors
- Tests pass (`make test`)
- Maintains portability
- Follows existing style

## License

MIT License - see [LICENSE](LICENSE) file for details

## Author

Portable Shell Contributors

