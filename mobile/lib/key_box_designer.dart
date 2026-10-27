import 'dart:math' as math;

import 'package:flutter/material.dart';

import 'key_pixel_preview.dart';
import 'models.dart';
import 'table_workspace.dart';
import 'workspace_widgets.dart';

/// PCController ActivityStrip's adaptive accent/Material/InkWell composition,
/// with Pealayer's reserved action gutter, grab-offset-preserving drag and
/// color-only hover. Layout editing never owns the hardware command callback.
class KeyBoxDesigner extends StatefulWidget {
  const KeyBoxDesigner({
    super.key,
    required this.status,
    required this.workspace,
    required this.enabled,
    required this.onTap,
    required this.openSettings,
    this.onHoldStart,
    this.onHoldEnd,
    this.onReset,
    this.onBlockedTap,
    this.blockedReason,
    this.inputKeyPrefix = 'game-key-drag',
  });

  final DeviceStatus status;
  final TableWorkspace workspace;
  final bool enabled;
  final ValueChanged<int> onTap;
  final VoidCallback openSettings;
  final ValueChanged<int>? onHoldStart;
  final VoidCallback? onHoldEnd;
  final VoidCallback? onReset;
  final ValueChanged<int>? onBlockedTap;
  final String? blockedReason;
  final String inputKeyPrefix;

  @override
  State<KeyBoxDesigner> createState() => _KeyBoxDesignerState();
}

class _KeyBoxDesignerState extends State<KeyBoxDesigner> {
  bool arranging = false, multiple = false;
  int? dragging;
  Offset? pointer;
  final dragOrigins = <int, Offset>{};
  Offset dragDelta = Offset.zero;

  int get mask {
    final config = widget.status.json['key_game'];
    return config is Map && config['enabled_mask'] is int
        ? config['enabled_mask'] as int
        : 0;
  }

  bool active(int key) => mask & (1 << (key - 1)) != 0;
  int number(String name) {
    final raw = widget.status.json['minigame'];
    return raw is Map && raw[name] is int ? raw[name] as int : 0;
  }

  bool pressed(int key) =>
      number('held_mask') & (1 << (key - 1)) != 0 ||
      (key <= 8
          ? widget.status.activeInputs & (1 << (key - 1)) != 0
          : widget.status.json['input_9'] == true);

  int get winner =>
      number('winner') > 0 ? number('winner') : widget.status.winner;

  @override
  void didUpdateWidget(covariant KeyBoxDesigner oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.workspace != widget.workspace ||
        oldWidget.status.json['board_id'] != widget.status.json['board_id']) {
      arranging = false;
      dragging = null;
      dragOrigins.clear();
    }
  }

  void select(int key) {
    if (!multiple) {
      widget.workspace.selectKeys(
        widget.workspace.selectedKeys.contains(key) ? {} : {key},
      );
      return;
    }
    final next = multiple ? {...widget.workspace.selectedKeys} : <int>{};
    if (!next.add(key)) next.remove(key);
    widget.workspace.selectKeys(next);
  }

  Future<void> menu(int key) async {
    final enabled = active(key);
    final owner = (
      widget.status.json['board_id'],
      widget.status.json['boot_id'],
    );
    bool sameOwner() {
      if (!mounted) return false;
      final same =
          owner ==
          (widget.status.json['board_id'], widget.status.json['boot_id']);
      if (!same) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text(
              'The connected board changed. Open the box actions again.',
            ),
          ),
        );
      }
      return same;
    }

    await showModalBottomSheet<void>(
      context: context,
      showDragHandle: true,
      isScrollControlled: true,
      builder: (context) => SafeArea(
        child: SingleChildScrollView(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              ListTile(
                leading: const Icon(Icons.sports_esports_outlined),
                title: Text('Box $key'),
                subtitle: Text(
                  widget.blockedReason ??
                      (enabled ? 'Button · Indicator' : 'Disabled'),
                ),
              ),
              ListTile(
                leading: const Icon(Icons.touch_app_outlined),
                title: const Text('Trigger box'),
                enabled: widget.enabled && enabled && !arranging,
                onTap: () {
                  Navigator.pop(context);
                  if (sameOwner() &&
                      widget.enabled &&
                      active(key) &&
                      !arranging) {
                    widget.onTap(key);
                  }
                },
              ),
              if (widget.onReset != null)
                ListTile(
                  leading: const Icon(Icons.restart_alt),
                  title: const Text('Reset to white'),
                  enabled: widget.enabled && !arranging,
                  onTap: () {
                    Navigator.pop(context);
                    if (sameOwner()) widget.onReset!();
                  },
                ),
              ListTile(
                leading: const Icon(Icons.check_circle_outline),
                title: Text(
                  widget.workspace.selectedKeys.contains(key)
                      ? 'Deselect box'
                      : 'Select box',
                ),
                onTap: () {
                  Navigator.pop(context);
                  if (!sameOwner()) return;
                  setState(() {
                    arranging = true;
                    multiple = true;
                  });
                  select(key);
                },
              ),
              ListTile(
                leading: const Icon(Icons.tune),
                title: const Text('Configure boxes'),
                onTap: () {
                  Navigator.pop(context);
                  if (sameOwner()) widget.openSettings();
                },
              ),
            ],
          ),
        ),
      ),
    );
  }

  Color? fallback(int key) {
    final mini = widget.status.json['minigame'];
    if (mini is Map && mini['mode'] != 'press-to-win') return null;
    final raw = widget.status.json['key_game'];
    final field = winner == 0
        ? 'idle_rgb'
        : winner == key
        ? 'win_rgb'
        : 'lose_rgb';
    final rgb = raw is Map ? raw[field] : null;
    return rgb is int ? Color(0xff000000 | rgb) : null;
  }

  void move(int key, Offset delta, Size travel) {
    if (dragging == null) {
      dragging = key;
      if (!widget.workspace.selectedKeys.contains(key)) {
        widget.workspace.selectKeys({key});
      }
      dragOrigins
        ..clear()
        ..addEntries(
          widget.workspace.selectedKeys.map(
            (id) => MapEntry(id, widget.workspace.keyPosition(id)),
          ),
        );
      dragDelta = Offset.zero;
    }
    dragDelta += delta;
    // Clamp the selection as a group; relative spacing survives edge dragging.
    var dx = travel.width == 0 ? 0.0 : dragDelta.dx / travel.width;
    var dy = travel.height == 0 ? 0.0 : dragDelta.dy / travel.height;
    for (final start in dragOrigins.values) {
      dx = dx.clamp(-start.dx, 1 - start.dx);
      dy = dy.clamp(-start.dy, 1 - start.dy);
    }
    for (final entry in dragOrigins.entries) {
      widget.workspace.moveKey(entry.key, entry.value + Offset(dx, dy));
    }
    setState(() {});
  }

  void finishDrag(Size travel) {
    if (dragging == null) return;
    // Snap once on release, keeping pointer movement continuous while dragging.
    final origin = widget.workspace.keyPosition(dragging!);
    final snap = Offset(
      travel.width == 0
          ? 0
          : (origin.dx * travel.width / 12).round() * 12 / travel.width,
      travel.height == 0
          ? 0
          : (origin.dy * travel.height / 12).round() * 12 / travel.height,
    );
    var delta = snap - origin;
    for (final key in dragOrigins.keys) {
      final at = widget.workspace.keyPosition(key);
      delta = Offset(
        delta.dx.clamp(-at.dx, 1 - at.dx),
        delta.dy.clamp(-at.dy, 1 - at.dy),
      );
    }
    for (final key in dragOrigins.keys) {
      widget.workspace.moveKey(key, widget.workspace.keyPosition(key) + delta);
    }
    setState(() {
      dragging = null;
      dragOrigins.clear();
    });
  }

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: widget.workspace,
    builder: (context, _) {
      final colors = Theme.of(context).colorScheme;
      final selected = widget.workspace.selectedKeys;
      return Container(
        decoration: BoxDecoration(
          color: colors.surfaceContainerLow,
          borderRadius: BorderRadius.circular(24),
          border: Border.all(
            color: colors.outlineVariant.withValues(alpha: .35),
          ),
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(18, 16, 14, 12),
              child: Wrap(
                alignment: WrapAlignment.spaceBetween,
                crossAxisAlignment: WrapCrossAlignment.center,
                spacing: 20,
                runSpacing: 12,
                children: [
                  Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Container(
                        padding: const EdgeInsets.all(9),
                        decoration: BoxDecoration(
                          color: colors.primary.withValues(alpha: .12),
                          borderRadius: BorderRadius.circular(11),
                        ),
                        child: Icon(
                          arranging
                              ? Icons.dashboard_customize_outlined
                              : Icons.sports_esports_outlined,
                          color: colors.primary,
                          size: 21,
                        ),
                      ),
                      const SizedBox(width: 10),
                      Flexible(
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            Text(
                              arranging ? 'Box designer' : 'Live boxes',
                              style: Theme.of(context).textTheme.titleMedium
                                  ?.copyWith(fontWeight: FontWeight.w700),
                            ),
                            Text(
                              '${List.generate(9, (i) => i + 1).where(active).length} enabled · ${arranging ? '${selected.length} selected' : 'Button + indicator'}',
                              style: Theme.of(context).textTheme.bodySmall
                                  ?.copyWith(color: colors.onSurfaceVariant),
                            ),
                          ],
                        ),
                      ),
                    ],
                  ),
                  Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Flexible(
                        child: FilterChip(
                          key: const ValueKey('key-designer-arrange'),
                          avatar: Icon(
                            arranging ? Icons.check : Icons.open_with,
                            size: 18,
                          ),
                          label: Text(
                            arranging ? 'Done arranging' : 'Arrange boxes',
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                          ),
                          selected: arranging,
                          onSelected: (value) => setState(() {
                            widget.onHoldEnd?.call();
                            arranging = value;
                            dragging = null;
                          }),
                        ),
                      ),
                      const SizedBox(width: 6),
                      PopupMenuButton<String>(
                        tooltip: 'Box workspace options',
                        icon: const Icon(Icons.more_horiz),
                        onSelected: (action) {
                          if (action == 'settings') widget.openSettings();
                          if (action == 'reset') {
                            widget.workspace.resetKeyPositions();
                          }
                          if (action == 'all') {
                            widget.workspace.selectKeys({
                              for (var k = 1; k <= 9; k++) k,
                            });
                          }
                          if (action == 'none') widget.workspace.selectKeys({});
                        },
                        itemBuilder: (_) => [
                          workspaceMenuItem(
                            'settings',
                            'Configure boxes',
                            Icons.tune,
                          ),
                          if (arranging) ...[
                            workspaceMenuItem(
                              'all',
                              'Select all boxes',
                              Icons.select_all,
                            ),
                            workspaceMenuItem(
                              'none',
                              'Clear selection',
                              Icons.deselect,
                            ),
                            workspaceMenuItem(
                              'reset',
                              'Reset positions',
                              Icons.grid_view_outlined,
                            ),
                          ],
                        ],
                      ),
                    ],
                  ),
                ],
              ),
            ),
            if (arranging)
              Padding(
                padding: const EdgeInsets.fromLTRB(18, 0, 18, 12),
                child: Wrap(
                  spacing: 10,
                  crossAxisAlignment: WrapCrossAlignment.center,
                  children: [
                    FilterChip(
                      label: const Text('Multiple selection'),
                      selected: multiple,
                      avatar: const Icon(
                        Icons.library_add_check_outlined,
                        size: 17,
                      ),
                      onSelected: (value) => setState(() => multiple = value),
                    ),
                    Text(
                      'Drag to arrange · Snap to grid',
                      style: Theme.of(context).textTheme.bodySmall
                          ?.copyWith(color: colors.onSurfaceVariant),
                    ),
                  ],
                ),
              ),
            LayoutBuilder(
              builder: (context, bounds) {
                final width = math.min(bounds.maxWidth, 900.0);
                final compact = width < 480;
                final cardWidth = math.min(220.0, (width - 48) / 3);
                final cardHeight = compact ? 142.0 : 176.0;
                final canvasHeight = cardHeight * 3 + (compact ? 40 : 56);
                final travel = Size(
                  width - cardWidth - 32,
                  canvasHeight - cardHeight - 32,
                );
                return Align(
                  child: SizedBox(
                    width: width,
                    height: canvasHeight,
                    child: MouseRegion(
                      onHover: (event) =>
                          setState(() => pointer = event.localPosition),
                      onExit: (_) => setState(() => pointer = null),
                      child: ClipRRect(
                        borderRadius: const BorderRadius.vertical(
                          bottom: Radius.circular(24),
                        ),
                        child: Stack(
                          key: const ValueKey('key-layout-canvas'),
                          children: [
                            Positioned.fill(
                              child: CustomPaint(
                                painter: _BoxGrid(
                                  colors.primary,
                                  colors.outlineVariant,
                                  arranging,
                                  pointer,
                                  dragging != null,
                                ),
                              ),
                            ),
                            for (var key = 1; key <= 9; key++)
                              Positioned(
                                key: ValueKey('game-key-position-$key'),
                                left:
                                    16 +
                                    widget.workspace.keyPosition(key).dx *
                                        travel.width,
                                top:
                                    16 +
                                    widget.workspace.keyPosition(key).dy *
                                        travel.height,
                                width: cardWidth,
                                height: cardHeight,
                                child: Semantics(
                                  button: true,
                                  selected: selected.contains(key),
                                  label: 'Key $key',
                                  enabled:
                                      arranging ||
                                      (widget.enabled && active(key)),
                                  child: GestureDetector(
                                    key: ValueKey(
                                      '${widget.inputKeyPrefix}-$key',
                                    ),
                                    behavior: HitTestBehavior.opaque,
                                    onTap: arranging
                                        ? () => select(key)
                                        : widget.enabled && active(key)
                                        ? () => widget.onTap(key)
                                        : widget.onBlockedTap == null
                                        ? null
                                        : () => widget.onBlockedTap!(key),
                                    onLongPress:
                                        !arranging && widget.onHoldStart == null
                                        ? () => menu(key)
                                        : null,
                                    onLongPressStart:
                                        !arranging &&
                                            widget.enabled &&
                                            active(key) &&
                                            widget.onHoldStart != null
                                        ? (_) => widget.onHoldStart!(key)
                                        : null,
                                    onLongPressEnd: (_) =>
                                        widget.onHoldEnd?.call(),
                                    onLongPressCancel: widget.onHoldEnd,
                                    onPanUpdate: arranging
                                        ? (details) =>
                                              move(key, details.delta, travel)
                                        : null,
                                    onPanEnd: arranging
                                        ? (_) => finishDrag(travel)
                                        : null,
                                    onPanCancel: arranging
                                        ? () => finishDrag(travel)
                                        : null,
                                    child: _BoxCard(
                                      key: ValueKey('game-key-$key'),
                                      number: key,
                                      active: active(key),
                                      selected:
                                          arranging && selected.contains(key),
                                      pressed: pressed(key),
                                      winner: winner == key,
                                      latched:
                                          number('latched_mask') &
                                              (1 << (key - 1)) !=
                                          0,
                                      lost: winner != 0 && winner != key,
                                      dragging:
                                          dragging != null &&
                                          selected.contains(key),
                                      arranging: arranging,
                                      illumination: keyPixelGroup(
                                        widget.status,
                                        key,
                                        0,
                                      ),
                                      indicator: keyPixelGroup(
                                        widget.status,
                                        key,
                                        1,
                                      ),
                                      fallback: fallback(key),
                                      compact: compact,
                                      onMenu: () => menu(key),
                                    ),
                                  ),
                                ),
                              ),
                          ],
                        ),
                      ),
                    ),
                  ),
                );
              },
            ),
          ],
        ),
      );
    },
  );
}

class _BoxCard extends StatefulWidget {
  const _BoxCard({
    super.key,
    required this.number,
    required this.active,
    required this.selected,
    required this.pressed,
    required this.winner,
    required this.latched,
    required this.lost,
    required this.dragging,
    required this.arranging,
    required this.compact,
    required this.onMenu,
    this.illumination,
    this.indicator,
    this.fallback,
  });
  final int number;
  final bool active,
      selected,
      pressed,
      winner,
      latched,
      lost,
      dragging,
      arranging,
      compact;
  final VoidCallback onMenu;
  final List<int>? illumination, indicator;
  final Color? fallback;
  @override
  State<_BoxCard> createState() => _BoxCardState();
}

class _BoxCardState extends State<_BoxCard> {
  bool hover = false;
  @override
  Widget build(BuildContext context) {
    final c = Theme.of(context).colorScheme;
    final accent = widget.selected || widget.pressed
        ? c.primary
        : widget.winner || widget.latched
        ? const Color(0xff38d996)
        : widget.lost
        ? c.error
        : c.onSurfaceVariant;
    final bright =
        widget.active &&
        (widget.selected || widget.pressed || widget.winner || widget.latched);
    return MouseRegion(
      cursor: widget.arranging
          ? (widget.dragging
                ? SystemMouseCursors.grabbing
                : SystemMouseCursors.grab)
          : widget.active
          ? SystemMouseCursors.click
          : SystemMouseCursors.basic,
      onEnter: (_) => setState(() => hover = true),
      onExit: (_) => setState(() => hover = false),
      child: AnimatedContainer(
        duration: MediaQuery.disableAnimationsOf(context)
            ? Duration.zero
            : const Duration(milliseconds: 140),
        padding: EdgeInsets.all(widget.compact ? 8 : 12),
        decoration: BoxDecoration(
          gradient: LinearGradient(
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
            colors: [
              Color.alphaBlend(
                accent.withValues(
                  alpha: bright
                      ? .16
                      : hover
                      ? .08
                      : .025,
                ),
                c.surfaceContainerHigh,
              ),
              c.surfaceContainerLow,
            ],
          ),
          borderRadius: BorderRadius.circular(20),
          border: Border.all(
            width: 1.5,
            color: bright
                ? accent.withValues(alpha: .8)
                : hover
                ? c.primary.withValues(alpha: .5)
                : c.outlineVariant.withValues(alpha: .35),
          ),
          boxShadow: [
            BoxShadow(
              color: Colors.black.withValues(
                alpha: widget.dragging ? .24 : .08,
              ),
              blurRadius: widget.dragging ? 22 : 10,
              offset: const Offset(0, 5),
            ),
          ],
        ),
        child: Opacity(
          opacity: widget.active ? 1 : .36,
          child: Column(
            children: [
              Row(
                children: [
                  Text(
                    widget.compact
                        ? '${widget.number}'.padLeft(2, '0')
                        : 'BOX ${'${widget.number}'.padLeft(2, '0')}',
                    style: Theme.of(context).textTheme.labelLarge?.copyWith(
                      letterSpacing: .8,
                      fontWeight: FontWeight.w800,
                    ),
                  ),
                  const Spacer(),
                  SizedBox(
                    width: 28,
                    height: 28,
                    child: IconButton(
                      padding: EdgeInsets.zero,
                      tooltip: 'Box ${widget.number} actions',
                      onPressed: widget.onMenu,
                      icon: Icon(
                        widget.arranging
                            ? Icons.drag_indicator
                            : Icons.more_horiz,
                        size: 19,
                      ),
                    ),
                  ),
                ],
              ),
              const Spacer(),
              Row(
                children: [
                  Expanded(
                    child: _LedDial(
                      label: 'Button',
                      colors: widget.illumination,
                      fallback: widget.fallback,
                      button: true,
                    ),
                  ),
                  const SizedBox(width: 6),
                  Expanded(
                    child: _LedDial(
                      label: 'Indicator',
                      colors: widget.indicator,
                      fallback: widget.fallback,
                      button: false,
                    ),
                  ),
                ],
              ),
              const Spacer(),
              Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  Icon(
                    !widget.active
                        ? Icons.do_not_disturb_on_outlined
                        : widget.selected
                        ? Icons.check_circle
                        : widget.winner
                        ? Icons.emoji_events_outlined
                        : widget.pressed
                        ? Icons.touch_app
                        : widget.latched
                        ? Icons.toggle_on_outlined
                        : widget.lost
                        ? Icons.remove_circle_outline
                        : Icons.circle,
                    size:
                        widget.active &&
                            !widget.selected &&
                            !widget.winner &&
                            !widget.latched &&
                            !widget.lost &&
                            !widget.pressed
                        ? 6
                        : 13,
                    color: accent,
                  ),
                  const SizedBox(width: 5),
                  Flexible(
                    child: Text(
                      !widget.active
                          ? 'Disabled'
                          : widget.arranging
                          ? widget.selected
                                ? 'Selected'
                                : 'Move'
                          : widget.winner
                          ? 'Winner'
                          : widget.pressed
                          ? 'Held'
                          : widget.latched
                          ? 'On'
                          : widget.lost
                          ? 'Lost'
                          : 'Ready',
                      maxLines: 1,
                      overflow: TextOverflow.ellipsis,
                      style: Theme.of(context).textTheme.labelSmall?.copyWith(
                        color: accent,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }
}

class _LedDial extends StatelessWidget {
  const _LedDial({
    required this.label,
    required this.colors,
    required this.fallback,
    required this.button,
  });
  final String label;
  final List<int>? colors;
  final Color? fallback;
  final bool button;
  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    return LayoutBuilder(
      builder: (context, bounds) {
        final diameter = math.min(64.0, bounds.maxWidth);
        return Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            SizedBox.square(
              dimension: diameter,
              child: CustomPaint(
                painter: _LedRing(colors, fallback, scheme, button),
              ),
            ),
            const SizedBox(height: 5),
            Text(
              label,
              maxLines: 1,
              overflow: TextOverflow.ellipsis,
              style: Theme.of(context).textTheme.labelSmall
                  ?.copyWith(fontSize: 10, color: scheme.onSurfaceVariant),
            ),
          ],
        );
      },
    );
  }
}

class _LedRing extends CustomPainter {
  _LedRing(this.pixels, this.fallback, this.scheme, this.button);
  final List<int>? pixels;
  final Color? fallback;
  final ColorScheme scheme;
  final bool button;
  @override
  void paint(Canvas canvas, Size size) {
    final center = size.center(Offset.zero), radius = size.shortestSide * .43;
    final rect = Rect.fromCircle(center: center, radius: radius);
    final count = pixels?.length ?? 12;
    canvas.drawCircle(
      center,
      radius - 7,
      Paint()..color = scheme.surfaceContainerLowest,
    );
    for (var index = 0; index < count; index++) {
      final rgb = pixels?[index];
      final color = rgb == null
          ? fallback ?? scheme.outlineVariant
          : Color(0xff000000 | rgb);
      final on = rgb == null ? fallback != null : rgb != 0;
      final paint = Paint()
        ..style = PaintingStyle.stroke
        ..strokeWidth = 4
        ..strokeCap = StrokeCap.round
        ..color = on ? color : scheme.outlineVariant.withValues(alpha: .3);
      final start = -math.pi / 2 + index * math.pi * 2 / count;
      canvas.drawArc(
        rect,
        start + .055,
        math.max(.02, math.pi * 2 / count - .11),
        false,
        paint,
      );
    }
    if (button) {
      final shade = fallback ?? scheme.primary;
      canvas.drawCircle(
        center,
        radius - 9,
        Paint()
          ..shader = RadialGradient(
            colors: [
              shade.withValues(alpha: .25),
              shade.withValues(alpha: .06),
            ],
          ).createShader(rect),
      );
      canvas.drawCircle(
        center,
        radius - 9,
        Paint()
          ..style = PaintingStyle.stroke
          ..strokeWidth = 1
          ..color = scheme.outlineVariant.withValues(alpha: .5),
      );
    } else {
      canvas.drawCircle(
        center,
        3,
        Paint()..color = scheme.onSurfaceVariant.withValues(alpha: .6),
      );
    }
  }

  @override
  bool shouldRepaint(covariant _LedRing old) =>
      old.pixels != pixels ||
      old.fallback != fallback ||
      old.scheme != scheme ||
      old.button != button;
}

class _BoxGrid extends CustomPainter {
  _BoxGrid(
    this.accent,
    this.neutral,
    this.editing,
    this.pointer,
    this.dragging,
  );
  final Color accent, neutral;
  final bool editing, dragging;
  final Offset? pointer;
  @override
  void paint(Canvas canvas, Size size) {
    for (double y = 16; y < size.height; y += 18) {
      for (double x = 16; x < size.width; x += 18) {
        final at = Offset(x, y);
        final near = pointer == null
            ? 0.0
            : (1 - (at - pointer!).distance / 140).clamp(0.0, 1.0);
        canvas.drawCircle(
          at,
          .8 + near * .6,
          Paint()
            ..color = Color.lerp(
              neutral.withValues(alpha: editing ? .24 : .10),
              accent.withValues(alpha: dragging ? .65 : .4),
              near,
            )!,
        );
      }
    }
  }

  @override
  bool shouldRepaint(covariant _BoxGrid old) =>
      old.accent != accent ||
      old.neutral != neutral ||
      old.editing != editing ||
      old.pointer != pointer ||
      old.dragging != dragging;
}
