import 'dart:async';

import 'package:flutter/material.dart';

import 'device_settings.dart';
import 'pixel_diagnostic.dart';

/// User-owned UI state, independent of telemetry leases and transport sockets.
/// Like Pealayer's stable workspace IDs, a board ID owns a workspace, not a
/// connection attempt or boot. Nothing here authorizes or replays a command.
class TableWorkspace extends ChangeNotifier {
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
    if (ids is List &&
        ids.length <= 80 &&
        ids.every((id) => id is int && id >= 1 && id <= 80)) {
      _selection = ids.cast<int>().toSet();
    }
    final text = saved['query'];
    query.text = text is String && text.length <= 512 ? text : '';
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
    double offset(Object? value) => value is num && value.isFinite && value >= 0
        ? value.toDouble().clamp(0, 100000)
        : 0;
    vertical = offset(saved['vertical']);
    horizontal = offset(saved['horizontal']);
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

  double vertical = 0, horizontal = 0;
  Map<String, dynamic> snapshot() => {
    'query': query.text.length <= 512
        ? query.text
        : query.text.substring(0, 512),
    'selection': selection.toList()..sort(),
    'multiple': multiple,
    'indicators': showIndicators,
    'effect': cellEffect,
    'output_cards': outputCards,
    'vertical': vertical,
    'horizontal': horizontal,
    'pixel_diagnostic': pixels.snapshot(),
    'front_panel': frontPanel.snapshot(),
    'settings': settings.snapshot(),
  };
  @override
  void dispose() {
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
    _speed = ms is num && ms.isFinite && ms >= 80 && ms <= 1000
        ? ms.toDouble()
        : 260;
    _forceScroll = saved['force_scroll'] == true;
    final cue = saved['cue'];
    _selectedCue = cue is String && cue.length <= 64 ? cue : null;
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
  double get speed => _speed;
  set speed(double value) {
    if (!value.isFinite || value < 80 || value > 1000 || value == _speed) {
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
