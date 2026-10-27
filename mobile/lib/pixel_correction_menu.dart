import 'package:flutter/material.dart';

import 'api.dart';
import 'controller_dialog.dart';
import 'pixel_correction_address.dart';
import 'pixel_corrections_editor.dart';

/// Reuses the imported PCController phone/tablet dialog. Choosing an operation
/// creates an editor intent only; persistence and live preview remain explicit.
Future<PixelCorrectionRuleIntent?> showPixelCorrectionMenu(
  BuildContext context,
  int pixel, {
  required PakarApi api,
  required bool Function() authorize,
}) => showDialog<PixelCorrectionRuleIntent>(
  context: context,
  builder: (dialog) =>
      _CorrectionMenu(raw: pixel, api: api, authorize: authorize),
);

class _CorrectionMenu extends StatefulWidget {
  const _CorrectionMenu({
    required this.raw,
    required this.api,
    required this.authorize,
  });
  final int raw;
  final PakarApi api;
  final bool Function() authorize;
  @override
  State<_CorrectionMenu> createState() => _CorrectionMenuState();
}

class _CorrectionMenuState extends State<_CorrectionMenu> {
  int? wire, revision;
  int pixels = 0;
  List<int>? outputs;
  String? error;
  @override
  void initState() {
    super.initState();
    load();
  }

  Future<void> load() async {
    try {
      if (!widget.authorize()) {
        throw const FormatException('Device connection changed.');
      }
      final value = await widget.api.getJson('/api/pixels/corrections');
      if (!mounted) return;
      if (!widget.authorize()) {
        throw const FormatException('Device connection changed.');
      }
      if (value['pixels'] is! int ||
          value['revision'] is! int ||
          value['revision'] < 0 ||
          value['revision'] > 0xffffffff ||
          (value.containsKey('index_base') && value['index_base'] != 0)) {
        throw const FormatException('Correction output mapping is unavailable');
      }
      if (value['preview'] is Map && value['preview']['active'] == true) {
        throw const FormatException(
          'Stop the active correction preview before selecting an output slot.',
        );
      }
      final matches = pixelCorrectionOutputSlots(
        raw: widget.raw,
        pixels: value['pixels'] as int,
        sources: value['sources'],
      );
      setState(() {
        pixels = value['pixels'] as int;
        revision = value['revision'] as int;
        outputs = matches;
        wire = matches.length == 1 ? matches.single : null;
      });
    } catch (failure) {
      if (mounted) setState(() => error = failure.toString());
    }
  }

  @override
  Widget build(BuildContext context) => ControllerDialog(
    title: wire == null
        ? 'Correct source pixel ${widget.raw + 1}'
        : 'Correct output slot ${wire! + 1}',
    icon: Icons.tune_rounded,
    maxWidth: 520,
    child: Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Text('Diagnostic source ${widget.raw + 1}'),
        if (error != null)
          Text(
            error!,
            style: TextStyle(color: Theme.of(context).colorScheme.error),
          ),
        if (outputs == null && error == null)
          const Padding(
            padding: EdgeInsets.all(12),
            child: CircularProgressIndicator(),
          ),
        if (outputs != null && outputs!.length > 1)
          DropdownButtonFormField<int>(
            key: const Key('correction-output-choice'),
            initialValue: wire,
            decoration: const InputDecoration(labelText: 'Output slot'),
            hint: const Text('Choose an output slot'),
            items: [
              for (final index in outputs!)
                DropdownMenuItem(
                  value: index,
                  child: Text('Output slot ${index + 1}'),
                ),
            ],
            onChanged: (value) => setState(() => wire = value),
          ),
        if (outputs != null && outputs!.isEmpty) ...[
          const Text(
            'This source is not present in the saved output. Choose the wire slot to correct.',
          ),
          TextFormField(
            key: const Key('correction-output-choice'),
            decoration: InputDecoration(labelText: 'Output slot (1–$pixels)'),
            keyboardType: TextInputType.number,
            onChanged: (text) {
              final value = int.tryParse(text);
              setState(
                () => wire = value != null && value >= 1 && value <= pixels
                    ? value - 1
                    : null,
              );
            },
          ),
        ],
        for (final option in [
          (
            'black',
            0,
            Icons.lightbulb_outline,
            'Black out',
            'Keep this address; send black.',
          ),
          (
            'remove',
            0,
            Icons.hide_source_outlined,
            'Remove address',
            'Skip this value; move following values earlier.',
          ),
          (
            'shift',
            1,
            Icons.arrow_forward_rounded,
            'Shift forward',
            'Move this and following values one slot later.',
          ),
          (
            'shift',
            -1,
            Icons.arrow_back_rounded,
            'Shift backward',
            'Move this and following values one slot earlier.',
          ),
        ])
          ListTile(
            key: ValueKey('correction-menu-${option.$1}-${option.$2}'),
            contentPadding: const EdgeInsets.symmetric(horizontal: 4),
            leading: Icon(
              option.$3,
              color: Theme.of(context).colorScheme.primary,
            ),
            title: Text(option.$4),
            subtitle: Text(option.$5),
            trailing: const Icon(Icons.chevron_right, size: 20),
            enabled: wire != null && widget.authorize(),
            onTap: wire == null
                ? null
                : () {
                    if (!widget.authorize()) return;
                    Navigator.pop(
                      context,
                      PixelCorrectionRuleIntent(
                        kind: option.$1,
                        pixel: wire!,
                        offset: option.$2,
                        revision: revision,
                      ),
                    );
                  },
          ),
      ],
    ),
  );
}
