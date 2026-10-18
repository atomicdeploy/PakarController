import 'dart:async';

import 'package:flutter/material.dart';

/// User-owned UI state, independent of telemetry leases and transport sockets.
/// Like Pealayer's stable workspace IDs, a board ID owns a workspace, not a
/// connection attempt or boot. Nothing here authorizes or replays a command.
class TableWorkspace extends ChangeNotifier {
  TableWorkspace({Map<String, dynamic> saved = const {}}) {
    final ids = saved['selection'];
    if (ids is List &&
        ids.length <= 80 &&
        ids.every((id) => id is int && id >= 1 && id <= 80)) {
      _selection = ids.cast<int>().toSet();
    }
    final text = saved['query'];
    query.text = text is String && text.length <= 512 ? text : '';
    _multiple = saved['multiple'] != false;
    _showIndicators = saved['indicators'] == true;
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
  final bucket = PageStorageBucket();
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

  bool _multiple = true, _showIndicators = false;
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
    'vertical': vertical,
    'horizontal': horizontal,
  };
  @override
  void dispose() {
    query.dispose();
    pageScroll.dispose();
    tableScroll.dispose();
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
