#pragma once
#include "ShellNames.h"
// Presentation only: stable API/property identifiers remain unchanged.
namespace PakarShell {
inline const char* propertyLabel(const char* key){
  struct Label {const char* key;const char* text;};
  static constexpr Label labels[]={
    {"hostname","Hostname"},{"label","Device name"},{"version","Firmware version"},
    {"boot_id","Boot identity"},{"firmware.hash","Firmware hash"},{"build.date","Build date"},
    {"uptime","Uptime (seconds)"},{"heap","Free memory (bytes)"},{"avr.online","AVR connected"},
    {"display.ack","Display detected"},{"firmware.phase","Update status"},
    {"network.mode","Network mode"},{"network.station_enabled","Join Wi-Fi"},
    {"network.ap_enabled","Access point"},{"network.auto_switch","Switch networks"},
    {"network.ap_ssid","Access point name"},{"network.dhcp","Automatic address"},
    {"network.ssid","Wi-Fi network"},{"network.password","Wi-Fi password"},
    {"network.address","Static IP address"},{"network.gateway","Gateway"},
    {"network.subnet","Subnet mask"},{"network.dns1","Primary DNS"},{"network.dns2","Secondary DNS"},
    {"network.ip","Wi-Fi address"},{"network.ap_ip","Access point address"},
    {"led.enabled","LED output"},{"led.mode","Output mode"},{"led.brightness","Brightness"},
    {"led.effect","Effect"},{"led.backend","Active driver"},{"led.rmt","RMT available"},{"led.dma","DMA available"},
    {"mqtt.enabled","MQTT enabled"},{"mqtt.tls","MQTT encryption"},{"mqtt.host","MQTT server"},
    {"mqtt.port","MQTT port"},{"mqtt.user","MQTT username"},{"mqtt.password","MQTT password"},{"mqtt.topic","MQTT topic"},
    {"websocket.enabled","WebSocket enabled"},{"websocket.tls","WebSocket encryption"},
    {"websocket.host","WebSocket server"},{"websocket.port","WebSocket port"},
    {"websocket.path","WebSocket path"},{"websocket.token","WebSocket token"},
    {"telemetry.seconds","Telemetry interval (s)"},{"ntfy.enabled","Notifications"},
    {"ntfy.host","Notification server"},{"ntfy.topic","Notification topic"},{"ntfy.token","Notification token"},
    {"shell.enabled","Telnet enabled"},{"shell.port","Telnet port"},{"shell.color","Default color"},
    {"ssh.enabled","SSH enabled"},{"ssh.port","SSH port"},
    {"game.enabled_mask","Enabled boxes (mask)"},{"game.auto_reset_ms","Auto reset (ms)"},
    {"game.effects","Extended effects"},{"game.chase_ms","Chase period (ms)"},
    {"game.neighbors","Chase neighbors"},{"game.invert","Invert chase dot"},
    {"game.idle_rgb","Ready color (RGB)"},{"game.win_rgb","Winner color (RGB)"},
    {"game.lose_rgb","Lost color (RGB)"},{"game.winner_cue","Winner melody"},{"game.reset_cue","Reset melody"}
  };
  for(const auto& label:labels)if(sameName(key,label.key))return label.text;
  return "Setting";
}
}
