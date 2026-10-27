import 'package:flutter/material.dart';

/// Adapted from PCController mobile/lib/unified/workspace_chrome.dart
/// ActivityStrip: stable targets, selected activity reveal, bounded touch
/// controls and a horizontally scrolling overflow bar. Preserve Pakar's original
/// Material footer for a reduced set that fits. Board-discovered IDs replace the
/// donor's fixed media/controller activity array; neither layout hides activities.
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
  (double, bool, double)? lastLayout;
  bool revealPending = false, animateReveal = false;
  void reveal({bool animate = false}) {
    animateReveal |= animate;
    if (revealPending) return;
    revealPending = true;
    WidgetsBinding.instance.addPostFrameCallback((_) {
      revealPending = false;
      final animate = animateReveal;
      animateReveal = false;
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
    final pagesChanged =
        oldWidget.pages.length != widget.pages.length ||
        oldWidget.pages.asMap().entries.any(
          (e) => e.value != widget.pages[e.key],
        );
    if (oldWidget.selected != widget.selected || pagesChanged) {
      reveal(animate: oldWidget.selected != widget.selected);
    }
  }

  bool fitsOriginalFooter(BuildContext context, double width) {
    if (widget.pages.length < 2 || widget.pages.length > 5 || !width.isFinite) {
      return false;
    }
    final itemWidth =
        (width - MediaQuery.paddingOf(context).horizontal) /
        widget.pages.length;
    if (itemWidth < 64) return false;
    final theme = Theme.of(context);
    for (final item in widget.pages) {
      for (final states in [
        <WidgetState>{},
        {WidgetState.selected},
      ]) {
        final painter = TextPainter(
          text: TextSpan(
            text: item.$2,
            style:
                theme.navigationBarTheme.labelTextStyle?.resolve(states) ??
                theme.textTheme.labelMedium,
          ),
          textDirection: Directionality.of(context),
          textScaler: MediaQuery.textScalerOf(context),
          maxLines: 1,
        )..layout();
        final fits = painter.width + 16 <= itemWidth;
        painter.dispose();
        if (!fits) return false;
      }
    }
    return true;
  }

  @override
  Widget build(BuildContext context) {
    final current = widget.pages.map((item) => item.$1).toSet();
    targets.removeWhere((id, _) => !current.contains(id));
    if (widget.pages.isEmpty) return const SizedBox.shrink();
    return LayoutBuilder(
      builder: (context, bounds) {
        final original = fitsOriginalFooter(context, bounds.maxWidth);
        final layout = (
          bounds.maxWidth,
          original,
          MediaQuery.textScalerOf(context).scale(14),
        );
        if (lastLayout != layout) {
          lastLayout = layout;
          if (!original) reveal();
        }
        if (original) {
          final selected = widget.pages.indexWhere(
            (item) => item.$1 == widget.selected,
          );
          return NavigationBar(
            selectedIndex: selected < 0 ? 0 : selected,
            onDestinationSelected: (index) =>
                widget.changed(widget.pages[index].$1),
            destinations: [
              for (final item in widget.pages)
                NavigationDestination(
                  key: ValueKey('activity:${item.$1}'),
                  icon: Icon(item.$3),
                  label: item.$2,
                ),
            ],
          );
        }
        return scrollingStrip(context, bounds.maxWidth);
      },
    );
  }

  Widget scrollingStrip(BuildContext context, double width) {
    final colors = Theme.of(context).colorScheme;
    // Keep one complete touch target within the viewport, even with enlarged
    // RTL labels. The strip scrolls; long labels keep a stable single line.
    final labelWidth = (width - MediaQuery.paddingOf(context).horizontal - 75)
        .clamp(24.0, double.infinity);
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
                                ConstrainedBox(
                                  constraints: BoxConstraints(
                                    maxWidth: labelWidth,
                                  ),
                                  child: Text(
                                    item.$2,
                                    maxLines: 1,
                                    overflow: TextOverflow.ellipsis,
                                    style: TextStyle(
                                      color: item.$1 == widget.selected
                                          ? colors.primary
                                          : colors.onSurfaceVariant,
                                      fontWeight: item.$1 == widget.selected
                                          ? FontWeight.w700
                                          : FontWeight.w500,
                                    ),
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
