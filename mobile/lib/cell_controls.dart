import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';

import 'icons.dart';

Color cellActionColor(String value, Map<String, dynamic> status) {
  final state = switch (value) {
    'white' => 'idle',
    'red' => 'lost',
    _ => value,
  };
  final presentation = status['state_presentation'];
  final states = presentation is Map ? presentation['states'] : null;
  final entry = states is Map ? states[state] : null;
  final rgb = entry is Map ? entry['rgb'] : null;
  if (rgb is int && rgb >= 0 && rgb <= 0xffffff) return Color(0xff000000 | rgb);
  return switch (state) {
    'idle' => const Color(0xffffffff),
    'lost' => const Color(0xffff0000),
    'won' => const Color(0xff00ff00),
    _ => const Color(0xff20242a),
  };
}

String cellActionLabel(String value, Color color, {required bool colors}) {
  if (value == 'off') return 'Off';
  if (!colors) {
    return switch (value) {
      'white' || 'idle' => 'Idle',
      'red' || 'lost' => 'Lost',
      'won' => 'Won',
      _ => value,
    };
  }
  return switch (color.toARGB32() & 0xffffff) {
    0xffffff => 'White',
    0xff0000 => 'Red',
    0x00ff00 => 'Green',
    0x0000ff => 'Blue',
    0xffff00 => 'Yellow',
    0x00ffff => 'Cyan',
    0xff00ff => 'Magenta',
    0 => 'Black',
    final rgb => '#${rgb.toRadixString(16).padLeft(6, '0').toUpperCase()}',
  };
}

/// Adapted from PCController's unified/relay_style.dart: semantic action tones,
/// contrast-selected ink, state-resolved fills and no outline on filled actions.
/// Pakar's palette is board-defined, so it is not tied to an on/off relay or the
/// app's accent. The subtle surface blend softens bright RGB values without
/// changing the color or state sent to the board.
ButtonStyle cellColorActionStyle(
  ColorScheme colors,
  Color semantic, {
  TextStyle? labelStyle,
  bool alreadyApplied = false,
}) {
  final solidFill = Color.lerp(semantic, colors.surfaceContainerHighest, .12)!;
  // Reuse the donor's tonal variant for an already satisfied action. Keep the
  // geometry and command unchanged: explicit taps may still replay an effect.
  final fill = alreadyApplied ? semantic.withValues(alpha: .08) : solidFill;
  final luminance = solidFill.computeLuminance();
  final solidInk = (luminance + .05) / .05 >= 1.05 / (luminance + .05)
      ? Colors.black
      : Colors.white;
  var ink = solidInk;
  if (alreadyApplied) {
    final surface = Color.alphaBlend(fill, colors.surface);
    ink = colors.onSurface;
    for (var step = 0; step <= 8; step++) {
      final candidate = Color.lerp(semantic, colors.onSurface, step / 8)!;
      final a = surface.computeLuminance();
      final b = candidate.computeLuminance();
      if ((a > b ? (a + .05) / (b + .05) : (b + .05) / (a + .05)) >= 4.5) {
        ink = candidate;
        break;
      }
    }
  }
  return ButtonStyle(
    minimumSize: const WidgetStatePropertyAll(Size(48, 36)),
    padding: const WidgetStatePropertyAll(
      EdgeInsets.symmetric(horizontal: 12, vertical: 6),
    ),
    foregroundColor: WidgetStateProperty.resolveWith(
      (states) => states.contains(WidgetState.disabled)
          ? colors.onSurface.withValues(alpha: .38)
          : ink,
    ),
    backgroundColor: WidgetStateProperty.resolveWith(
      (states) => states.contains(WidgetState.disabled)
          ? Color.alphaBlend(
              semantic.withValues(alpha: .06),
              colors.surfaceContainerLow,
            )
          : fill,
    ),
    overlayColor: WidgetStateProperty.resolveWith((states) {
      if (states.contains(WidgetState.disabled)) return Colors.transparent;
      if (states.contains(WidgetState.pressed)) {
        return ink.withValues(alpha: .12);
      }
      if (states.contains(WidgetState.focused)) {
        return ink.withValues(alpha: .10);
      }
      if (states.contains(WidgetState.hovered)) {
        return ink.withValues(alpha: .06);
      }
      return Colors.transparent;
    }),
    // Reserve one pixel in every state: keyboard focus must not resize controls.
    side: WidgetStateProperty.resolveWith(
      (states) => BorderSide(
        color:
            !states.contains(WidgetState.disabled) &&
                states.contains(WidgetState.focused)
            ? ink.withValues(alpha: .35)
            : Colors.transparent,
      ),
    ),
    elevation: const WidgetStatePropertyAll(0),
    shadowColor: const WidgetStatePropertyAll(Colors.transparent),
    surfaceTintColor: const WidgetStatePropertyAll(Colors.transparent),
    textStyle: WidgetStatePropertyAll(
      (labelStyle ?? const TextStyle()).copyWith(
        fontSize: 13,
        height: 1.2,
        fontWeight: FontWeight.w600,
      ),
    ),
    shape: WidgetStatePropertyAll(
      RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
    ),
    animationDuration: const Duration(milliseconds: 140),
    visualDensity: VisualDensity.standard,
    tapTargetSize: MaterialTapTargetSize.padded,
  );
}

// Compact painted controls retain padded touch targets and reserved borders.
class CellColorAction extends StatelessWidget {
  const CellColorAction({
    super.key,
    required this.value,
    this.onPressed,
    this.color,
    this.label,
    this.alreadyApplied = false,
  });
  final String value;
  final Color? color;
  final String? label;
  final bool alreadyApplied;
  final VoidCallback? onPressed;
  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    final semantic =
        color ??
        switch (value) {
          'red' || 'lost' => const Color(0xffd9364a),
          'green' || 'won' => const Color(0xff28b86b),
          'white' || 'idle' => const Color(0xffeeeeee),
          _ => colors.surfaceContainerHighest,
        };
    return Semantics(
      value: alreadyApplied ? 'Already applied to selection' : null,
      child: FilledButton.icon(
        onPressed: onPressed,
        style: cellColorActionStyle(
          colors,
          semantic,
          labelStyle: Theme.of(context).textTheme.labelLarge,
          alreadyApplied: alreadyApplied,
        ),
        icon: Icon(
          alreadyApplied
              ? Icons.check_rounded
              : value == 'off'
              ? Icons.power_settings_new
              : PakarIcons.circle,
          size: 16,
        ),
        label: Text(label ?? 'Set $value'),
      ),
    );
  }
}

class CellOption extends StatelessWidget {
  const CellOption({
    super.key,
    required this.label,
    required this.icon,
    required this.selected,
    required this.onSelected,
  });
  final String label;
  final IconData icon;
  final bool selected;
  final ValueChanged<bool> onSelected;
  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    return FilterChip(
      label: Text(label),
      avatar: Icon(
        icon,
        size: 16,
        color: selected ? colors.primary : colors.onSurfaceVariant,
      ),
      labelStyle: TextStyle(
        color: selected ? colors.primary : colors.onSurfaceVariant,
        fontSize: 13,
        height: 1.2,
        fontWeight: selected ? FontWeight.w600 : FontWeight.w500,
      ),
      checkmarkColor: colors.primary,
      selected: selected,
      onSelected: onSelected,
      showCheckmark: true,
      padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
      visualDensity: VisualDensity.standard,
      materialTapTargetSize: MaterialTapTargetSize.padded,
      selectedColor: Color.alphaBlend(
        colors.primary.withValues(alpha: .12),
        colors.surfaceContainerLow,
      ),
      backgroundColor: colors.surfaceContainerLow,
      surfaceTintColor: Colors.transparent,
      elevation: 0,
      pressElevation: 0,
      side: BorderSide(
        color: selected
            ? colors.primary.withValues(alpha: .20)
            : Colors.transparent,
      ),
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
    );
  }
}

/// Telemetry reconstructs byte arrays; MemoryImage identity must not do so.
class StableApplicationIcon extends StatefulWidget {
  const StableApplicationIcon({
    super.key,
    required this.bytes,
    required this.fallback,
  });
  final Uint8List bytes;
  final Uint8List fallback;
  @override
  State<StableApplicationIcon> createState() => _StableApplicationIconState();
}

class _StableApplicationIconState extends State<StableApplicationIcon> {
  late Uint8List bytes = widget.bytes;
  @override
  void didUpdateWidget(StableApplicationIcon oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (!listEquals(bytes, widget.bytes)) bytes = widget.bytes;
  }

  @override
  Widget build(BuildContext context) => Image.memory(
    bytes,
    gaplessPlayback: true,
    errorBuilder: (_, _, _) =>
        Image.memory(widget.fallback, gaplessPlayback: true),
  );
}
