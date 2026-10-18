import 'package:flutter/material.dart';

/// User-owned UI state, independent of telemetry leases and transport sockets.
/// Like Pealayer's stable workspace IDs, a board ID owns a workspace, not a
/// connection attempt or boot. Nothing here authorizes or replays a command.
class TableWorkspace {
  TableWorkspace({Map<String, dynamic> saved = const {}}) {
    final ids = saved['selection'];
    if (ids is List &&
        ids.length <= 80 &&
        ids.every((id) => id is int && id >= 1 && id <= 80)) {
      selection = ids.cast<int>().toSet();
    }
    final text = saved['query'];
    query.text = text is String && text.length <= 512 ? text : '';
    multiple = saved['multiple'] != false;
    showIndicators = saved['indicators'] == true;
    final effect = saved['effect'];
    cellEffect = ['immediate', 'fade', 'pulse'].contains(effect)
        ? effect as String
        : 'immediate';
    double offset(Object? value) => value is num && value.isFinite && value >= 0
        ? value.toDouble().clamp(0, 100000)
        : 0;
    vertical = offset(saved['vertical']);
    horizontal = offset(saved['horizontal']);
    pageScroll = ScrollController(initialScrollOffset: vertical)
      ..addListener(() {
        if (pageScroll.hasClients) vertical = pageScroll.offset;
      });
    tableScroll = ScrollController(initialScrollOffset: horizontal)
      ..addListener(() {
        if (tableScroll.hasClients) horizontal = tableScroll.offset;
      });
  }
  final query = TextEditingController();
  final bucket = PageStorageBucket();
  late final ScrollController pageScroll, tableScroll;
  Set<int> selection = {};
  bool multiple = true, showIndicators = false;
  String cellEffect = 'immediate';
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
  void dispose() {
    query.dispose();
    pageScroll.dispose();
    tableScroll.dispose();
  }
}
