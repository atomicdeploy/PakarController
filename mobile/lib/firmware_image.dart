import 'dart:typed_data';

enum EspFirmwarePlatform { esp8266, esp32s3 }

/// Same raw-image admission as tools/create-remote-manifest.mjs. Limits are
/// exclusive slot capacities, not a generic allocation limit or flash-chip size.
/// The maximum merely bounds document/network intake; it never validates bytes.
abstract final class FirmwareImagePolicy {
  static const s3Capacity = 0x330000;
  static const legacyCapacity = 0x100000;
  static const maximumBytes = s3Capacity - 1;
  static const avrBytes = 32768;

  static int maximum({
    required bool esp,
    EspFirmwarePlatform? platform,
    int? otaCapacity,
  }) {
    if (!esp) {
      return avrBytes;
    }
    final fixed = platform == EspFirmwarePlatform.esp8266
        ? legacyCapacity
        : s3Capacity;
    if (otaCapacity != null && otaCapacity <= 24) {
      throw const FormatException(
        'Invalid advertised OTA application capacity',
      );
    }
    final capacity = otaCapacity == null || otaCapacity > fixed
        ? fixed
        : otaCapacity;
    return capacity - 1;
  }

  static EspFirmwarePlatform? platform(Map<String, dynamic>? status) {
    final board = status?['board_id'];
    if (board is String && board.startsWith('esp32s3-')) {
      return EspFirmwarePlatform.esp32s3;
    }
    if (board is String && board.startsWith('esp8266-')) {
      return EspFirmwarePlatform.esp8266;
    }
    return null;
  }

  /// Consume only a capacity explicitly observed in the selected status. Never
  /// infer the slot size from free heap, total flash or an unbound info response.
  static int? capacity(Map<String, dynamic>? status) {
    final nested = status?['esp'];
    final value = status?.containsKey('ota_capacity_bytes') == true
        ? status!['ota_capacity_bytes']
        : nested is Map && nested.containsKey('ota_capacity_bytes')
        ? nested['ota_capacity_bytes']
        : null;
    if (value == null) {
      return null;
    }
    if (value is! int || value <= 24 || value > 0xffffffff) {
      throw const FormatException(
        'Invalid advertised OTA application capacity',
      );
    }
    return value;
  }
}

void validateFirmwareImage(
  bool esp,
  Uint8List bytes, {
  EspFirmwarePlatform? platform,
  int? otaCapacity,
}) {
  if (!esp) {
    if (bytes.length != FirmwareImagePolicy.avrBytes) {
      throw const FormatException(
        'Use exactly 32768 raw AVR bytes including the intended bootloader; not partial application or HEX',
      );
    }
    return;
  }
  if (bytes.length < 24 ||
      bytes.first != 0xe9 ||
      bytes[1] < 1 ||
      bytes[1] > 16) {
    throw const FormatException(
      'Use a complete uncompressed ESP E9 application with 1–16 segments',
    );
  }
  final chip = ByteData.sublistView(bytes).getUint16(12, Endian.little);
  // ESP8266 has no extended header: byte 23 is arbitrary application data,
  // not an ESP32 hash flag. Do not misclassify legitimate legacy code bytes.
  final s3Candidate =
      chip == 9 || bytes.length >= FirmwareImagePolicy.legacyCapacity;
  final selected =
      platform ??
      (s3Candidate ? EspFirmwarePlatform.esp32s3 : EspFirmwarePlatform.esp8266);
  if (selected == EspFirmwarePlatform.esp32s3 &&
      (chip != 9 || bytes[23] != 1)) {
    throw const FormatException(
      'Use a hashed ESP32-S3 application image, not another chip or merged flash image',
    );
  }
  if (selected == EspFirmwarePlatform.esp8266 && chip == 9) {
    throw const FormatException(
      'ESP32-S3 firmware cannot update an ESP8266 board',
    );
  }
  if (bytes.length >
      FirmwareImagePolicy.maximum(
        esp: true,
        platform: selected,
        otaCapacity: otaCapacity,
      )) {
    throw const FormatException(
      'Firmware must fit strictly inside the selected OTA application slot',
    );
  }
}
