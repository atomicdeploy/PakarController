import 'dart:convert';

const minigameConfigure = '/api/minigame/configure';
const minigameControl = '/api/minigame/control';
const minigameInput = '/api/minigame/input';
const minigamePaths = {minigameConfigure, minigameControl, minigameInput};

// These are local command keys; all transports send the peer-bound envelope to
// /api/command. They are never independent HTTP write routes.
const minigameLimits = <String, (int, int)>{
  'limit_ms': (0, 86400000),
  'warning_ms': (0, 86400000),
  'urgent_ms': (0, 86400000),
  'penalty_ms': (0, 86400000),
  'window_ms': (100, 60000),
  'show_ms': (100, 10000),
  'gap_ms': (50, 5000),
  'feedback_ms': (50, 5000),
  'intro_ms': (100, 10000),
  'preview_ms': (0, 10000),
  'mismatch_ms': (600, 10000),
  'rounds': (1, 32),
  'sequence_length': (1, 32),
  'palette_size': (2, 9),
  'fixed_target': (0, 9),
};
const minigameDescriptions = <String, String>{
  'press-to-win': 'First press wins; reset for the next round',
  'illuminated': 'Press the glowing target before its window closes',
  'color-match': 'Cycle button colors to match the indicator colors',
  'simon': 'Watch, remember and repeat the light sequence',
  'memory-pairs': 'Reveal colors and find every matching pair',
  'clear-all': 'Press every illuminated button to clear the board',
  'color-order': 'Press the boxes in palette color order',
  'switch-pattern': 'Toggle buttons to copy the indicator pattern',
  'reaction-chase': 'Catch the moving light inside its response window',
  'toggle': 'Control latched or momentary outputs without a timer',
};
const minigameOutputSources = [
  'disabled',
  'active',
  'success',
  'failure',
  'held',
  'latched',
  'winner',
  'lost',
];

class MiniGameSnapshot {
  MiniGameSnapshot._(this.json, this.modes, this.settings);
  final Map<String, dynamic> json;
  final Map<String, String> modes;
  final Map<String, dynamic> settings;
  String get mode => json['mode'] as String;
  String get phase => json['phase'] as String;
  int number(String key) => json[key] as int;
  bool get ready => json['ready'] == true;
  bool get running => const ['show', 'input', 'feedback'].contains(phase);
  bool get acceptsInput =>
      ready &&
      (phase == 'input' ||
          (phase == 'ready' &&
              const ['press-to-win', 'toggle'].contains(mode)));
  bool get usesHold =>
      mode == 'color-match' ||
      (mode == 'toggle' && settings['toggle_kind'] == 'press');
  String description(String id) {
    for (final entry in json['available_modes'] as List) {
      if (entry['id'] == id && entry['description'] is String) {
        return entry['description'] as String;
      }
    }
    return minigameDescriptions[id] ?? 'Board-provided game';
  }

  static MiniGameSnapshot? read(Object? raw) {
    if (raw is! Map ||
        raw['ready'] is! bool ||
        raw['settings'] is! Map ||
        raw['available_modes'] is! List ||
        raw['timed'] is! bool) {
      return null;
    }
    final modes = <String, String>{};
    for (final mode in raw['available_modes'] as List) {
      if (mode is! Map ||
          mode['id'] is! String ||
          mode['label'] is! String ||
          (mode['id'] as String).isEmpty ||
          (mode['label'] as String).isEmpty ||
          modes.containsKey(mode['id'])) {
        return null;
      }
      modes[mode['id'] as String] = mode['label'] as String;
    }
    if (!modes.containsKey(raw['mode']) ||
        !const [
          'ready',
          'show',
          'input',
          'feedback',
          'success',
          'timeout',
          'failed',
          'paused',
          'stopped',
        ].contains(raw['phase'])) {
      return null;
    }
    for (final key in [
      'revision',
      'elapsed_ms',
      'remaining_ms',
      'participating_mask',
      'held_mask',
      'latched_mask',
      'winner',
      'target',
      'round',
      'progress',
      'mistakes',
      'warning',
      'last_reaction_ms',
      'best_reaction_ms',
    ]) {
      if (raw[key] is! int || raw[key] < 0 || raw[key] > 0xffffffff) {
        return null;
      }
    }
    for (final key in ['participating_mask', 'held_mask', 'latched_mask']) {
      if (raw[key] > 511) return null;
    }
    if (raw['winner'] > 9 || raw['target'] > 9) return null;
    return MiniGameSnapshot._(
      Map<String, dynamic>.from(raw),
      modes,
      Map<String, dynamic>.from(raw['settings'] as Map),
    );
  }

  Map<String, Object> command(String path, Map<String, String> fields) {
    if (!ready) throw const FormatException('Minigame is not ready');
    return _validatedFields(path, fields);
  }

  Map<String, Object> _validatedFields(
    String path,
    Map<String, String> fields,
  ) {
    if (path == minigameConfigure) {
      if (fields.length != 1 || !fields.containsKey('settings')) {
        throw const FormatException('Expected minigame settings');
      }
      final raw = jsonDecode(fields['settings']!);
      if (raw is! Map || raw.isEmpty) {
        throw const FormatException('Empty minigame settings');
      }
      final patch = <String, Object>{};
      for (final entry in raw.entries) {
        final range = minigameLimits[entry.key], value = entry.value;
        bool valid;
        if (range != null) {
          valid = value is int && value >= range.$1 && value <= range.$2;
        } else {
          valid = switch (entry.key) {
            'mode' => modes.containsKey(value),
            'hard' ||
            'intro_enabled' ||
            'color_arpeggio' ||
            'segment_animation' ||
            'timer_beeps' ||
            'timer_alternate_tones' => value is bool,
            'boot_mode' => const ['first', 'last'].contains(value),
            'toggle_kind' => const ['toggle', 'latch', 'press'].contains(value),
            'outputs' => validOutputs(value),
            _ => false,
          };
        }
        if (!valid) throw FormatException('Invalid ${entry.key}');
        patch[entry.key as String] = value as Object;
      }
      final merged = {...settings, ...patch};
      if (merged['urgent_ms'] is int &&
          merged['warning_ms'] is int &&
          merged['urgent_ms'] > merged['warning_ms']) {
        throw const FormatException(
          'Urgent threshold exceeds warning threshold',
        );
      }
      return {'settings': patch};
    }
    if (path == minigameControl &&
        fields.length == 1 &&
        const [
          'start',
          'pause',
          'resume',
          'reset',
          'stop',
        ].contains(fields['action'])) {
      return {'action': fields['action']!};
    }
    if (path == minigameInput &&
        fields.length == 2 &&
        RegExp(r'^[1-9]$').hasMatch(fields['key'] ?? '') &&
        const ['tap', 'down', 'up'].contains(fields['action'])) {
      return {'key': int.parse(fields['key']!), 'action': fields['action']!};
    }
    throw const FormatException('Invalid minigame command');
  }

  bool allows(String path, Map<String, String> fields) {
    try {
      command(path, fields);
      return true;
    } on FormatException {
      return false;
    }
  }
}

bool validOutputs(Object? raw) =>
    raw is List &&
    raw.length == 8 &&
    raw.every(
      (rule) =>
          rule is Map &&
          rule.length == 2 &&
          minigameOutputSources.contains(rule['source']) &&
          rule['key'] is int &&
          (minigameOutputSources.indexOf(rule['source'] as String) < 4
              ? rule['key'] == 0
              : rule['key'] >= 1 && rule['key'] <= 9),
    );

bool minigameReadbackMatches(
  Object? raw,
  String path,
  Map<String, String> fields,
) {
  final game = MiniGameSnapshot.read(raw);
  if (game == null) return false;
  if (path == minigameConfigure) {
    try {
      // A successful save starts an AVR input fence and can report ready=false.
      // Validate the saved values without granting admission for another write.
      final requested = game._validatedFields(path, fields)['settings'] as Map;
      return requested.entries.every(
        (entry) => minigameValueMatches(game.settings[entry.key], entry.value),
      );
    } on FormatException {
      return false;
    }
  }
  // Timed input can immediately advance into feedback/success. The correlated
  // receipt proves acceptance; a later phase must not be mistaken for failure.
  return true;
}

bool minigameValueMatches(Object? actual, Object? requested) {
  if (requested is Map) {
    return actual is Map &&
        requested.length == actual.length &&
        requested.entries.every(
          (entry) => minigameValueMatches(actual[entry.key], entry.value),
        );
  }
  if (requested is List) {
    return actual is List &&
        requested.length == actual.length &&
        List.generate(
          requested.length,
          (i) => i,
        ).every((i) => minigameValueMatches(actual[i], requested[i]));
  }
  return actual == requested;
}
