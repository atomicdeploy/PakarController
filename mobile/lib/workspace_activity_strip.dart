import 'package:flutter/material.dart';

/// Adapted from PCController mobile/lib/unified/workspace_chrome.dart
/// ActivityStrip: stable targets, selected activity reveal, bounded touch
/// controls and a horizontally scrolling phone bar. Board-discovered IDs
/// replace the donor's fixed media/controller activity array.
class WorkspaceActivityStrip extends StatefulWidget {
  const WorkspaceActivityStrip({
    super.key,
    required this.pages,
    required this.selected,
    required this.changed,
  });
  final List<(String, String, IconData)> pages;
  final String selected;
  final ValueChanged<String> changed;
  @override
  State<WorkspaceActivityStrip> createState() => _WorkspaceActivityStripState();
}

class _WorkspaceActivityStripState extends State<WorkspaceActivityStrip> {
  final targets = <String, GlobalKey>{};
  void reveal({bool animate = false}) {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      final target = targets[widget.selected]?.currentContext;
      if (mounted && target != null) {
        Scrollable.ensureVisible(
          target,
          alignment: .5,
          duration: animate ? const Duration(milliseconds: 180) : Duration.zero,
        );
      }
    });
  }

  @override
  void initState() {
    super.initState();
    reveal();
  }

  @override
  void didUpdateWidget(covariant WorkspaceActivityStrip oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.selected != widget.selected) reveal(animate: true);
  }

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    final current = widget.pages.map((item) => item.$1).toSet();
    targets.removeWhere((id, _) => !current.contains(id));
    return Material(
      color: colors.surfaceContainer,
      child: SafeArea(
        top: false,
        child: SingleChildScrollView(
          scrollDirection: Axis.horizontal,
          padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 6),
          child: Row(
            children: [
              for (final item in widget.pages)
                Padding(
                  key: targets.putIfAbsent(item.$1, GlobalKey.new),
                  padding: const EdgeInsets.symmetric(horizontal: 3),
                  child: Semantics(
                    selected: item.$1 == widget.selected,
                    child: Tooltip(
                      message: item.$2,
                      child: Material(
                        color: item.$1 == widget.selected
                            ? colors.primary.withValues(alpha: .13)
                            : Colors.transparent,
                        shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(12),
                          side: BorderSide(
                            color: item.$1 == widget.selected
                                ? colors.primary.withValues(alpha: .38)
                                : Colors.transparent,
                          ),
                        ),
                        child: InkWell(
                          borderRadius: BorderRadius.circular(12),
                          onTap: () => widget.changed(item.$1),
                          child: Container(
                            constraints: const BoxConstraints(
                              minHeight: 48,
                              minWidth: 48,
                            ),
                            padding: const EdgeInsets.symmetric(horizontal: 12),
                            child: Row(
                              mainAxisSize: MainAxisSize.min,
                              children: [
                                Icon(
                                  item.$3,
                                  size: 20,
                                  color: item.$1 == widget.selected
                                      ? colors.primary
                                      : colors.onSurfaceVariant,
                                ),
                                const SizedBox(width: 9),
                                Text(
                                  item.$2,
                                  style: TextStyle(
                                    color: item.$1 == widget.selected
                                        ? colors.primary
                                        : colors.onSurfaceVariant,
                                    fontWeight: item.$1 == widget.selected
                                        ? FontWeight.w700
                                        : FontWeight.w500,
                                  ),
                                ),
                              ],
                            ),
                          ),
                        ),
                      ),
                    ),
                  ),
                ),
            ],
          ),
        ),
      ),
    );
  }
}
