import 'dart:async';

import 'package:flutter/material.dart';

/// Adapts PCController EffectEditor's guarded dismiss to a shared tab route.
/// TabBar changes its controller before onTap: retire that speculative change
/// synchronously, then change the actual page only after its exit guard agrees.
class PixelToolsNavigation extends StatefulWidget {
  const PixelToolsNavigation({
    super.key,
    required this.length,
    required this.initialIndex,
    required this.blocked,
    required this.beforeLeave,
    required this.builder,
  });
  final int length, initialIndex;
  final bool blocked;
  final Future<bool> Function(int index) beforeLeave;
  final Widget Function(
    BuildContext context,
    int index,
    TabController controller,
    ValueChanged<int> select,
    VoidCallback close,
  )
  builder;

  @override
  State<PixelToolsNavigation> createState() => _PixelToolsNavigationState();
}

class _PixelToolsNavigationState extends State<PixelToolsNavigation>
    with SingleTickerProviderStateMixin {
  late final controller = TabController(
    length: widget.length,
    initialIndex: widget.initialIndex,
    vsync: this,
  );
  late int index = widget.initialIndex;
  bool leaving = false;

  Future<void> leave({int? next}) async {
    // Keep the old page active (including its preview) while a decision is open.
    if (controller.index != index) controller.index = index;
    if (leaving || widget.blocked || next == index) return;
    leaving = true;
    try {
      // The guard awaits its own pending work. A parent's blocked widget value
      // may still be from the saving frame until the next rebuild, so it must
      // not veto a just-confirmed Save-and-leave result.
      if (!await widget.beforeLeave(index) || !mounted) {
        return;
      }
      if (next == null) {
        Navigator.of(context).pop();
      } else {
        setState(() => index = next);
        controller.index = next;
      }
    } finally {
      leaving = false;
    }
  }

  @override
  void dispose() {
    controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => PopScope(
    canPop: false,
    onPopInvokedWithResult: (didPop, _) {
      if (!didPop) unawaited(leave());
    },
    child: widget.builder(
      context,
      index,
      controller,
      (next) => unawaited(leave(next: next)),
      () => unawaited(leave()),
    ),
  );
}
