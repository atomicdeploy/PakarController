import 'dart:convert';

import 'models.dart' show audioCueNames;

const presentationLimits = <String, (int, int)>{
  'flashes': (0, 10),
  'flash_on_ms': (50, 5000),
  'flash_off_ms': (50, 5000),
  'fade_ms': (0, 10000),
  'pulse_ms': (100, 10000),
  'chase_ms': (100, 20000),
};

Map<String, Object> presentationPatch(Object? raw) {
  if (raw is! Map || raw.isEmpty) {
    throw const FormatException('Empty presentation');
  }
  final result = <String, Object>{};
  for (final entry in raw.entries) {
    final range = presentationLimits[entry.key];
    if (range != null) {
      final n = entry.value;
      if (n is! int || n < range.$1 || n > range.$2) {
        throw FormatException('Invalid ${entry.key}');
      }
      result[entry.key as String] = n;
    } else if (entry.key == 'groups') {
      final groups = entry.value;
      if (groups is! List || groups.isEmpty || groups.length > 18) {
        throw const FormatException('Invalid presentation groups');
      }
      final seen = <int>{};
      result['groups'] = [
        for (final group in groups)
          (() {
            if (group is! Map ||
                group['id'] is! int ||
                group['id'] < 1 ||
                group['id'] > 18 ||
                !seen.add(group['id'] as int)) {
              throw const FormatException('Invalid group identity');
            }
            final copy = <String, Object>{};
            for (final field in group.entries) {
              final v = field.value;
              final valid = switch (field.key) {
                'id' => true,
                'override_colors' => v is bool,
                'win_rgb' || 'lose_rgb' => v is int && v >= 0 && v <= 0xffffff,
                'win_motion' || 'lose_motion' => v is int && v >= 0 && v <= 3,
                _ => false,
              };
              if (!valid) throw const FormatException('Invalid group setting');
              copy[field.key as String] = v as Object;
            }
            return copy;
          })(),
      ];
    } else {
      throw const FormatException('Unknown presentation setting');
    }
  }
  return result;
}

bool presentationMatches(Object? actual, Object? requested) {
  if (requested is Map) {
    return actual is Map &&
        requested.entries.every(
          (e) => presentationMatches(actual[e.key], e.value),
        );
  }
  if (requested is List) {
    return actual is List &&
        requested.every(
          (g) =>
              g is Map &&
              actual.any(
                (a) =>
                    a is Map && a['id'] == g['id'] && presentationMatches(a, g),
              ),
        );
  }
  return actual == requested;
}

bool gameMelodyPatch(Map<String, String> values) =>
    values.containsKey('winner_cue') || values.containsKey('reset_cue');

/// The ESP, WebSocket and USB transports all accept the same partial game
/// settings patch. Keep values typed before a request can reach the board.
Map<String, Object> gameSettingsPatch(Map<String, String> values) {
  const limits = <String, int>{
    'enabled_mask': 511,
    'idle_rgb': 0xffffff,
    'win_rgb': 0xffffff,
    'lose_rgb': 0xffffff,
    'auto_reset_ms': 86400000,
    'chase_period_ms': 3000,
  };
  if (values.isEmpty) throw const FormatException('Empty game settings patch');
  final patch = <String, Object>{};
  for (final entry in values.entries) {
    if (entry.key == 'presentation') {
      patch[entry.key] = presentationPatch(jsonDecode(entry.value));
      continue;
    }
    if (entry.key == 'winner_cue' || entry.key == 'reset_cue') {
      if (entry.value != 'silent' && !audioCueNames.contains(entry.value)) {
        throw const FormatException('Invalid game melody');
      }
      patch[entry.key] = entry.value;
      continue;
    }
    if (entry.key == 'extended_effects' ||
        entry.key == 'chase_neighbors' ||
        entry.key == 'chase_invert') {
      if (entry.value != 'true' && entry.value != 'false') {
        throw const FormatException('Invalid game effect setting');
      }
      patch[entry.key] = entry.value == 'true';
      continue;
    }
    final limit = limits[entry.key];
    if (!RegExp(r'^(?:0|[1-9][0-9]*)$').hasMatch(entry.value)) {
      throw const FormatException('Invalid game setting');
    }
    final value = int.tryParse(entry.value);
    if (limit == null ||
        value == null ||
        value < 0 ||
        value > limit ||
        (entry.key == 'enabled_mask' && value == 0) ||
        (entry.key == 'chase_period_ms' && value < 400)) {
      throw const FormatException('Invalid game setting');
    }
    patch[entry.key] = value;
  }
  return patch;
}
