// Generated from assets/contracts/device-settings.json; do not hand-edit.
const PAGES: [(&str, &str, &str); 10] = [
    ("Overview", icons::CIRCUITRY, "device version identity status hardware NTP time discovery build flags"),
    ("Board identity", icons::GLOBE, "identity hostname rename board name application title icon mDNS AP"),
    ("Wi-Fi & Ethernet", icons::WIFI_HIGH, "network Ethernet W5500 wired DHCP static IPv4 gateway DNS station access point recovery SSID password scan"),
    ("MQTT", icons::BROADCAST, "cloud broker port topic username password TLS telemetry"),
    ("WebSocket", icons::LINK, "cloud websocket host port path token TLS telemetry"),
    ("Device access", icons::LOCK, "login authentication username password credentials security"),
    ("LEDs & cells", icons::GRID_FOUR, "table LED brightness effects mapping names logical physical modes games"),
    ("Front panel", icons::GAUGE, "TM1637 LCD display scrolling marquee once interval melodies buzzer audio"),
    ("AVR & inputs", icons::CPU, "ATmega328P saved settings EEPROM board name silent welcome brightness telemetry menu pages order threshold shift registers outputs ADC6 ninth input UART ISP busy build flags"),
    ("Firmware & storage", icons::FLOPPY_DISK, "OTA firmware flash EEPROM backup ISP update ntfy notifications topic"),
];
const PAGE_IDS: [&str; 10] = ["overview", "identity", "wifi", "mqtt", "websocket", "access", "leds", "front-panel", "avr", "firmware"];
const PAGE_FEATURES: [&[&str]; 10] = [&[], &["identity"], &["network"], &["network"], &["network"], &["network"], &["leds", "table"], &["display", "text", "buzzer", "lcd"], &["inputs", "outputs", "settings"], &["isp", "ota"]];
