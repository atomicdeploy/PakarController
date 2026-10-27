import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

/// Explicit menu intent for a zero-based OUTPUT slot, never a projected raw
/// source. Identity distinguishes repeat user actions with equal parameters.
class PixelCorrectionRuleIntent {
  const PixelCorrectionRuleIntent({
    required this.kind,
    required this.pixel,
    this.offset = 0,
    this.revision,
  });
  final String kind;
  final int pixel, offset;

  /// Revision of the saved raw-to-wire projection used to choose this slot.
  final int? revision;
  Map<String, dynamic> toRule() => {
    'kind': kind,
    'pixel': pixel,
    'offset': offset,
    'enabled': true,
  };
}

/// Local-only projection of BoardCore/PixelCorrections.h. A preview is not an
/// applied observation and never changes the raw scene or sends a command.
List<int> pixelCorrectionSources(
  int pixels,
  bool enabled,
  List<Map<String, dynamic>> rules,
) {
  if (pixels < 1 || pixels > 480 || rules.length > 32) {
    throw const FormatException('Invalid correction capacity');
  }
  for (final rule in rules) {
    final pixel = rule['pixel'], offset = rule['offset'] ?? 0;
    if (pixel is! int ||
        pixel < 0 ||
        pixel >= pixels ||
        offset is! int ||
        offset.abs() > pixels ||
        !['black', 'remove', 'shift'].contains(rule['kind']) ||
        (rule['kind'] != 'shift' && offset != 0) ||
        (rule['enabled'] != null && rule['enabled'] is! bool)) {
      throw const FormatException('Invalid pixel correction rule');
    }
  }
  final sources = List<int>.generate(pixels, (index) => index);
  if (!enabled) return sources;
  for (final rule in rules) {
    if (rule['enabled'] == false) continue;
    final pixel = rule['pixel'] as int;
    final offset = (rule['offset'] as int?) ?? 0;
    switch (rule['kind']) {
      case 'black':
        sources[pixel] = -1;
        break;
      case 'remove':
        for (var index = pixel; index + 1 < pixels; index++) {
          sources[index] = sources[index + 1];
        }
        sources[pixels - 1] = -1;
        break;
      case 'shift':
        if (offset > 0) {
          for (var index = pixels - 1; index >= pixel; index--) {
            sources[index] = index - offset >= pixel
                ? sources[index - offset]
                : -1;
          }
        } else if (offset < 0) {
          for (
            var index = (pixel + offset).clamp(0, pixels);
            index < pixels + offset;
            index++
          ) {
            sources[index] = sources[index - offset];
          }
          for (
            var index = (pixels + offset).clamp(pixel, pixels);
            index < pixels;
            index++
          ) {
            sources[index] = -1;
          }
        }
        break;
    }
  }
  return sources;
}

String correctionOperationTitle(String kind) => switch (kind) {
  'black' => 'Black out pixel',
  'remove' => 'Remove address',
  _ => 'Shift from pixel',
};

String correctionOperationDescription(String kind) => switch (kind) {
  'black' => 'Send black to this address. Later pixels keep their positions.',
  'remove' => 'Skip this value. Following values move one address earlier; the last address is black.',
  _ => 'Move this pixel and all following values. Positive moves later; negative moves earlier. Empty addresses are black.',
};

IconData correctionOperationIcon(String kind) => switch (kind) {
  'black' => Icons.lightbulb_outline,
  'remove' => Icons.hide_source_outlined,
  _ => Icons.swap_horiz_rounded,
};

/// Adapts Pealayer NumericValueControl's bounded +/- and disabled endpoints,
/// and PCController PwmSetpointControl's owned draft/authoritative value split.
/// No writes happen here; invalid drafts stay visible and block form submission.
class PixelCorrectionNumberControl extends StatefulWidget {
  const PixelCorrectionNumberControl({
    super.key,
    required this.label,
    required this.value,
    required this.minimum,
    required this.maximum,
    required this.onChanged,
    required this.onValidityChanged,
    this.enabled = true,
    this.signed = false,
  });
  final String label;
  final int value, minimum, maximum;
  final ValueChanged<int> onChanged;
  final ValueChanged<bool> onValidityChanged;
  final bool enabled, signed;
  @override
  State<PixelCorrectionNumberControl> createState() =>
      _PixelCorrectionNumberControlState();
}

class _PixelCorrectionNumberControlState
    extends State<PixelCorrectionNumberControl> {
  late final input = TextEditingController(text: '${widget.value}');
  final field = GlobalKey<FormFieldState<String>>();
  @override
  void didUpdateWidget(PixelCorrectionNumberControl oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (widget.value != oldWidget.value &&
        int.tryParse(input.text) != widget.value) {
      input.text = '${widget.value}';
    }
  }

  @override
  void dispose() {
    input.dispose();
    super.dispose();
  }

  bool valid(int? value) =>
      value != null && value >= widget.minimum && value <= widget.maximum;
  void changed(String text) {
    final number = int.tryParse(text);
    widget.onValidityChanged(valid(number));
    if (valid(number)) widget.onChanged(number!);
    setState(() {});
  }

  void step(int amount) {
    final number = ((int.tryParse(input.text) ?? widget.value) + amount).clamp(
      widget.minimum,
      widget.maximum,
    );
    input.text = '$number';
    widget.onValidityChanged(true);
    widget.onChanged(number);
    field.currentState?.validate();
    setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    final number = int.tryParse(input.text) ?? widget.value;
    final colors = Theme.of(context).colorScheme;
    final iconStyle = IconButton.styleFrom(
      foregroundColor: colors.onSurfaceVariant,
      backgroundColor: colors.surfaceContainerHigh,
      shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
      minimumSize: const Size(40, 40),
    );
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(bottom: 6),
          child: Text(
            widget.label,
            style: Theme.of(context).textTheme.labelMedium,
          ),
        ),
        Row(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            IconButton(
              tooltip: 'Decrease ${widget.label.toLowerCase()}',
              style: iconStyle,
              onPressed: widget.enabled && number > widget.minimum
                  ? () => step(-1)
                  : null,
              icon: const Icon(Icons.remove_rounded, size: 18),
            ),
            const SizedBox(width: 6),
            Expanded(
              child: TextFormField(
                key: field,
                controller: input,
                enabled: widget.enabled,
                textAlign: TextAlign.center,
                keyboardType: TextInputType.numberWithOptions(
                  signed: widget.signed,
                ),
                inputFormatters: [
                  TextInputFormatter.withFunction((oldValue, next) {
                    var text = next.text.replaceAll('−', '-');
                    for (var digit = 0; digit < 10; digit++) {
                      text = text
                          .replaceAll('۰۱۲۳۴۵۶۷۸۹'[digit], '$digit')
                          .replaceAll('٠١٢٣٤٥٦٧٨٩'[digit], '$digit');
                    }
                    return RegExp(widget.signed ? r'^-?[0-9]*$' : r'^[0-9]*$')
                            .hasMatch(text)
                        ? next.copyWith(text: text)
                        : oldValue;
                  }),
                ],
                decoration: InputDecoration(
                  isDense: true,
                  contentPadding: const EdgeInsets.symmetric(
                    horizontal: 8,
                    vertical: 13,
                  ),
                  semanticCounterText: widget.label,
                ),
                validator: (text) => valid(int.tryParse(text ?? ''))
                    ? null
                    : widget.signed
                    ? '−${widget.maximum}…${widget.maximum}'
                    : '${widget.minimum}–${widget.maximum}',
                onChanged: changed,
              ),
            ),
            const SizedBox(width: 6),
            IconButton(
              tooltip: 'Increase ${widget.label.toLowerCase()}',
              style: iconStyle,
              onPressed: widget.enabled && number < widget.maximum
                  ? () => step(1)
                  : null,
              icon: const Icon(Icons.add_rounded, size: 18),
            ),
          ],
        ),
      ],
    );
  }
}

/// Wire positions are never renamed as raw/source positions. The small crossed
/// tile is explicitly black/missing, not an invented RGB observation.
class PixelCorrectionDiagram extends StatelessWidget {
  const PixelCorrectionDiagram({
    super.key,
    required this.sources,
    this.start = 0,
    this.focus,
    this.rawRgb,
    this.compact = false,
  });
  final List<int> sources;
  final int start;
  final int? focus;
  final List<int>? rawRgb;
  final bool compact;
  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    return Semantics(
      label: sources
          .asMap()
          .entries
          .map(
            (entry) =>
                'Wire ${start + entry.key + 1}: ${entry.value < 0 ? 'black' : 'source ${entry.value + 1}'}',
          )
          .join(', '),
      child: ExcludeSemantics(
        child: Wrap(
          spacing: 6,
          runSpacing: 8,
          children: [
            for (var index = 0; index < sources.length; index++)
              Builder(
                builder: (context) {
                  final source = sources[index];
                  final rgb =
                      source >= 0 && rawRgb != null && source < rawRgb!.length
                      ? rawRgb![source]
                      : null;
                  final color = source < 0
                      ? Colors.black
                      : rgb != null
                      ? Color(0xff000000 | rgb)
                      : colors.surfaceContainerHighest;
                  final selected = focus == start + index;
                  final diameter = compact ? 28.0 : 38.0;
                  return SizedBox(
                    width: diameter + 8,
                    child: Column(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Text(
                          '${start + index + 1}',
                          maxLines: 1,
                          style: Theme.of(context).textTheme.labelSmall
                              ?.copyWith(
                                color: selected
                                    ? colors.primary
                                    : colors.onSurfaceVariant,
                              ),
                        ),
                        const SizedBox(height: 3),
                        Container(
                          key: ValueKey('wire-pixel-${start + index}'),
                          width: diameter,
                          height: diameter,
                          alignment: Alignment.center,
                          decoration: BoxDecoration(
                            color: color,
                            shape: BoxShape.circle,
                            border: Border.all(
                              color: selected
                                  ? colors.primary
                                  : colors.outlineVariant,
                            ),
                          ),
                          child: source < 0
                              ? Icon(
                                  Icons.close_rounded,
                                  size: compact ? 14 : 18,
                                  color: Colors.white70,
                                )
                              : Padding(
                                  padding: const EdgeInsets.all(3),
                                  child: FittedBox(
                                    fit: BoxFit.scaleDown,
                                    child: Text(
                                      '${source + 1}',
                                      style: Theme.of(context)
                                          .textTheme
                                          .labelSmall
                                          ?.copyWith(
                                            color: rgb == null
                                                ? colors.onSurfaceVariant
                                                : color.computeLuminance() > .35
                                                ? Colors.black
                                                : Colors.white,
                                            fontWeight: FontWeight.w600,
                                          ),
                                    ),
                                  ),
                                ),
                        ),
                      ],
                    ),
                  );
                },
              ),
          ],
        ),
      ),
    );
  }
}
