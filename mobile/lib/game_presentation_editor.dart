import 'dart:convert';

import 'package:flutter/material.dart';

import 'game_settings_command.dart';
import 'color_picker.dart';

/// Reuses PCController EffectEditor local JSON copy, keyed numeric drafts and
/// validation-before-save; game fields replace its host sequence definition.
/// Only the selected timing/group patch leaves this dialog.
class GamePresentationEditor extends StatefulWidget {
  const GamePresentationEditor({super.key, required this.presentation});
  final Map presentation;
  @override
  State<GamePresentationEditor> createState() => _GamePresentationEditorState();
}

class _GamePresentationEditorState extends State<GamePresentationEditor> {
  late final Map<String, dynamic> draft = Map<String, dynamic>.from(
    jsonDecode(jsonEncode(widget.presentation)) as Map,
  );
  int selected = 0;
  final invalid = <String>{};
  final numberDrafts = <String, String>{};
  Map<String, dynamic> get group => Map<String, dynamic>.from(
    (draft['groups'] as List).firstWhere((g) => g['id'] == selected) as Map,
  );
  void field(String name, Object value) {
    if (selected == 0) {
      draft[name] = value;
    } else {
      ((draft['groups'] as List).firstWhere((g) => g['id'] == selected)
              as Map)[name] =
          value;
    }
  }

  Widget number(
    String name,
    String label,
    Object? value, {
    bool hex = false,
  }) => Padding(
    padding: const EdgeInsets.only(top: 12),
    child: TextFormField(
      key: ValueKey('$selected/$name'),
      initialValue:
          numberDrafts['$selected/$name'] ??
          (hex ? (value as int).toRadixString(16).padLeft(6, '0') : '$value'),
      keyboardType: hex ? TextInputType.text : TextInputType.number,
      decoration: InputDecoration(
        labelText: label,
        prefixText: hex ? '#' : null,
        errorText: invalid.contains('$selected/$name')
            ? 'Enter a valid ${hex ? 'six-digit color' : 'value (${presentationLimits[name]!.$1}–${presentationLimits[name]!.$2})'}'
            : null,
      ),
      onChanged: (text) => setState(() {
        numberDrafts['$selected/$name'] = text;
        final n = int.tryParse(text, radix: hex ? 16 : 10);
        final range = hex ? (0, 0xffffff) : presentationLimits[name]!;
        if (n == null ||
            n < range.$1 ||
            n > range.$2 ||
            (hex && !RegExp(r'^[a-fA-F0-9]{6}$').hasMatch(text))) {
          invalid.add('$selected/$name');
        } else {
          invalid.remove('$selected/$name');
          field(name, n);
        }
      }),
    ),
  );
  Widget color(String name, String label, int value) => ListTile(
    contentPadding: EdgeInsets.zero,
    title: Text(label),
    subtitle: Text(rgbName(value)),
    leading: Icon(Icons.circle, color: Color(0xff000000 | value)),
    trailing: const Icon(Icons.palette_outlined),
    onTap: () async {
      final next = await showLedColorPicker(
        context,
        initial: value,
        title: label,
      );
      if (next != null && mounted) setState(() => field(name, next));
    },
  );
  @override
  Widget build(BuildContext context) {
    final current = selected == 0 ? draft : group;
    return AlertDialog(
      title: const Text('Round presentation'),
      content: SizedBox(
        width: 430,
        child: SingleChildScrollView(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              DropdownButtonFormField<int>(
                initialValue: selected,
                decoration: const InputDecoration(labelText: 'Edit'),
                items: [
                  const DropdownMenuItem(value: 0, child: Text('Round timing')),
                  for (var id = 1; id <= 18; id++)
                    DropdownMenuItem(
                      value: id,
                      child: Text(
                        'Box ${(id + 1) ~/ 2} · ${id.isOdd ? 'button' : 'indicator'}',
                      ),
                    ),
                ],
                onChanged: (value) => setState(() {
                  selected = value!;
                }),
              ),
              if (selected == 0) ...[
                for (final entry in const {
                  'flashes': 'Winner flashes',
                  'flash_on_ms': 'Flash on (ms)',
                  'flash_off_ms': 'Flash gap (ms)',
                  'fade_ms': 'Fade (ms)',
                  'pulse_ms': 'Pulse period (ms)',
                  'chase_ms': 'Chase period (ms)',
                }.entries)
                  number(entry.key, entry.value, current[entry.key]),
              ] else ...[
                SwitchListTile(
                  contentPadding: EdgeInsets.zero,
                  title: const Text('Custom group colors'),
                  value: current['override_colors'] == true,
                  onChanged: (v) => setState(() => field('override_colors', v)),
                ),
                if (current['override_colors'] == true) ...[
                  color('win_rgb', 'Winner color', current['win_rgb'] as int),
                  color(
                    'lose_rgb',
                    'Other box color',
                    current['lose_rgb'] as int,
                  ),
                ],
                for (final name in ['win_motion', 'lose_motion'])
                  Padding(
                    padding: const EdgeInsets.only(top: 12),
                    child: DropdownButtonFormField<int>(
                      key: ValueKey('$selected/$name'),
                      initialValue: current[name] as int,
                      decoration: InputDecoration(
                        labelText: name == 'win_motion'
                            ? 'Winner motion'
                            : 'Other box motion',
                      ),
                      items: [
                        for (final entry in const {
                          0: 'Solid',
                          1: 'Pulse',
                          2: 'Ring',
                          3: 'Shared loser chase',
                        }.entries)
                          if (entry.key != 3 ||
                              (selected.isEven && name == 'lose_motion') ||
                              current[name] == 3)
                            DropdownMenuItem(
                              value: entry.key,
                              child: Text(entry.value),
                            ),
                      ],
                      onChanged: (v) => setState(() => field(name, v!)),
                    ),
                  ),
              ],
            ],
          ),
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: const Text('Cancel'),
        ),
        FilledButton(
          onPressed: invalid.isNotEmpty
              ? null
              : () {
                  final patch = selected == 0
                      ? {
                          for (final key in presentationLimits.keys)
                            key: draft[key],
                        }
                      : {
                          'groups': [group],
                        };
                  Navigator.pop(context, presentationPatch(patch));
                },
          child: const Text('Save selected settings'),
        ),
      ],
    );
  }
}
