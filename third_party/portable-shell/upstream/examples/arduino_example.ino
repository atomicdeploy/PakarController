/*
 * Arduino Example for Portable Shell
 * 
 * This example shows how to use the portable shell library
 * with Arduino's Serial interface.
 */

#include "portable_shell.h"

// Global shell instance
PortableShell* shell = NULL;

// Device parameters
int led_brightness = 128;
float sensor_value = 0.0;
char device_id[32] = "Arduino-001";
bool debug_mode = false;

// Arduino-specific I/O functions
int arduino_read_char(void* ctx) {
    if (Serial.available() > 0) {
        return Serial.read();
    }
    return -1;
}

int arduino_write_char(void* ctx, char c) {
    Serial.write(c);
    return 0;
}

int arduino_write_str(void* ctx, const char* str) {
    Serial.print(str);
    return 0;
}

// Custom command: Read analog pin
int cmd_analog(PortableShell* shell, int argc, char** argv) {
    if (argc < 2) {
        ps_println(shell, "Usage: analog <pin>");
        return -1;
    }
    
    int pin = atoi(argv[1]);
    int value = analogRead(pin);
    
    char buf[64];
    snprintf(buf, sizeof(buf), "A%d = %d", pin, value);
    ps_println(shell, buf);
    
    return 0;
}

// Custom command: Control LED
int cmd_led(PortableShell* shell, int argc, char** argv) {
    if (argc < 2) {
        ps_println(shell, "Usage: led <on|off>");
        return -1;
    }
    
    if (strcmp(argv[1], "on") == 0) {
        digitalWrite(LED_BUILTIN, HIGH);
        ps_println(shell, "LED ON");
    } else if (strcmp(argv[1], "off") == 0) {
        digitalWrite(LED_BUILTIN, LOW);
        ps_println(shell, "LED OFF");
    } else {
        ps_println(shell, "Usage: led <on|off>");
        return -1;
    }
    
    return 0;
}

void setup() {
    // Initialize serial
    Serial.begin(115200);
    while (!Serial) {
        ; // Wait for serial port to connect
    }
    
    // Initialize LED
    pinMode(LED_BUILTIN, OUTPUT);
    
    // Create shell configuration
    PSConfig config;
    config.read_char = arduino_read_char;
    config.write_char = arduino_write_char;
    config.write_str = arduino_write_str;
    config.io_ctx = NULL;
    config.history_size = 5;  // Limited history due to memory constraints
    config.vt100_enabled = true;
    config.prompt = "> ";
    
    // Create shell
    shell = ps_create(&config);
    
    if (!shell) {
        Serial.println("Failed to create shell!");
        return;
    }
    
    // Register custom commands
    ps_register_command(shell, "analog", "Read analog pin", cmd_analog);
    ps_register_command(shell, "led", "Control LED (on/off)", cmd_led);
    
    // Register parameters
    ps_register_parameter(shell, "brightness", PS_PARAM_INT, &led_brightness, 0);
    ps_register_parameter(shell, "sensor", PS_PARAM_FLOAT, &sensor_value, 0);
    ps_register_parameter(shell, "device_id", PS_PARAM_STRING, device_id, sizeof(device_id));
    ps_register_parameter(shell, "debug", PS_PARAM_BOOL, &debug_mode, 0);
    
    // Print welcome message
    Serial.println("=================================");
    Serial.println("Portable Shell - Arduino Example");
    Serial.println("=================================");
    Serial.println("Type 'help' for available commands");
    Serial.println();
    
    ps_print_prompt(shell);
}

void loop() {
    // Process incoming characters
    if (Serial.available() > 0) {
        char c = Serial.read();
        ps_process_char(shell, c);
    }
    
    // Update sensor value (example)
    sensor_value = analogRead(A0) * 0.1;
}
