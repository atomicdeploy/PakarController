import 'settings_catalog.dart';
import 'peer_context.dart';
import 'upgrade_observation.dart';

class DeviceStatus {
  DeviceStatus(this.json);
  final Map<String, dynamic> json;
  late final peer = PeerContext.fromStatus(json);
  UpgradeObservation? get upgrade => UpgradeObservation.read(
    json['upgrade'],
    board: json['board_id'],
    boot: json['boot_id'],
  );

  /// A progress packet cannot invent a peer, replace full status or refresh a
  /// hardware lease. Keep only its newer, same-boot lifecycle observation.
  DeviceStatus? withUpgradeEvent(Map<String, dynamic> event) {
    if (event['type'] != 'update-progress') return null;
    final next = UpgradeObservation.read(
      event,
      board: json['board_id'],
      boot: json['boot_id'],
    );
    if (next == null || (upgrade?.revision ?? -1) >= next.revision) return null;
    return DeviceStatus({...json, 'upgrade': next.json});
  }

  DeviceStatus preservingNewerUpgrade(DeviceStatus? current) {
    final previous = current?.upgrade;
    if (previous == null ||
        current?.json['board_id'] != json['board_id'] ||
        current?.json['boot_id'] != json['boot_id'] ||
        (upgrade?.revision ?? -1) >= previous.revision) {
      return this;
    }
    return DeviceStatus({...json, 'upgrade': previous.json});
  }

  // An older in-flight HTTP read must not undo a newer WS command receipt.
  // This comparison is only meaningful within the exact same board boot.
  bool olderThan(DeviceStatus? current) {
    final clock = json['command_clock_ms'];
    final previous = current?.json['command_clock_ms'];
    return json['board_id'] != null &&
        json['boot_id'] != null &&
        json['board_id'] == current?.json['board_id'] &&
        json['boot_id'] == current?.json['boot_id'] &&
        clock is int &&
        previous is int &&
        clock < previous;
  }

  /// Shared catalog, filtered by this peer's explicit ACKs. The caller still
  /// owns freshness; this DTO never infers capabilities from pin/pixel counts.
  List<Map<String, dynamic>> get deviceSettingsPages {
    return (deviceSettingsCatalog['pages'] as List)
        .cast<Map<String, dynamic>>()
        .where(
          (page) =>
              (page['features'] as List).isEmpty ||
              (page['features'] as List).any((feature) => peer.has(feature)),
        )
        .toList(growable: false);
  }

  Map<String, dynamic> get observedBuildInformation => {
    for (final key in ['version', 'link_mode', 'pixels', 'avr_clock_hz'])
      if (json.containsKey(key)) key: json[key],
  };

  String get hostname => json['hostname'] as String? ?? '';
  String get mode => json['mode'] as String? ?? '';
  String get stationIp => json['station_ip'] as String? ?? '';
  int? get rssi => json['station_rssi'] as int?;
  bool get avrOnline => json['avr_online'] == true;
  List<String> get audioCues {
    final reported = json['audio_cues'];
    if (!peer.has('buzzer') || !avrOnline || reported is! List) return const [];
    return reported
        .whereType<String>()
        .where((cue) => RegExp(r'^[a-z0-9][a-z0-9-]{0,39}$').hasMatch(cue))
        .toSet()
        .take(64)
        .toList();
  }

  bool get tmReady => json['tm1637_ack'] == true;
  List<String> get lcdLines => peer.has('lcd') && lcdObservationReady(json)
      ? ((json['lcd'] as Map)['lines'] as List).cast<String>()
      : const [];
  bool get mdns => json['mdns_started'] == true;
  bool get ssdp => json['ssdp_started'] == true;
  bool get mqtt => json['mqtt_connected'] == true;
  bool get ntp => json['ntp_synced'] == true;
  String get effect => json['effect'] as String? ?? 'none';
  int get brightness => json['brightness'] as int? ?? 255;
  bool? get ledOutputEnabled {
    if (!peer.has('leds')) return null;
    final output = json['led_output'];
    return output is Map && output['enabled'] is bool
        ? output['enabled'] as bool
        : null;
  }

  String? get ledOutputMode {
    if (!peer.has('leds')) return null;
    final output = json['led_output'];
    final value = output is Map ? output['mode'] : null;
    return value == 'static' || value == 'effects' ? value as String : null;
  }

  int get winner => json['winner'] as int? ?? 0;
  int get selectedCell => json['selected_cell'] as int? ?? 1;
  int get activeInputs => json['active_inputs'] as int? ?? 0;
  int get activeOutputs => json['active_outputs'] as int? ?? 0;
  List<String> get cells => (json['cells'] as List? ?? const [])
      .map((value) => value.toString())
      .toList(growable: false);
  List<int> get segments {
    final values = json['tm1637_segments'];
    if (values is! List ||
        values.length != 4 ||
        values.any((v) => v is! int || v < 0 || v > 255)) {
      return const [];
    }
    return values.cast<int>();
  }

  int get segmentBrightness => json['tm1637_brightness'] as int? ?? 0;
}

const audioCueNames = [
  'welcome',
  'update-start',
  'update-success',
  'error',
  'winner',
  'finish',
  'lost',
  'incorrect',
  'rapid-error',
  'fault',
  'increase',
  'decrease',
  'welcome-puzzleboard',
  'welcome-40led',
  'attention',
];

class CellMap {
  CellMap({required this.physical, required this.names});
  final List<int> physical;
  final List<String> names;

  bool get valid =>
      physical.length == 80 &&
      names.length == 80 &&
      physical.toSet().length == 80 &&
      physical.every((v) => v >= 1 && v <= 80) &&
      names.every((v) => validText(v, 15));

  factory CellMap.fromJson(Map<String, dynamic> json) {
    final physical = json['physical'], names = json['names'];
    if (physical is! List ||
        names is! List ||
        physical.any((v) => v is! int) ||
        names.any((v) => v is! String)) {
      throw const FormatException('Physical cell mapping unavailable');
    }
    final map = CellMap(
      physical: physical.cast<int>(),
      names: names.cast<String>(),
    );
    if (!map.valid) {
      throw const FormatException('Invalid physical cell mapping');
    }
    return map;
  }

  factory CellMap.defaults() => CellMap(
    physical: List.generate(80, (index) => index + 1),
    names: List.generate(80, (index) => '${index + 1}'),
  );
}
