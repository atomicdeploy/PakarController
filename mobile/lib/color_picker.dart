import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

// Adapt Pealayer ColorPicker.tsx / ColorDropdown.tsx: one picker owns presets,
// custom editing and 12 deduplicated session recents. No preference/flash write.
const ledColorPalette = <String, int>{
  'White': 0xffffff,
  'Red': 0xff0000,
  'Green': 0x00ff00,
  'Blue': 0x0000ff,
  'Amber': 0xffbf00,
  'Orange': 0xff8000,
  'Yellow': 0xffff00,
  'Lime': 0x80ff00,
  'Mint': 0x00ff80,
  'Cyan': 0x00ffff,
  'Azure': 0x0080ff,
  'Violet': 0x8000ff,
  'Magenta': 0xff00ff,
  'Rose': 0xff0080,
  'Pink': 0xff80bf,
  'Off': 0x000000,
};
final _recentLedColors = <int>[];
String rgbHex(int rgb) => rgb.toRadixString(16).padLeft(6, '0').toUpperCase();
String rgbName(int rgb) =>
    ledColorPalette.entries
        .where((entry) => entry.value == rgb)
        .map((entry) => entry.key)
        .firstOrNull ??
    '#${rgbHex(rgb)}';

Future<int?> showLedColorPicker(
  BuildContext context, {
  required int initial,
  String title = 'Color',
}) async {
  final result = await showDialog<int>(
    context: context,
    builder: (_) => _ColorPicker(initial: initial, title: title),
  );
  if (result != null) {
    _recentLedColors.remove(result);
    _recentLedColors.insert(0, result);
    if (_recentLedColors.length > 12) _recentLedColors.removeLast();
  }
  return result;
}

class _ColorPicker extends StatefulWidget {
  const _ColorPicker({required this.initial, required this.title});
  final int initial;
  final String title;
  @override
  State<_ColorPicker> createState() => _ColorPickerState();
}

class _ColorPickerState extends State<_ColorPicker> {
  late HSVColor hsv = HSVColor.fromColor(Color(0xff000000 | widget.initial));
  late final hex = TextEditingController(text: rgbHex(widget.initial));
  bool valid = true;
  int get rgb => hsv.toColor().toARGB32() & 0xffffff;
  void setColor(int value) => setState(() {
    hsv = HSVColor.fromColor(Color(0xff000000 | value));
    hex.text = rgbHex(value);
    valid = true;
  });
  void change(HSVColor value) => setState(() {
    hsv = value;
    hex.text = rgbHex(rgb);
    valid = true;
  });
  @override
  void dispose() {
    hex.dispose();
    super.dispose();
  }

  Widget swatch(String name, int value) => Tooltip(
    message: '$name · #${rgbHex(value)}',
    child: Semantics(
      label: name,
      selected: rgb == value,
      button: true,
      child: SizedBox.square(
        dimension: 44,
        child: Padding(
          padding: const EdgeInsets.all(3),
          child: Material(
            color: Color(0xff000000 | value),
            shape: CircleBorder(
              side: BorderSide(
                color: rgb == value
                    ? Theme.of(context).colorScheme.primary
                    : Theme.of(context).colorScheme.outlineVariant,
                width: 2,
              ),
            ),
            child: InkWell(
              customBorder: const CircleBorder(),
              onTap: () => setColor(value),
              child: rgb == value
                  ? Icon(
                      Icons.check,
                      size: 18,
                      color: Color(0xff000000 | value).computeLuminance() > .4
                          ? Colors.black
                          : Colors.white,
                    )
                  : null,
            ),
          ),
        ),
      ),
    ),
  );
  @override
  Widget build(BuildContext context) => AlertDialog(
    title: Text(widget.title),
    content: SizedBox(
      width: 380,
      child: SingleChildScrollView(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Container(
                  width: 48,
                  height: 48,
                  decoration: BoxDecoration(
                    color: Color(0xff000000 | rgb),
                    borderRadius: BorderRadius.circular(12),
                    border: Border.all(
                      color: Theme.of(context).colorScheme.outlineVariant,
                    ),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Text(
                    rgbName(rgb),
                    style: Theme.of(context).textTheme.titleMedium,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 16),
            Wrap(
              children: [
                for (final entry in ledColorPalette.entries)
                  swatch(entry.key, entry.value),
              ],
            ),
            if (_recentLedColors.isNotEmpty) ...[
              const SizedBox(height: 12),
              const Text('Recent colors'),
              Wrap(
                children: [
                  for (final value in _recentLedColors)
                    swatch(rgbName(value), value),
                ],
              ),
            ],
            const SizedBox(height: 12),
            for (final part in ['Hue', 'Saturation', 'Brightness']) ...[
              Text(part),
              Slider(
                label: part == 'Hue'
                    ? '${hsv.hue.round()}°'
                    : '${((part == 'Saturation' ? hsv.saturation : hsv.value) * 100).round()}%',
                value: part == 'Hue'
                    ? hsv.hue
                    : part == 'Saturation'
                    ? hsv.saturation
                    : hsv.value,
                max: part == 'Hue' ? 360 : 1,
                onChanged: (v) => change(
                  part == 'Hue'
                      ? hsv.withHue(v)
                      : part == 'Saturation'
                      ? hsv.withSaturation(v)
                      : hsv.withValue(v),
                ),
              ),
            ],
            TextField(
              controller: hex,
              maxLength: 6,
              inputFormatters: [
                FilteringTextInputFormatter.allow(RegExp('[0-9a-fA-F]')),
              ],
              decoration: InputDecoration(
                labelText: 'RGB hex',
                prefixText: '#',
                errorText: valid ? null : 'Enter six hexadecimal digits',
              ),
              onChanged: (text) => setState(() {
                valid = text.length == 6;
                if (valid) {
                  hsv = HSVColor.fromColor(
                    Color(0xff000000 | int.parse(text, radix: 16)),
                  );
                }
              }),
            ),
          ],
        ),
      ),
    ),
    actions: [
      TextButton(
        onPressed: () => Navigator.pop(context),
        child: const Text('Cancel'),
      ),
      FilledButton.icon(
        onPressed: valid ? () => Navigator.pop(context, rgb) : null,
        icon: const Icon(Icons.check),
        label: const Text('Apply'),
      ),
    ],
  );
}
