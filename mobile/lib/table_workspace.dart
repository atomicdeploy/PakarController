import 'dart:async';

import 'package:flutter/material.dart';

import 'device_settings.dart';
import 'pixel_diagnostic.dart';
import 'cell_search.dart';
import 'table_viewport.dart';

/// User-owned UI state, independent of telemetry leases and transport sockets.
/// Like Pealayer's stable workspace IDs, a board ID owns a workspace, not a
/// connection attempt or boot. Nothing here authorizes or replays a command.
class TableWorkspace extends ChangeNotifier {
  final viewport = TableViewportController();
  TableWorkspace({Map<String, dynamic> saved = const {}}) {
    Map<String, dynamic> section(String key) =>
        saved[key] is Map<String, dynamic>
        ? saved[key] as Map<String, dynamic>
        : const {};
    pixels = PixelDiagnosticWorkspace(saved: section('pixel_diagnostic'))
      ..addListener(notifyListeners);
    frontPanel = FrontPanelWorkspace(saved: section('front_panel'))
      ..addListener(notifyListeners);
    settings = DeviceSettingsWorkspace(saved: section('settings'))
      ..addListener(notifyListeners);
    final ids = saved['selection'];
    final hasSavedSelection =
        ids is List &&
        ids.length <= 80 &&
        ids.every((id) => id is int && id >= 1 && id <= 80);
    if (hasSavedSelection) {
      _selection = ids.cast<int>().toSet();
    }
    final text = saved['query'];
    query.text = text is String && text.length <= 512 ? text : '';
    if (!saved.containsKey('selection')) {
      try {
        _selection = parseCellSearch(query.text);
      } on FormatException {
        /* Keep no selection. */
      }
    }
    query.text = formatCellSelection(_selection);
    // Requested presentation refresh: existing installations switch to color
    // names once. A later explicit States choice is retained independently of
    // firmware state identity, telemetry, selection and connection lifecycle.
    _colorLabels =
        saved['cell_label_preferences'] != 1 ||
        saved['cell_labels'] != 'states';
    _multiple = saved['multiple'] == true;
    _showIndicators = saved['indicators'] != false;
    final order = saved['output_cards'];
    if (order is List) {
      _outputCards = [
        ...order.whereType<String>().where(outputCardIds.contains).toSet(),
        ...outputCardIds.where((id) => !order.contains(id)),
      ];
    }
    final effect = saved['effect'];
    _cellEffect = ['immediate', 'fade', 'pulse'].contains(effect)
        ? effect as String
        : 'immediate';
    final keyPositions = saved['key_positions'];
    final selectedKeys = saved['key_selection'];
    if (selectedKeys is List) {
      _selectedKeys.addAll(
        selectedKeys.whereType<int>().where((key) => key >= 1 && key <= 9),
      );
    }
    if (keyPositions is Map) {
      for (var key = 1; key <= 9; key++) {
        final raw = keyPositions['$key'];
        if (raw is List &&
            raw.length == 2 &&
            raw[0] is num &&
            raw[1] is num &&
            (raw[0] as num).isFinite &&
            (raw[1] as num).isFinite) {
          _keyPositions[key] = Offset(
            (raw[0] as num).toDouble().clamp(0, 1).toDouble(),
            (raw[1] as num).toDouble().clamp(0, 1).toDouble(),
          );
        }
      }
    }
    double offset(Object? value) => value is num && value.isFinite && value >= 0
        ? value.toDouble().clamp(0, 100000)
        : 0;
    vertical = offset(saved['vertical']);
    horizontal = offset(saved['horizontal']);
    final view = section('viewport');
    double finite(Object? value, double fallback) =>
        value is num && value.isFinite ? value.toDouble() : fallback;
    viewport.change(
      finite(view['zoom'], 1),
      Offset(
        finite(view['x'], 0).clamp(-100000, 100000),
        finite(view['y'], 0).clamp(-100000, 100000),
      ),
    );
    viewport.addListener(notifyListeners);
    pageScroll = _RetainedScrollController(initialOffset: vertical)
      ..addListener(() {
        if (pageScroll.hasClients && vertical != pageScroll.offset) {
          vertical = pageScroll.offset;
          notifyListeners();
        }
      });
    tableScroll = _RetainedScrollController(initialOffset: horizontal)
      ..addListener(() {
        if (tableScroll.hasClients && horizontal != tableScroll.offset) {
          horizontal = tableScroll.offset;
          notifyListeners();
        }
      });
    var previousText = query.text;
    query.addListener(() {
      if (query.text == previousText) return; // Caret movement is not a save.
      previousText = query.text;
      notifyListeners();
    });
  }
  final query = TextEditingController();
  late final PixelDiagnosticWorkspace pixels;
  late final FrontPanelWorkspace frontPanel;
  late final DeviceSettingsWorkspace settings;
  final bucket = PageStorageBucket();
  static const outputCardIds = ['scene', 'output', 'diagnostics'];
  List<String> _outputCards = List.of(outputCardIds);
  List<String> get outputCards => List.unmodifiable(_outputCards);
  void reorderOutputCard(int oldIndex, int newIndex) {
    if (oldIndex < 0 ||
        oldIndex >= _outputCards.length ||
        newIndex < 0 ||
        newIndex >= _outputCards.length) {
      return;
    }
    if (oldIndex == newIndex) return;
    final item = _outputCards.removeAt(oldIndex);
    _outputCards.insert(newIndex, item);
    notifyListeners();
  }

  late final ScrollController pageScroll, tableScroll;
  Set<int> _selection = {};
  Set<int> get selection => Set.unmodifiable(_selection);
  set selection(Set<int> value) {
    if (value.length == _selection.length && _selection.containsAll(value)) {
      return;
    }
    _selection = Set.of(value);
    notifyListeners();
  }

  bool _multiple = false, _showIndicators = true;
  bool _colorLabels = true;
  bool get colorLabels => _colorLabels;
  set colorLabels(bool value) {
    if (_colorLabels == value) return;
    _colorLabels = value;
    notifyListeners();
  }

  bool get multiple => _multiple;
  set multiple(bool value) {
    if (_multiple == value) return;
    _multiple = value;
    notifyListeners();
  }

  bool get showIndicators => _showIndicators;
  set showIndicators(bool value) {
    if (_showIndicators == value) return;
    _showIndicators = value;
    notifyListeners();
  }

  String _cellEffect = 'immediate';
  String get cellEffect => _cellEffect;
  set cellEffect(String value) {
    if (_cellEffect == value) return;
    _cellEffect = value;
    notifyListeners();
  }

  static Offset defaultKeyPosition(int key) {
    final index = (key - 1).clamp(0, 8);
    return Offset((index % 3) / 2, (index ~/ 3) / 2);
  }

  final Map<int, Offset> _keyPositions = {};
  final Set<int> _selectedKeys = {};
  Set<int> get selectedKeys => Set.unmodifiable(_selectedKeys);
  void selectKeys(Set<int> keys) {
    final next = keys.where((key) => key >= 1 && key <= 9).toSet();
    if (_selectedKeys.length == next.length &&
        _selectedKeys.containsAll(next)) {
      return;
    }
    _selectedKeys
      ..clear()
      ..addAll(next);
    notifyListeners();
  }

  Offset keyPosition(int key) => _keyPositions[key] ?? defaultKeyPosition(key);
  void moveKey(int key, Offset position) {
    if (key < 1 || key > 9 || !position.dx.isFinite || !position.dy.isFinite) {
      return;
    }
    final next = Offset(
      position.dx.clamp(0, 1).toDouble(),
      position.dy.clamp(0, 1).toDouble(),
    );
    if (keyPosition(key) == next) return;
    _keyPositions[key] = next;
    notifyListeners();
  }

  void resetKeyPositions() {
    if (_keyPositions.isEmpty) return;
    _keyPositions.clear();
    notifyListeners();
  }

  double vertical = 0, horizontal = 0;
  Map<String, dynamic> snapshot() => {
    'query': query.text.length <= 512
        ? query.text
        : query.text.substring(0, 512),
    'selection': selection.toList()..sort(),
    'cell_labels': colorLabels ? 'colors' : 'states',
    'cell_label_preferences': 1,
    'multiple': multiple,
    'indicators': showIndicators,
    'effect': cellEffect,
    'output_cards': outputCards,
    'key_positions': {
      for (var key = 1; key <= 9; key++)
        '$key': [keyPosition(key).dx, keyPosition(key).dy],
    },
    'key_selection': _selectedKeys.toList()..sort(),
    'vertical': vertical,
    'horizontal': horizontal,
    'viewport': {
      'zoom': viewport.zoom,
      'x': viewport.pan.dx,
      'y': viewport.pan.dy,
    },
    'pixel_diagnostic': pixels.snapshot(),
    'front_panel': frontPanel.snapshot(),
    'settings': settings.snapshot(),
  };
  @override
  void dispose() {
    viewport.dispose();
    query.dispose();
    pageScroll.dispose();
    tableScroll.dispose();
    pixels.dispose();
    frontPanel.dispose();
    settings.dispose();
    super.dispose();
  }
}

/// A presentation draft, never an observed display or queued melody command.
class FrontPanelWorkspace extends ChangeNotifier {
  FrontPanelWorkspace({Map<String, dynamic> saved = const {}}) {
    final value = saved['text'];
    text.text = value is String && value.length <= 40
        ? value
        : 'PAKAR CONTROLLER';
    _repeat = ['once', 'loop', 'interval'].contains(saved['repeat'])
        ? saved['repeat'] as String
        : 'once';
    final ms = saved['speed'];
    _speed = ms is num && ms.isFinite && ms >= 80 && ms <= 5000
        ? ms.toDouble()
        : 260;
    _forceScroll = saved['force_scroll'] == true;
    final cue = saved['cue'];
    _selectedCue = cue is String && cue.length <= 64 ? cue : null;
    int draft(String key, int fallback, int min, int max) =>
        saved[key] is int &&
            (saved[key] as int) >= min &&
            (saved[key] as int) <= max
        ? saved[key] as int
        : fallback;
    _number = draft('number', 0, -999, 9999);
    _holdMs = draft('hold_ms', 1500, 0, 60000);
    _intervalSeconds = draft('interval_seconds', 5, 1, 255);
    _frequency = draft('frequency', 880, 20, 12000);
    _durationMs = draft('duration_ms', 150, 1, 5000);
    final offset = saved['vertical'];
    vertical = offset is num && offset.isFinite && offset >= 0
        ? offset.toDouble().clamp(0, 100000)
        : 0;
    scroll = _RetainedScrollController(initialOffset: vertical)
      ..addListener(() {
        if (scroll.hasClients && vertical != scroll.offset) {
          vertical = scroll.offset;
          notifyListeners();
        }
      });
    var prior = text.text;
    text.addListener(() {
      if (text.text == prior) return;
      prior = text.text;
      notifyListeners();
    });
  }
  final text = TextEditingController();
  late final ScrollController scroll;
  double vertical = 0, _speed = 260;
  String _repeat = 'once';
  String? _selectedCue;
  bool _forceScroll = false;
  int _number = 0,
      _holdMs = 1500,
      _intervalSeconds = 5,
      _frequency = 880,
      _durationMs = 150;
  int get number => _number;
  set number(int value) {
    if (value < -999 || value > 9999 || value == _number) return;
    _number = value;
    notifyListeners();
  }

  int get holdMs => _holdMs;
  set holdMs(int value) {
    if (value < 0 || value > 60000 || value == _holdMs) return;
    _holdMs = value;
    notifyListeners();
  }

  int get intervalSeconds => _intervalSeconds;
  set intervalSeconds(int value) {
    if (value < 1 || value > 255 || value == _intervalSeconds) return;
    _intervalSeconds = value;
    notifyListeners();
  }

  int get frequency => _frequency;
  set frequency(int value) {
    if (value < 20 || value > 12000 || value == _frequency) return;
    _frequency = value;
    notifyListeners();
  }

  int get durationMs => _durationMs;
  set durationMs(int value) {
    if (value < 1 || value > 5000 || value == _durationMs) return;
    _durationMs = value;
    notifyListeners();
  }

  double get speed => _speed;
  set speed(double value) {
    if (!value.isFinite || value < 80 || value > 5000 || value == _speed) {
      return;
    }
    _speed = value;
    notifyListeners();
  }

  String get repeat => _repeat;
  set repeat(String value) {
    if (!['once', 'loop', 'interval'].contains(value) || value == _repeat) {
      return;
    }
    _repeat = value;
    notifyListeners();
  }

  bool get forceScroll => _forceScroll;
  set forceScroll(bool value) {
    if (value == _forceScroll) return;
    _forceScroll = value;
    notifyListeners();
  }

  String? get selectedCue => _selectedCue;
  set selectedCue(String? value) {
    if (value == _selectedCue || value != null && value.length > 64) return;
    _selectedCue = value;
    notifyListeners();
  }

  Map<String, dynamic> snapshot() => {
    'text': text.text.length > 40 ? text.text.substring(0, 40) : text.text,
    'repeat': repeat,
    'speed': speed,
    'force_scroll': forceScroll,
    if (selectedCue != null) 'cue': selectedCue,
    'number': number,
    'hold_ms': holdMs,
    'interval_seconds': intervalSeconds,
    'frequency': frequency,
    'duration_ms': durationMs,
    'vertical': vertical,
  };
  @override
  void dispose() {
    text.dispose();
    scroll.dispose();
    super.dispose();
  }
}

/// Reattachment uses the last actual offset, including a drag interrupted by
/// screen-off. PageStorage's last scroll-end value must not override it.
class _RetainedScrollController extends ScrollController {
  _RetainedScrollController({required double initialOffset})
    : retainedOffset = initialOffset,
      super(keepScrollOffset: false) {
    addListener(() {
      if (hasClients) retainedOffset = offset;
    });
  }
  double retainedOffset;
  @override
  double get initialScrollOffset => retainedOffset;
}

/// One ordered writer for UI snapshots. Coalesce changes during a pending
/// platform write; an older completion cannot overwrite the latest workspace.
/// A failed write stays retryable on the next change/lifecycle flush, not a loop.
class WorkspaceCacheWriter {
  WorkspaceCacheWriter({required this.write, this.saved = ''});
  final Future<bool> Function(String) write;
  String saved;
  String? _pending;
  Future<void>? _saving;

  Future<void> save(String encoded) {
    _pending = encoded;
    if (_saving != null) return _saving!;
    final completion = Completer<void>();
    _saving = completion.future;
    unawaited(_drain(completion));
    return completion.future;
  }

  Future<void> _drain(Completer<void> completion) async {
    try {
      while (_pending != null) {
        final next = _pending!;
        if (next != saved) {
          if (!await write(next)) throw StateError('Workspace save failed');
          saved = next;
        }
        if (_pending == next) _pending = null;
      }
      completion.complete();
    } catch (error, stack) {
      completion.completeError(error, stack);
    } finally {
      _saving = null;
    }
  }
}
