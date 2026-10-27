// Generated shared device page catalog; mobile native editors remain tracked.
const deviceSettingsCatalog = {
  "pages": [
    {
      "id": "overview",
      "label": "Overview",
      "icon": "CIRCUITRY",
      "keywords": "device version identity status hardware NTP time discovery build flags",
      "features": []
    },
    {
      "id": "identity",
      "label": "Board identity",
      "icon": "GLOBE",
      "keywords": "identity hostname rename board name application title icon mDNS AP",
      "features": [
        "identity"
      ]
    },
    {
      "id": "wifi",
      "label": "Wi-Fi & Ethernet",
      "icon": "WIFI_HIGH",
      "keywords": "network Ethernet W5500 wired DHCP static IPv4 gateway DNS station access point recovery SSID password scan",
      "features": [
        "network"
      ]
    },
    {
      "id": "mqtt",
      "label": "MQTT",
      "icon": "BROADCAST",
      "keywords": "cloud broker port topic username password TLS telemetry",
      "features": [
        "network"
      ]
    },
    {
      "id": "websocket",
      "label": "WebSocket",
      "icon": "LINK",
      "keywords": "cloud websocket host port path token TLS telemetry",
      "features": [
        "network"
      ]
    },
    {
      "id": "access",
      "label": "Device access",
      "icon": "LOCK",
      "keywords": "login authentication username password credentials security",
      "features": [
        "network"
      ]
    },
    {
      "id": "leds",
      "label": "LEDs & cells",
      "icon": "GRID_FOUR",
      "keywords": "table LED brightness effects mapping names logical physical modes games pixel groups custom counts",
      "features": [
        "leds",
        "table",
        "pixel-group-layout"
      ]
    },
    {
      "id": "front-panel",
      "label": "Front panel",
      "icon": "GAUGE",
      "keywords": "TM1637 LCD display scrolling marquee once interval melodies buzzer audio",
      "features": [
        "display",
        "text",
        "buzzer",
        "lcd"
      ]
    },
    {
      "id": "avr",
      "label": "AVR & inputs",
      "icon": "CPU",
      "keywords": "ATmega328P saved settings EEPROM board name silent welcome brightness telemetry menu pages order threshold shift registers outputs ADC6 ninth input UART ISP busy build flags",
      "features": [
        "inputs",
        "outputs",
        "settings"
      ]
    },
    {
      "id": "firmware",
      "label": "Firmware & storage",
      "icon": "FLOPPY_DISK",
      "keywords": "OTA firmware flash EEPROM backup ISP update ntfy notifications topic",
      "features": [
        "isp",
        "ota"
      ]
    }
  ],
  "build_only": [
    "link_mode",
    "AVR clock and fuses",
    "pin ownership",
    "WS2811 chipset and channel order",
    "maximum pixel capacity and default group count"
  ],
  "network": {
    "station_enabled": "boolean",
    "ap_enabled": "boolean",
    "station_ssid": "string",
    "dhcp": "optional:boolean",
    "address": "optional:string",
    "gateway": "optional:string",
    "subnet": "optional:string",
    "dns1": "optional:string",
    "dns2": "optional:string"
  },
  "ethernet": {
    "enabled": "optional:boolean",
    "lldp_enabled": "optional:boolean",
    "lldp_receive": "optional:boolean",
    "dhcp": "boolean",
    "address": "string",
    "gateway": "string",
    "subnet": "string",
    "dns1": "string",
    "dns2": "string"
  },
  "cloud": {
    "mqtt_enabled": "boolean",
    "mqtt_tls": "boolean",
    "mqtt_host": "string",
    "mqtt_port": "port",
    "mqtt_user": "string",
    "mqtt_base_topic": "string",
    "mqtt_password_set": "boolean",
    "websocket_enabled": "boolean",
    "websocket_tls": "boolean",
    "websocket_host": "string",
    "websocket_port": "port",
    "websocket_path": "string",
    "websocket_token_set": "boolean",
    "telemetry_seconds": "interval",
    "ntfy_enabled": "optional:boolean",
    "ntfy_host": "optional:string",
    "ntfy_topic": "optional:string",
    "ntfy_token_set": "optional:boolean"
  }
};
