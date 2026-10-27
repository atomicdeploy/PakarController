import 'models.dart';
import 'cell_search.dart';

import 'dart:convert';

import 'cell_assignments.dart';
import 'game_settings_command.dart';
import 'semantic_cells.dart';
import 'led_backend.dart';

/// Compare observed command state, never draft values or model defaults.
/// A matching buffer/status is not physical LED/display/audio acceptance.
bool commandReadbackMatches(
  DeviceStatus actual,
  String path,
  Map<String, String> values,
) {
  final json = actual.json;
  switch (path) {
    case '/api/outputs':
      final requested = int.tryParse(values['mask'] ?? '');
      return requested != null && requested >= 0 && requested <= 255 &&
          actual.peer.has('outputs') && actual.avrOnline &&
          json['programming_latch'] == false && json['active_outputs'] == requested;
    case '/api/led/preview':
      final preview = json['color_preview'];
      if (preview is! Map) return false;
      if (values['cancel'] == 'true') return preview['active'] == false;
      return preview['active'] == true && preview['rgb'] == int.tryParse(values['rgb'] ?? '');
    case '/api/led/presentation':
      try {
        return presentationMatches(json['state_presentation'], jsonDecode(values['settings'] ?? ''));
      } on FormatException { return false; }
    case '/api/game/settings':
      final settings = json['key_game'];
      if (settings is! Map) return false;
      if (gameMelodyPatch(values) && json['game_audio_applied'] != true) {
        return false;
      }
      try {
        final requested = gameSettingsPatch(values);
        return requested.entries.every(
          (entry) => presentationMatches(settings[entry.key], entry.value),
        );
      } on FormatException {
        return false;
      }
    case '/api/cells/lock':
      return const ['true', 'false'].contains(values['enabled']) &&
          json['red_lock'] == (values['enabled'] == 'true');
    case '/api/cells/assignments':
      try {
        final assignments = CellAssignments.fromRows(
          jsonDecode(values['assignments'] ?? ''),
        );
        final cells = json['cells'];
        return cells is List &&
            cells.length == 80 &&
            assignments.values.entries.every(
              (entry) => sameCellState(cells[entry.key - 1], entry.value),
            );
      } on Object {
        return false;
      }
    case '/api/cells':
      try {
        final ids = parseCellSearch(values['ids'] ?? '');
        final cells = json['cells'];
        return ids.isNotEmpty &&
            cells is List &&
            ids.every(
              (id) => id <= cells.length && sameCellState(cells[id - 1], values['state']),
            );
      } on FormatException {
        return false;
      }
    case '/api/cell':
      final id = int.tryParse(values['id'] ?? '');
      final cells = json['cells'];
      return id != null &&
          cells is List &&
          id >= 1 &&
          id <= cells.length &&
          sameCellState(cells[id - 1], values['state']);
    case '/api/led/output':
      return (!values.containsKey('backend') ||
              LedBackendState.fromStatus(json)?.matches(values['backend']!) == true) &&
          (!values.containsKey('enabled') ||
              const ['true', 'false'].contains(values['enabled'])) &&
          (!values.containsKey('enabled') ||
              actual.ledOutputEnabled == (values['enabled'] == 'true')) &&
          (!values.containsKey('mode') ||
              actual.ledOutputMode == values['mode']);
    case '/api/brightness':
      final requested = int.tryParse(values['value'] ?? '');
      return requested != null &&
          requested >= 0 &&
          requested <= 255 &&
          json['brightness'] == requested;
    case '/api/effect':
      return json['effect'] == values['value'];
    case '/api/mode':
      return json['mode'] == values['value'];
    default:
      // Other actions (e.g. a finite melody) do not have a persistent status
      // value. The caller reports ACK + fresh status, never verified actuation.
      return true;
  }
}
