import 'dart:math' as math;

import 'package:flutter/material.dart';

import 'minigame.dart';

/// Board-advertised choices only; opening/focusing never changes game state.
/// Reuses the PCController ActivityStrip's bounded text and semantic icon surface.
class GameModePicker extends StatelessWidget {
  const GameModePicker({
    super.key,
    required this.game,
    required this.onChanged,
  });

  final MiniGameSnapshot game;
  final ValueChanged<String>? onChanged;

  String description(String id) {
    final supplied = game.description(id).trim();
    return supplied.isNotEmpty
        ? supplied
        : minigameDescriptions[id] ?? 'Board-provided game';
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context), scaler = MediaQuery.textScalerOf(context);
    final bold = MediaQuery.boldTextOf(context);
    final titleStyle = theme.textTheme.bodyMedium!.copyWith(
      fontWeight: bold ? FontWeight.bold : FontWeight.w600,
      height: 1.25,
    );
    final detailStyle = theme.textTheme.bodySmall!.copyWith(
      color: theme.colorScheme.onSurfaceVariant,
      height: 1.35,
      fontWeight: bold ? FontWeight.bold : null,
    );
    final numberStyle = theme.textTheme.labelMedium!.copyWith(
      color: theme.colorScheme.onSurfaceVariant,
      fontWeight: bold ? FontWeight.bold : null,
    );
    TextPainter measure(String text, TextStyle style) => TextPainter(
      text: TextSpan(text: text, style: style),
      textDirection: TextDirection.ltr,
      textScaler: scaler,
      maxLines: 1,
    )..layout();
    var numberWidth = 26.0;
    for (var i = 1; i <= game.modes.length; i++) {
      final number = measure('$i.', numberStyle);
      numberWidth = math.max(numberWidth, number.width.ceilToDouble() + 2);
      number.dispose();
    }
    final title = measure('Ag', titleStyle),
        detail = measure('Ag', detailStyle);
    // Reserve both title/description lines for every choice. Selecting a longer
    // name, hover or keyboard focus must not move the surrounding controls.
    final rowHeight = math
        .max(
          84.0,
          math.max(36.0, title.height * 2 + 3 + detail.height * 2) + 16,
        )
        .ceilToDouble();
    title.dispose();
    detail.dispose();
    return SizedBox(
      width: 380,
      child: InputDecorator(
        decoration: InputDecoration(
          labelText: 'Game mode',
          enabled: onChanged != null,
          contentPadding: const EdgeInsets.symmetric(
            horizontal: 14,
            vertical: 4,
          ),
        ),
        child: DropdownButtonHideUnderline(
          child: DropdownButton<String>(
            key: const ValueKey('minigame-mode'),
            value: game.mode,
            isExpanded: true,
            itemHeight: rowHeight,
            isDense: false,
            borderRadius: BorderRadius.circular(16),
            items: [
              for (final (index, entry) in game.modes.entries.indexed)
                DropdownMenuItem(
                  value: entry.key,
                  child: _GameModeChoice(
                    number: index + 1,
                    numberWidth: numberWidth,
                    id: entry.key,
                    label: entry.value,
                    description: description(entry.key),
                    titleStyle: titleStyle,
                    detailStyle: detailStyle,
                    numberStyle: numberStyle,
                  ),
                ),
            ],
            onChanged: onChanged == null
                ? null
                : (value) {
                    if (value != null && value != game.mode) onChanged!(value);
                  },
          ),
        ),
      ),
    );
  }
}

class _GameModeChoice extends StatelessWidget {
  const _GameModeChoice({
    required this.number,
    required this.numberWidth,
    required this.id,
    required this.label,
    required this.description,
    required this.titleStyle,
    required this.detailStyle,
    required this.numberStyle,
  });
  final int number;
  final double numberWidth;
  final String id, label, description;
  final TextStyle titleStyle, detailStyle, numberStyle;

  @override
  Widget build(BuildContext context) => Tooltip(
    message: '$number. $label\n$description',
    excludeFromSemantics: true,
    child: Semantics(
      label: '$number. $label. $description',
      excludeSemantics: true,
      child: Row(
        key: ValueKey('game-mode-choice-$id'),
        children: [
          SizedBox(
            width: numberWidth,
            child: Text(
              '$number.',
              maxLines: 1,
              softWrap: false,
              textDirection: TextDirection.ltr,
              style: numberStyle,
            ),
          ),
          Container(
            width: 36,
            height: 36,
            alignment: Alignment.center,
            decoration: BoxDecoration(
              color: Theme.of(context).colorScheme.primary
                  .withValues(alpha: .10),
              borderRadius: BorderRadius.circular(10),
            ),
            child: Icon(
              switch (id) {
                'press-to-win' => Icons.emoji_events_outlined,
                'illuminated' => Icons.my_location,
                'color-match' => Icons.palette_outlined,
                'simon' => Icons.queue_music,
                'memory-pairs' => Icons.style_outlined,
                'clear-all' => Icons.checklist_rounded,
                'color-order' => Icons.sort_rounded,
                'switch-pattern' => Icons.grid_view_rounded,
                'reaction-chase' => Icons.bolt_rounded,
                'mine-field' => Icons.diamond_outlined,
                'toggle' => Icons.toggle_on_outlined,
                _ => Icons.sports_esports_outlined,
              },
              size: 22,
              color: Theme.of(context).colorScheme.primary,
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  label,
                  maxLines: 2,
                  overflow: TextOverflow.ellipsis,
                  style: titleStyle,
                ),
                const SizedBox(height: 3),
                Text(
                  description,
                  maxLines: 2,
                  overflow: TextOverflow.ellipsis,
                  style: detailStyle,
                ),
              ],
            ),
          ),
        ],
      ),
    ),
  );
}
