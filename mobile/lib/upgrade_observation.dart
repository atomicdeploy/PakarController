/// Board-owned presentation of one update, never authorization for a mutation.
/// Adapt PCController's explicit measured/unknown and ordered progress rules.
class UpgradeObservation {
  UpgradeObservation._(this.json);
  final Map<String, dynamic> json;
  int get revision => json['revision'] as int;
  int get operation => json['operation_id'] as int;
  String get source => json['source'] as String;
  String get phase => json['phase'] as String;
  int get received => json['received_bytes'] as int;
  int get expected => json['expected_bytes'] as int;
  bool get active => json['active'] as bool;
  String get error => json['error'] as String;
  bool get verified =>
      !active && phase == 'complete' && json['completion_verified'] == true;
  bool get measured =>
      {
        'receiving',
        'download_esp',
        'download_avr',
        'backup_flash',
        'backup_eeprom',
        'verify_flash',
        'verify_eeprom',
        'verify_esp',
        'program_avr',
        'program_esp',
      }.contains(phase) &&
      json['progress_known'] == true &&
      expected > 0 &&
      received <= expected &&
      json['progress_percent'] is int &&
      json['progress_percent'] == received * 100 ~/ expected;

  static UpgradeObservation? read(
    Object? value, {
    required Object? board,
    required Object? boot,
  }) {
    bool identity(Object? v) =>
        v is String && RegExp(r'^[!-~]{1,80}$').hasMatch(v);
    if (!identity(board) || !identity(boot) || value is! Map) return null;
    if (value.keys.any((key) => key is! String)) return null;
    final raw = Map<String, dynamic>.from(value);
    bool uint(String key) =>
        raw[key] is int && raw[key] >= 0 && raw[key] <= 0xffffffff;
    if (raw['board_id'] != board ||
        raw['boot_id'] != boot ||
        !identity(raw['source_boot_id']) ||
        !{
          'local-ota',
          'remote-firmware',
          'avr-maintenance',
        }.contains(raw['source']) ||
        !uint('operation_id') ||
        raw['operation_id'] == 0 ||
        !uint('revision') ||
        raw['phase'] is! String ||
        !RegExp(r'^[a-z][a-z0-9_-]{0,47}$').hasMatch(raw['phase']) ||
        raw['active'] is! bool ||
        raw['progress_known'] is! bool ||
        raw['completion_verified'] is! bool ||
        !uint('received_bytes') ||
        !uint('expected_bytes') ||
        raw['expected_bytes'] > 0 &&
            raw['received_bytes'] > raw['expected_bytes'] ||
        raw['error'] is! String ||
        (raw['error'] as String).length > 1024) {
      return null;
    }
    // A verified post-reboot local receipt legitimately names its source boot.
    // Only the current board/boot envelope establishes observation ownership.
    return UpgradeObservation._(Map.unmodifiable(raw));
  }
}
