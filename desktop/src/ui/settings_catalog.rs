// Generated from assets/contracts/device-settings.json; do not hand-edit.
const PAGES: [(&str, &str, &str); 10] = [
    ("Overview", icons::CIRCUITRY, "device version identity status hardware NTP time discovery build flags"),
    ("Board identity", icons::GLOBE, "identity hostname rename board name application title icon mDNS AP"),
    ("Wi-Fi", icons::WIFI_HIGH, "network station access point recovery SSID password scan"),
    ("MQTT", icons::BROADCAST, "cloud broker port topic username password TLS telemetry"),
    ("WebSocket", icons::LINK, "cloud websocket host port path token TLS telemetry"),
    ("Device access", icons::LOCK, "login authentication username password credentials security"),
    ("LEDs & cells", icons::GRID_FOUR, "table LED brightness effects mapping names logical physical modes games"),
    ("Front panel", icons::GAUGE, "TM1637 display scrolling marquee once interval melodies buzzer audio"),
    ("AVR & inputs", icons::CPU, "ATmega328P shift registers outputs ADC6 ninth input UART ISP busy build flags"),
    ("Firmware & storage", icons::FLOPPY_DISK, "OTA firmware flash EEPROM backup ISP update"),
];
const PAGE_IDS: [&str; 10] = ["overview", "identity", "wifi", "mqtt", "websocket", "access", "leds", "front-panel", "avr", "firmware"];
const PAGE_FEATURES: [&[&str]; 10] = [&[], &["identity"], &["network"], &["network"], &["network"], &["network"], &["table"], &["display", "text", "buzzer"], &["inputs", "outputs", "settings"], &["isp", "ota"]];
