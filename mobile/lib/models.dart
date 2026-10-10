class DeviceStatus {
  DeviceStatus(this.json);
  final Map<String, dynamic> json;

  String get hostname => json['hostname'] as String? ?? '';
  String get mode => json['mode'] as String? ?? '';
  String get stationIp => json['station_ip'] as String? ?? '';
  int? get rssi => json['station_rssi'] as int?;
  bool get avrOnline => json['avr_online'] == true;
  bool get tmReady => json['tm1637_ack'] == true;
  bool get mdns => json['mdns_started'] == true;
  bool get ssdp => json['ssdp_started'] == true;
  bool get mqtt => json['mqtt_connected'] == true;
  bool get ntp => json['ntp_synced'] == true;
  String get effect => json['effect'] as String? ?? 'none';
  int get brightness => json['brightness'] as int? ?? 255;
  int get winner => json['winner'] as int? ?? 0;
  int get selectedCell => json['selected_cell'] as int? ?? 1;
  int get activeInputs => json['active_inputs'] as int? ?? 0;
  int get activeOutputs => json['active_outputs'] as int? ?? 0;
  List<String> get cells => (json['cells'] as List? ?? const [])
      .map((value) => value.toString())
      .toList(growable: false);
  List<int> get segments => (json['tm1637_segments'] as List? ?? const [])
      .map((value) => value as int)
      .toList(growable: false);
  int get segmentBrightness => json['tm1637_brightness'] as int? ?? 0;
}

class CellMap {
  CellMap({required this.physical, required this.names});
  final List<int> physical;
  final List<String> names;

  factory CellMap.fromJson(Map<String, dynamic> json) => CellMap(
    physical: (json['physical'] as List? ?? const [])
        .map((value) => value as int)
        .toList(growable: false),
    names: (json['names'] as List? ?? const [])
        .map((value) => value.toString())
        .toList(growable: false),
  );

  factory CellMap.defaults() => CellMap(
    physical: List.generate(80, (index) => index + 1),
    names: List.generate(80, (index) => '${index + 1}'),
  );
}
