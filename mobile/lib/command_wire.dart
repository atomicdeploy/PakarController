import 'dart:convert';
import 'dart:math';

import 'models.dart';
import 'cell_search.dart';

const commandNames = {
  '/api/cells': 'cells',
  '/api/cell': 'cell',
  '/api/effect': 'effect',
  '/api/mode': 'mode',
  '/api/brightness': 'brightness',
  '/api/led/output': 'led-output',
  '/api/led/diagnostic': 'pixel-diagnostic',
};

String commandPacket(
  DeviceStatus status,
  String path,
  Map<String, String> values,
) {
  final random = Random.secure();
  final id = List.generate(
    16,
    (_) => random.nextInt(256).toRadixString(16).padLeft(2, '0'),
  ).join();
  final clock = status.json['command_clock_ms'];
  if (clock is! int ||
      clock < 0 ||
      clock > 0x1fffffffffffff - 8000 ||
      !status.peer.allows(path, values)) {
    throw const FormatException('Fresh command admission unavailable');
  }
  return jsonEncode({
    'type': 'command',
    'request_id': id,
    'board_id': status.json['board_id'],
    'boot_id': status.json['boot_id'],
    'expires_at_ms': clock + 8000,
    'command': commandNames[path],
    for (final item in values.entries)
      item.key:
          item.key == 'id' ||
              (path == '/api/led/diagnostic' &&
                  ['index', 'count', 'rgb'].contains(item.key)) ||
              path == '/api/brightness' && item.key == 'value'
          ? int.parse(item.value)
          : item.key == 'enabled' || item.key == 'off'
          ? item.value == 'true'
          : item.value,
  });
}

DeviceStatus verifyCommandReply(
  Map<String, dynamic> reply,
  String packet,
  String path,
  Map<String, String> values,
) {
  final request = jsonDecode(packet) as Map<String, dynamic>;
  if (reply['type'] != 'command-result' ||
      reply['request_id'] != request['request_id'] ||
      reply['board_id'] != request['board_id'] ||
      reply['boot_id'] != request['boot_id']) {
    throw const FormatException(
      'Foreign command receipt; no new intent was sent',
    );
  }
  if (reply['accepted'] != true) {
    throw FormatException('Command rejected: ${reply['error'] ?? 'unknown'}');
  }
  if (path == '/api/cells' &&
      reply['count'] != parseCellSearch(values['ids'] ?? '').length) {
    throw const FormatException('Atomic selection receipt count differs');
  }
  final json = reply['status'];
  if (json is! Map<String, dynamic> ||
      json['board_id'] != request['board_id'] ||
      json['boot_id'] != request['boot_id']) {
    throw const FormatException('Same-board command readback unavailable');
  }
  return DeviceStatus(json);
}
