#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
c++ -std=c++17 -Wall -Wextra -pedantic \
  -I "$root/libraries/BoardCore/src" \
  "$root/tests/protocol_test.cpp" -o "$root/build/protocol-test"
"$root/build/protocol-test"
c++ -std=c++17 -Wall -Wextra -Werror "$root/tests/semantic_panel_test.cpp" -o "$root/build/semantic-panel-test"
"$root/build/semantic-panel-test"
c++ -std=c++17 -Wall -Wextra -Werror -pthread -I "$root/firmware/esp8266/src" "$root/tests/led_scheduler_test.cpp" -o "$root/build/led-scheduler-test"
"$root/build/led-scheduler-test"
# Match test-led-worker.ps1: scenario names are specific to the compiled backend.
# Retain all DMA failure cases; do not send them to the default static worker.
for profile in static-bgr static-rgb dma-bgr dma-rgb; do
  worker_flags=()
  if [[ "$profile" == *-rgb ]]; then worker_flags+=(-DPAKAR_S3_LED_SWAP_RED_BLUE=0); fi
  if [[ "$profile" == dma-* ]]; then worker_flags+=(-DPAKAR_S3_LED_USE_DMA=1); fi
  c++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -pthread "${worker_flags[@]}" \
    -I "$root/tests/led_worker_stubs" "$root/tests/led_worker_test.cpp" \
    "$root/firmware/esp32s3/src/LedOutput.cpp" -o "$root/build/led-worker-test"
  printf 'Actual worker wire profile: %s\n' "$profile"
  if [[ "$profile" == dma-* ]]; then worker_scenarios=(functional timeout stop-failure); else worker_scenarios=(static); fi
  for scenario in "${worker_scenarios[@]}"; do "$root/build/led-worker-test" "$scenario"; done
done
c++ -std=c++17 -Wall -Wextra -Werror "$root/tests/mndp_test.cpp" -o "$root/build/mndp-test"
"$root/build/mndp-test"
c++ -std=c++17 -Wall -Wextra -Werror "$root/tests/remote_policy_test.cpp" -o "$root/build/remote-policy-test"
"$root/build/remote-policy-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/isp_stubs" "$root/tests/avr_isp_test.cpp" "$root/firmware/esp8266/src/AvrIsp.cpp" -o "$root/build/avr-isp-test"
"$root/build/avr-isp-test"
c++ -std=c++17 -Wall -Wextra -Werror "$root/tests/urboot_protocol_test.cpp" -o "$root/build/urboot-protocol-test"
"$root/build/urboot-protocol-test"
node "$root/tests/urboot_integration_test.mjs"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/access_stubs" -I "$HOME/Arduino/libraries/ArduinoJson/src" -I "$root/libraries/BoardCore/src" "$root/tests/client_telemetry_test.cpp" -o "$root/build/client-telemetry-test"
"$root/build/client-telemetry-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/remote_stubs" -I "$HOME/Arduino/libraries/ArduinoJson/src" -I "$root/libraries/BoardCore/src" "$root/tests/remote_firmware_test.cpp" "$root/firmware/esp8266/src/RemoteFirmware.cpp" -lcrypto -o "$root/build/remote-firmware-test"
"$root/build/remote-firmware-test"
pwsh -NoProfile -File "$root/tools/consult-reuse.ps1" -Concern remote-ota -Gate > /dev/null
c++ -std=c++17 -Wall -Wextra -Werror -DARDUINO_ARCH_ESP32 -DESP32 \
  -I "$root/tests/s3_ota_stubs" -I "$root/tests/remote_stubs" \
  -I "$HOME/Arduino/libraries/ArduinoJson/src" -I "$root/libraries/BoardCore/src" \
  "$root/tests/remote_s3_test.cpp" "$root/firmware/esp8266/src/RemoteFirmware.cpp" \
  -lcrypto -o "$root/build/remote-s3-test"
"$root/build/remote-s3-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/s3_dns_stubs" \
  "$root/tests/s3_dns_lock_test.cpp" "$root/tests/s3_dns_runtime.cpp" \
  "$root/firmware/esp32s3/src/DnsCoreLock.cpp" -Wl,--wrap=dns_clear_cache -o "$root/build/s3-dns-lock-test"
"$root/build/s3-dns-lock-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/s3_dns_stubs" "$root/tests/s3_dns_recovery_test.cpp" -o "$root/build/s3-dns-recovery-test"
"$root/build/s3-dns-recovery-test"
# Match test-s3-flash-guard.ps1 and the production six-function flash boundary.
# These must be linker wraps, not __real_* test aliases: the fixture verifies
# that SDK-like calls actually enter the fail-closed production wrappers.
c++ -std=c++17 -Wall -Wextra -Werror -pthread -I "$root/tests/s3_flash_guard_stubs" \
  "$root/tests/s3_flash_guard_test.cpp" "$root/tests/s3_flash_guard_runtime.cpp" \
  "$root/firmware/esp32s3/src/LedFlashGuard.cpp" \
  -Wl,--wrap=esp_partition_write,--wrap=esp_partition_write_raw,--wrap=esp_partition_erase_range,--wrap=esp_partition_read,--wrap=esp_partition_read_raw,--wrap=esp_image_verify \
  -o "$root/build/s3-flash-guard-test"
"$root/build/s3-flash-guard-test"
# The Linux equivalents of the focused PowerShell runners use the SAME gates,
# profiles and canonical output directory. Use Arduino CLI's existing Linux
# library path, not Windows MyDocuments; no second SDK/cache or test install.
pwsh -NoProfile -File "$root/tools/consult-reuse.ps1" -Concern api-contract -Gate > /dev/null
pwsh -NoProfile -File "$root/tools/consult-reuse.ps1" -Concern network-cloud > /dev/null
pwsh -NoProfile -File "$root/tools/consult-reuse.ps1" -Concern build-storage -Gate > /dev/null
pwsh -NoProfile -File "$root/tools/consult-reuse.ps1" -Concern peer-binding -Gate > /dev/null
mkdir -p "$root/build/native"
pwsh -NoProfile -File "$root/tools/consult-reuse.ps1" -Concern remote-ota -Gate > /dev/null
for upgrade_case in upgrade_lifecycle_test local_ota_receipt_test; do
  c++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 \
    -I "$root/tests/remote_stubs" -I "$HOME/Arduino/libraries/ArduinoJson/src" \
    "$root/tests/$upgrade_case.cpp" -o "$root/build/native/$upgrade_case"
  "$root/build/native/$upgrade_case"
done
for upgrade_platform in native ESP8266; do
  upgrade_flags=()
  if [[ "$upgrade_platform" == ESP8266 ]]; then upgrade_flags=(-DARDUINO_ARCH_ESP8266 -I "$root/tests/atomic_stubs"); fi
  c++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -pthread "${upgrade_flags[@]}" \
    "$root/tests/update_progress_test.cpp" -o "$root/build/native/update_progress_test"
  "$root/build/native/update_progress_test"
done
node "$root/tests/upgrade_web_test.mjs"
node "$root/tests/boot_audio_test.mjs"
for cloud_platform in ESP8266 ESP32; do
  c++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror "-D$cloud_platform" \
    -I "$root/libraries/BoardCore/src" "$root/tests/cloud_command_guard_test.cpp" \
    -o "$root/build/native/cloud_command_guard_test"
  "$root/build/native/cloud_command_guard_test"
  c++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror "-D$cloud_platform" \
    -I "$root/tests/cloud_stubs" "$root/tests/cloud_config_test.cpp" \
    "$root/firmware/esp8266/src/CloudConfig.cpp" -o "$root/build/native/cloud_config_test"
  "$root/build/native/cloud_config_test"
  c++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror "-D$cloud_platform" \
    -I "$root/tests/cloud_service_stubs" -I "$HOME/Arduino/libraries/ArduinoJson/src" \
    -I "$root/libraries/BoardCore/src" -I "$root/firmware/esp8266/src" \
    "$root/tests/cloud_services_test.cpp" "$root/firmware/esp8266/src/CloudServices.cpp" \
    "$root/firmware/esp8266/src/CloudConfig.cpp" -o "$root/build/native/cloud_services_test"
  "$root/build/native/cloud_services_test"
  cloud_arch_flags=()
  if [[ "$cloud_platform" == ESP32 ]]; then cloud_arch_flags=(-DARDUINO_ARCH_ESP32); fi
  c++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror "-D$cloud_platform" "${cloud_arch_flags[@]}" \
    -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 \
    -I "$root/tests/management_stubs" -I "$root/tests/cloud_stubs" \
    -I "$root/tests/network_stubs" -I "$HOME/Arduino/libraries/ArduinoJson/src" \
    "$root/tests/cloud_management_test.cpp" "$root/firmware/esp8266/src/CloudConfig.cpp" \
    "$root/firmware/esp8266/src/NetworkConfig.cpp" -o "$root/build/native/cloud_management_test"
  "$root/build/native/cloud_management_test"
done
c++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror \
  -I "$root/tests/access_stubs" -I "$HOME/Arduino/libraries/ArduinoJson/src" \
  -I "$root/libraries/BoardCore/src" -I "$root/firmware/esp8266/src" \
  "$root/tests/command_values_test.cpp" -o "$root/build/native/command_values_test"
"$root/build/native/command_values_test"
c++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -DESP32 \
  -I "$root/tests/ethernet_config_stubs" "$root/tests/ethernet_config_test.cpp" \
  -o "$root/build/native/ethernet_config_test"
"$root/build/native/ethernet_config_test"
for ethernet_profile in 0 1; do
  c++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -DESP32 -DARDUINO_ARCH_ESP32 \
    "-DPAKAR_S3_ETHERNET_ENABLED=$ethernet_profile" -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 \
    -I "$root/tests/management_stubs" -I "$root/tests/ethernet_config_stubs" \
    -I "$root/tests/network_stubs" -I "$HOME/Arduino/libraries/ArduinoJson/src" \
    "$root/tests/ethernet_management_test.cpp" "$root/firmware/esp8266/src/CloudConfig.cpp" \
    "$root/firmware/esp8266/src/NetworkConfig.cpp" -o "$root/build/native/ethernet_management_test"
  "$root/build/native/ethernet_management_test"
  c++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -DARDUINO_ARCH_ESP32 -DCONFIG_IDF_TARGET_ESP32S3 \
    "-DPAKAR_S3_ETHERNET_ENABLED=$ethernet_profile" -I "$root/tests/ethernet_stubs" \
    "$root/tests/s3_ethernet_port_test.cpp" "$root/firmware/esp32s3/src/EthernetPort.cpp" \
    -o "$root/build/native/s3_ethernet_port_test"
  "$root/build/native/s3_ethernet_port_test"
  c++ -std=c++17 -O2 -g0 -Wall -Wextra -Werror -DESP32 -DARDUINO_ARCH_ESP32 -DCONFIG_IDF_TARGET_ESP32S3 \
    "-DPAKAR_S3_ETHERNET_ENABLED=$ethernet_profile" -I "$root/tests/network_path_stubs" -I "$root" \
    "$root/tests/network_path_test.cpp" "$root/firmware/esp32s3/src/EthernetPort.cpp" \
    -o "$root/build/native/network_path_test"
  "$root/build/native/network_path_test"
done
node "$root/tests/ethernet_discovery_test.mjs"
node "$root/tests/esp_only_maintenance_test.mjs"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/firmware/esp8266/src" "$root/tests/telnet_shell_test.cpp" -o "$root/build/telnet-shell-test"
"$root/build/telnet-shell-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/busy_stubs" -I "$root/firmware/esp8266/src" "$root/tests/busy_line_test.cpp" -o "$root/build/busy-line-test"
"$root/build/busy-line-test"
c++ -std=c++17 -Wall -Wextra -Werror -DF_CPU=16000000UL -I "$root/libraries/BoardCore/src" -I "$root/tests/tone_stubs" -I "$root/firmware/avr" "$root/tests/tone_player_test.cpp" "$root/firmware/avr/TonePlayer.cpp" "$root/firmware/avr/BootMelody.cpp" -o "$root/build/tone-player-test"
"$root/build/tone-player-test"
c++ -std=c++17 -Wall -Wextra -Werror -pthread -I "$root/tests/access_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/esp8266/src" \
  "$root/tests/device_identity_test.cpp" "$root/firmware/esp8266/src/DeviceIdentity.cpp" -o "$root/build/device-identity-test"
"$root/build/device-identity-test"
for identity_flags in -DPAKAR_TABLE_ENABLED=1 -DPAKAR_TABLE_ENABLED=0 -DPAKAR_IDENTITY_CUSTOM=1; do
  c++ -std=c++17 -Wall -Wextra -Werror "$identity_flags" -I "$root/tests/access_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/esp8266/src" "$root/tests/application_identity_test.cpp" -o "$root/build/application-identity-test"
  "$root/build/application-identity-test"
done
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/libraries/BoardCore/src" \
  "$root/tests/access_policy_test.cpp" -o "$root/build/access-policy-test"
"$root/build/access-policy-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/libraries/BoardCore/src" "$root/tests/device_context_test.cpp" -o "$root/build/device-context-test"
"$root/build/device-context-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/access_stubs" -I "$root/libraries/BoardCore/src" -I "$HOME/Arduino/libraries/ArduinoJson/src" -I "$root/firmware/esp8266/src" \
  "$root/tests/access_config_test.cpp" "$root/firmware/esp8266/src/AccessConfig.cpp" -o "$root/build/access-config-test"
"$root/build/access-config-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/access_stubs" -I "$root/firmware/esp8266/src" \
  "$root/tests/async_http_test.cpp" -pthread -o "$root/build/async-http-test"
"$root/build/async-http-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/access_stubs" -I "$root/firmware/esp8266/src" \
  "$root/tests/firmware_range_test.cpp" -o "$root/build/firmware-range-test"
"$root/build/firmware-range-test"
c++ -std=c++17 -Wall -Wextra -Werror \
  -I "$root/tests/stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/avr" \
  "$root/tests/front_panel_test.cpp" "$root/firmware/avr/FrontPanel.cpp" \
  -o "$root/build/front-panel-test"
"$root/build/front-panel-test"
c++ -std=c++17 -Wall -Wextra -Werror \
  -I "$root/tests/pc_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/avr" \
  "$root/tests/pc_contract_test.cpp" "$root/firmware/avr/PcControllerRuntime.cpp" \
  "$root/firmware/avr/BoardSettings.cpp" "$root/firmware/avr/SevenSegments.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/MacroRing.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/Keys.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/ControllerEvents.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/AutomationStore.cpp" \
  -o "$root/build/pc-contract-test"
"$root/build/pc-contract-test"
c++ -std=c++17 -Wall -Wextra -Werror \
  -I "$root/tests/pc_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/avr" -I "$root/firmware/esp8266/src" \
  "$root/tests/esp_front_panel_test.cpp" "$root/firmware/avr/PcControllerRuntime.cpp" \
  "$root/firmware/avr/BoardSettings.cpp" "$root/firmware/avr/SevenSegments.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/MacroRing.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/Keys.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/ControllerEvents.cpp" \
  "$root/libraries/BoardCore/src/pccontroller/AutomationStore.cpp" -o "$root/build/esp-front-panel-test"
"$root/build/esp-front-panel-test"
for rgb_case in s3_status_rgb_test s3_status_rgb_config_test s3_passive_avr_rx_test; do
  c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/access_stubs" -I "$root/firmware/esp32s3/src" \
    "$root/tests/$rgb_case.cpp" -o "$root/build/$rgb_case"
  "$root/build/$rgb_case"
done
pwsh -NoProfile -File "$root/tests/device_network_script_test.ps1"
pwsh -NoProfile -File "$root/tests/avr_rx_script_test.ps1"
pwsh -NoProfile -File "$root/tools/consult-reuse.ps1" -Concern tm1637-display -Gate > /dev/null
for display_flags in '' '-D__AVR__'; do
  c++ -std=c++17 -Wall -Wextra -Werror $display_flags -I "$root/tests/display_stubs" -I "$root/firmware/avr" \
    "$root/tests/tm1637_bus_test.cpp" "$root/firmware/avr/SevenSegments.cpp" -o "$root/build/tm1637-bus-test"
  "$root/build/tm1637-bus-test"
done
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/pixel_stubs" -I "$root/libraries/BoardCore/src" -I "$root/firmware/esp8266/src" \
  "$root/tests/led_grid_test.cpp" "$root/firmware/esp8266/src/LedGrid.cpp" -o "$root/build/led-grid-test"
"$root/build/led-grid-test"
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/libraries/BoardCore/src" -I "$root/firmware/esp32s3/src" \
  "$root/tests/s3_menu_audio_test.cpp" -o "$root/build/s3-menu-audio-test"
"$root/build/s3-menu-audio-test"
pwsh -NoProfile -File "$root/tools/test-uart-flow.ps1"
pwsh -NoProfile -File "$root/tools/consult-reuse.ps1" -Concern peer-binding -Gate > /dev/null
c++ -std=c++17 -Wall -Wextra -Werror -I "$root/tests/access_stubs" -I "$root/libraries/BoardCore/src" -I "$HOME/Arduino/libraries/ArduinoJson/src" -I "$root/firmware/esp8266/src" \
  "$root/tests/web_access_fence_test.cpp" "$root/firmware/esp8266/src/AccessConfig.cpp" -o "$root/build/web-access-fence-test"
"$root/build/web-access-fence-test"
pwsh -NoProfile -File "$root/tools/test-tasks.ps1"
