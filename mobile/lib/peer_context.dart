import 'dart:convert';
import 'dart:typed_data';

/// Actual PCController peripheral-availability.ts ready AND advertised rule,
/// adapted through Pakar's living capability manifest (capabilities.rs).
/// No table, peripheral or application identity is inferred from pixel counts.
class PeerContext {
  PeerContext._(
    this.known,
    this.kind,
    this.name,
    this.hostname,
    this.features,
    this.table,
    this.modes,
    this.application,
    this.ledOutput,
    this.role,
  );
  static final unknown = PeerContext._(
    false,
    '',
    '',
    '',
    const {},
    false,
    const [],
    null,
    null,
    '',
  );
  final bool known;
  final String kind, name, hostname;
  final Set<String> features;
  final bool table;
  final List<PeerMode> modes;
  final PeerApplication? application;
  final LedOutputCapability? ledOutput;
  final String role;
  bool get tableSelection =>
      table &&
      (role == 'table-selection' ||
          (role.isEmpty && modes.any((mode) => mode.id == 'cell')));

  static PeerContext fromStatus(Map<String, dynamic> status) {
    final value = status['capabilities'];
    if (value is! Map ||
        value['board_kind'] is! String ||
        !validText(value['board_kind'], 64) ||
        value['acknowledged_features'] is! List) {
      return unknown;
    }
    final host = value['hostname'];
    if (host is! String || !validHostname(host) || status['hostname'] != host) {
      return unknown; // Mixed manifests must never authorize another board.
    }
    final rawFeatures = value['acknowledged_features'];
    final features = rawFeatures is List
        ? rawFeatures.whereType<String>().where(knownFeatures.contains).toSet()
        : <String>{};
    final geometry = value['table'];
    final table =
        features.contains('table') &&
        value['board_kind'] == 'table' &&
        geometry is Map &&
        geometry['columns'] is int &&
        geometry['columns'] == 10 &&
        geometry['rows'] is int &&
        geometry['rows'] == 8 &&
        geometry['pixels_per_cell'] is int &&
        geometry['pixels_per_cell'] == 6;
    // A geometry claim alone cannot create a Table feature.
    if (!table) features.remove('table');
    // The measured TM1637 bus ACK is independent from AVR connectivity/text.
    if (status['tm1637_ack'] != true) features.remove('display');
    if (!lcdObservationReady(status)) features.remove('lcd');
    final rawModes = value['modes'];
    final rawRole = value['role'];
    final role = const ['table-selection', 'first-wins'].contains(rawRole)
        ? rawRole as String
        : '';
    if (role != 'table-selection') {
      features.removeAll({'cell-assignments', 'cell-lock'});
    }
    final modes = <PeerMode>[];
    if (table && rawModes is List) {
      for (final mode in rawModes) {
        if (mode is! Map || mode['code'] is! int) continue;
        final id = mode['id'], code = mode['code'], kind = mode['kind'];
        final valid =
            (id == 'cell' && code == 1 && kind == 'presentation') ||
            (id == 'first-wins' &&
                code == 2 &&
                kind == 'game' &&
                features.contains('inputs') &&
                features.contains('outputs'));
        final matchesRole =
            !value.containsKey('role') ||
            (role == 'table-selection' && id == 'cell') ||
            (role == 'first-wins' && id == 'first-wins');
        if (!valid || !matchesRole || modes.any((item) => item.id == id)) {
          continue;
        }
        final label = mode['label'];
        modes.add(
          PeerMode(id as String, validText(label, 96) ? label as String : id),
        );
      }
    }
    final name = value['name'];
    return PeerContext._(
      true,
      value['board_kind'] is String ? value['board_kind'] : '',
      validText(name, 63) ? name as String : 'Connected board',
      host,
      Set.unmodifiable(features),
      table,
      List.unmodifiable(modes),
      PeerApplication.parse(value['application']),
      features.contains('leds')
          ? LedOutputCapability.parse(value['led_output'])
          : null,
      role,
    );
  }

  bool has(String feature) => known && features.contains(feature);
  bool get frontPanel =>
      has('display') || has('text') || has('buzzer') || has('lcd');
  bool allows(String path, Map<String, String> values) {
    if (!known) return false;
    return switch (path) {
      '/api/cell' || '/api/cells/mapping' => tableSelection,
      '/api/game/reset' => modes.any((mode) => mode.id == 'first-wins'),
      '/api/cells' => tableSelection && has('cell-batch'),
      '/api/cells/assignments' => tableSelection && has('cell-assignments'),
      '/api/cells/lock' => tableSelection && has('cell-lock'),
      '/api/mode' => modes.any((mode) => mode.id == values['value']),
      '/api/display' => has(values.containsKey('text') ? 'text' : 'display'),
      '/api/audio' || '/api/buzzer' => has('buzzer'),
      '/api/outputs' => has('outputs'),
      '/api/brightness' || '/api/effect' || '/api/led/output' => has('leds'),
      '/api/led/diagnostic' => has('leds') && has('pixel-diagnostic'),
      '/api/device/name' => has('identity'),
      '/api/network' || '/api/cloud' => has('network'),
      _ => false, // Add an explicit contract before exposing a new mutation.
    };
  }

  String get title => known
      ? '${application?.title ?? 'PakarController'} · $name · $hostname.local'
      : 'PakarController';
}

/// Selected board wire implementation, not animation mode or active/ready state.
/// Missing, mistyped or contradictory metadata is unknown, never false.
class LedOutputCapability {
  const LedOutputCapability._(this.backend, this.rmt, this.dma);
  final String backend;
  final bool rmt, dma;
  static LedOutputCapability? parse(Object? raw) {
    if (raw is! Map || raw['rmt'] is! bool || raw['dma'] is! bool) return null;
    final backend = raw['backend'], rmt = raw['rmt'], dma = raw['dma'];
    if (!((backend == 'rmt-dma' && rmt == true && dma == true) ||
        ((backend == 'gpio-static' || backend == 'fastled') &&
            rmt == false &&
            dma == false))) {
      return null;
    }
    return LedOutputCapability._(backend as String, rmt as bool, dma as bool);
  }

  String get label => backend == 'rmt-dma'
      ? 'RMT + DMA'
      : backend == 'gpio-static'
      ? 'GPIO · no RMT/DMA'
      : 'FastLED · no RMT/DMA';
}

const knownFeatures = {
  'usb-demand',
  'usb-pixel-diagnostic',
  'status-rgb',
  'command-ack',
  'pixel-diagnostic',
  'network',
  'telemetry',
  'leds',
  'streaming',
  'table',
  'cell-batch',
  'cell-assignments',
  'cell-lock',
  'display',
  'text',
  'buzzer',
  'inputs',
  'outputs',
  'settings',
  'isp',
  'ota',
  'identity',
  'lcd',
};
bool lcdObservationReady(Map<String, dynamic> status) {
  final lcd = status['lcd'];
  if (lcd is! Map ||
      lcd['configured'] != true ||
      lcd['started'] != true ||
      lcd['detection_known'] != true ||
      lcd['backpack_ack'] != true ||
      lcd['initialized'] != true ||
      lcd['available'] != true ||
      lcd['columns'] != 16 ||
      lcd['rows'] != 2 ||
      !const [0x27, 0x3f].contains(lcd['address'])) {
    return false;
  }
  final lines = lcd['lines'];
  return lines is List &&
      lines.length == 2 &&
      lines.every(
        (line) =>
            line is String &&
            line.length == 16 &&
            line.codeUnits.every((code) => code >= 0x20 && code <= 0x7e),
      );
}

bool validHostname(String value) =>
    value.length <= 63 &&
    !{'local', 'localhost'}.contains(value) &&
    RegExp(r'^[a-z0-9](?:[a-z0-9-]*[a-z0-9])?$').hasMatch(value);
bool validText(Object? value, int limit) =>
    value is String &&
    value.trim().isNotEmpty &&
    utf8.encode(value).length <= limit &&
    !RegExp(r'[\x00-\x1f\x7f-\x9f]').hasMatch(value);

class PeerMode {
  const PeerMode(this.id, this.label);
  final String id, label;
}

/// Same bounded, inline-only identity validation as desktop identity.rs.
/// Invalid icons never cause a remote fetch or replace the default branding.
class PeerApplication {
  PeerApplication._(this.id, this.name, this.title, this.icon);
  final String id, name, title;
  final Uint8List? icon;
  static PeerApplication? parse(Object? raw) {
    if (raw is! Map ||
        !validText(raw['id'], 96) ||
        !validText(raw['name'], 96) ||
        !validText(raw['title'], 160)) {
      return null;
    }
    Uint8List? icon;
    final encoded = raw['icon_png_base64'];
    if (encoded is String && encoded.length <= 4096) {
      try {
        final bytes = base64Decode(encoded);
        if (bytes.length >= 33 &&
            bytes.take(8).join(',') == '137,80,78,71,13,10,26,10' &&
            ascii.decode(bytes.sublist(12, 16), allowInvalid: true) == 'IHDR') {
          final data = ByteData.sublistView(bytes);
          final width = data.getUint32(16), height = data.getUint32(20);
          if (width >= 8 && width <= 256 && height >= 8 && height <= 256) {
            icon = bytes;
          }
        }
      } on FormatException {
        /* Identity still valid, artwork unavailable. */
      }
    }
    return PeerApplication._(raw['id'], raw['name'], raw['title'], icon);
  }
}
