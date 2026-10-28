import 'dart:convert';
import 'dart:math';

import 'models.dart';
import 'cell_search.dart';
import 'command_failure.dart';
import 'cell_assignments.dart';
import 'game_settings_command.dart';
import 'command_readback.dart';
import 'minigame.dart';

const commandNames = {
  minigameConfigure: 'minigame-configure',
  minigameControl: 'minigame-control',
  minigameInput: 'minigame-input',
  '/api/audio': 'audio',
  '/api/buzzer': 'buzzer',
  '/api/outputs': 'outputs',
  '/api/led/presentation': 'state-presentation',
  '/api/led/preview': 'color-preview',
  '/api/game/press': 'game-press',
  '/api/game/reset': 'reset',
  '/api/game/settings': 'game-settings',
  '/api/cells': 'cells',
  '/api/cell': 'cell',
  '/api/cells/assignments': 'cell-assignments',
  '/api/cells/lock': 'cell-lock',
  '/api/effect': 'effect',
  '/api/mode': 'mode',
  '/api/brightness': 'brightness',
  '/api/led/output': 'led-output',
  '/api/led/diagnostic': 'pixel-diagnostic',
  '/api/pixels/corrections/preview': 'pixel-corrections-preview',
};

// Receipt ownership is identity, not admission for a subsequent mutation. A
// valid configure/start receipt can temporarily report ready=false during the
// AVR input fence. Never turn that successful handoff into a disconnection.
bool commandPeerMatches(DeviceStatus? actual, Object? board, Object? boot) =>
    board is String &&
    board.isNotEmpty &&
    boot is String &&
    boot.isNotEmpty &&
    actual?.json['board_id'] == board &&
    actual?.json['boot_id'] == boot;

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
  final gameSettings = path == '/api/game/settings'
      ? gameSettingsPatch(values)
      : null;
  final miniFields = minigamePaths.contains(path)
      ? status.peer.minigame!.command(path, values)
      : null;
  return jsonEncode({
    'type': 'command',
    'request_id': id,
    'board_id': status.json['board_id'],
    'boot_id': status.json['boot_id'],
    'expires_at_ms': clock + 8000,
    'command': commandNames[path],
    ...?gameSettings,
    ...?miniFields,
    if (gameSettings == null && miniFields == null)
      for (final item in values.entries)
        item.key:
            path == '/api/pixels/corrections/preview' &&
                item.key == 'configuration'
            ? jsonDecode(item.value)
            : path == '/api/led/presentation' && item.key == 'settings'
            ? jsonDecode(item.value)
            : path == '/api/led/preview' &&
                  ['rgb', 'duration_ms'].contains(item.key)
            ? int.parse(item.value)
            : path == '/api/led/preview' && item.key == 'cancel'
            ? item.value == 'true'
            : path == '/api/cells/assignments' && item.key == 'assignments'
            ? CellAssignments.fromRows(jsonDecode(item.value)).wireAssignments
            : (path == '/api/buzzer' &&
                      ['frequency', 'duration_ms'].contains(item.key)) ||
                  (path == '/api/outputs' && item.key == 'mask') ||
                  item.key == 'id' ||
                  (path == '/api/game/press' && item.key == 'key') ||
                  (path == '/api/led/diagnostic' &&
                      [
                        'index',
                        'count',
                        'rgb',
                        'period_ms',
                        'duty_percent',
                        'min_brightness',
                        'max_brightness',
                        'group_layout_revision',
                      ].contains(item.key)) ||
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
  Map<String, String> values, {
  bool requireGameReadback = true,
}) {
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
    throw DeviceCommandRejected(error: reply['reason'] ?? reply['error']);
  }
  if (const ['/api/audio', '/api/buzzer', '/api/outputs'].contains(path) &&
      reply['avr_acknowledged'] != true) {
    throw const FormatException(
      'AVR command acknowledgment missing; command was not retried',
    );
  }
  if ((path == '/api/game/reset' || path == '/api/game/press') &&
      reply['avr_acknowledged'] != true) {
    throw const FormatException(
      'AVR reset acknowledgment missing; reset was not retried',
    );
  }
  if (path == '/api/cells' &&
      reply['count'] != parseCellSearch(values['ids'] ?? '').length) {
    throw const FormatException('Atomic selection receipt count differs');
  }
  if (path == '/api/cells/assignments' &&
      reply['count'] !=
          CellAssignments.fromRows(jsonDecode(values['assignments'] ?? ''))
              .values
              .length) {
    throw const FormatException('Atomic assignment receipt count differs');
  }
  final json = reply['status'];
  if (json is! Map<String, dynamic> ||
      json['board_id'] != request['board_id'] ||
      json['boot_id'] != request['boot_id']) {
    throw const FormatException('Same-board command readback unavailable');
  }
  final actual = DeviceStatus(json);
  if (minigamePaths.contains(path) &&
      !minigameReadbackMatches(json['minigame'], path, values)) {
    throw const FormatException('Minigame readback differs; not retried');
  }
  if (requireGameReadback &&
      path == '/api/game/settings' &&
      !commandReadbackMatches(actual, path, values)) {
    throw const FormatException(
      'Game settings receipt differs from device readback; not retried',
    );
  }
  return actual;
}
