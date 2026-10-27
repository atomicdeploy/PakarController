import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'cell_pixel_preview.dart';
import 'controller_dialog.dart';
import 'models.dart';
import 'pixel_diagnostic.dart';
import 'workspace_widgets.dart';

typedef OpenPixelDiagnostics = Future<void> Function(
  PixelProbe probe, {
  bool corrections,
  bool groups,
});

/// A physical address inspector. Opening/selecting never writes to the board.
/// Live colours are published observations, not pending test-colour drafts.
class CellPixelDialog extends StatefulWidget {
  const CellPixelDialog({
    super.key,
    required this.id,
    required this.mapping,
    required this.observed,
    required this.current,
    required this.openDiagnostics,
    required this.editMapping,
    this.sendProbe,
    this.updates,
  });
  final int id;
  final CellMap mapping;
  final DeviceStatus? Function() observed;
  final bool Function() current;
  final OpenPixelDiagnostics openDiagnostics;
  final VoidCallback editMapping;
  final Future<DeviceStatus> Function(PixelProbe)? sendProbe;
  final Stream<DeviceStatus>? updates;
  @override
  State<CellPixelDialog> createState() => _CellPixelDialogState();
}

class _CellPixelDialogState extends State<CellPixelDialog> {
  late final layout = widget.observed()?.pixelGroups;
  late final groupId = widget.mapping.physical[widget.id - 1];
  late final first = layout?.start(groupId) ?? 0;
  late final count = layout?.count(groupId) ?? 0;
  late int selected = first;
  int color = 0x00ff00;
  bool busy = false;
  String? feedback;
  final hex = TextEditingController(text: '00FF00');
  StreamSubscription<DeviceStatus>? subscription;
  @override
  void initState() {
    super.initState();
    subscription = widget.updates?.listen((_) {
      if (mounted) setState(() {});
    });
  }

  @override
  void dispose() {
    subscription?.cancel();
    hex.dispose();
    super.dispose();
  }

  bool get available =>
      widget.current() &&
      !busy &&
      layout != null &&
      widget.observed()?.pixelGroups?.sameAs(layout!) == true;
  bool get overlay =>
      widget.observed()?.peer.has('pixel-diagnostic-overlay') == true;
  PixelProbe probe({bool group = false, bool solo = false}) => PixelProbe(
    index: group ? first : selected,
    group: group,
    rgb: color,
    overlay: !solo && overlay,
    layout: widget.observed()?.peer.has('pixel-group-layout') == true
        ? layout
        : null,
  );
  Future<void> send({bool group = false, bool solo = false}) async {
    if (!available || widget.sendProbe == null) return;
    setState(() {
      busy = true;
      feedback = null;
    });
    try {
      await widget.sendProbe!(probe(group: group, solo: solo));
      if (mounted) {
        setState(() => feedback = 'Board acknowledged the diagnostic.');
      }
    } catch (error) {
      if (mounted) setState(() => feedback = '$error');
    } finally {
      if (mounted) setState(() => busy = false);
    }
  }

  Future<void> open({
    bool group = false,
    bool corrections = false,
    bool groups = false,
  }) async {
    if (!available) return;
    final next = probe(group: group);
    Navigator.pop(context);
    await widget.openDiagnostics(
      next,
      corrections: corrections,
      groups: groups,
    );
  }

  Future<void> pixelMenu(int index) async {
    if (!available) return;
    setState(() => selected = index);
    final action = await showModalBottomSheet<String>(
      context: context,
      showDragHandle: true,
      isScrollControlled: true,
      builder: (context) => SafeArea(
        child: SingleChildScrollView(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              ListTile(
                title: Text('Physical pixel ${index + 1}'),
                subtitle: const Text(
                  'Choose a test or inspect its correction rule',
                ),
              ),
              for (final item in [
                (
                  'solo',
                  Icons.highlight,
                  'Solo this pixel',
                  'Turn the other pixels off for this test',
                ),
                (
                  'group',
                  Icons.apps,
                  'Solo this $count-pixel group',
                  'Test the complete mapped cell',
                ),
                (
                  'diagnostics',
                  pixelDiagnosticIcon,
                  'Open pixel diagnostics',
                  'Start at this physical pixel',
                ),
                if (widget.observed()?.peer.has('pixel-corrections') == true)
                  (
                    'corrections',
                    pixelCorrectionIcon,
                    'Open pixel corrections',
                    'Use this address in a correction rule',
                  ),
              ])
                ListTile(
                  leading: Icon(item.$2),
                  title: Text(item.$3),
                  subtitle: Text(item.$4),
                  onTap: () => Navigator.pop(context, item.$1),
                ),
            ],
          ),
        ),
      ),
    );
    if (!mounted || !available) return;
    switch (action) {
      case 'solo':
        await send(solo: true);
      case 'group':
        await send(group: true, solo: true);
      case 'diagnostics':
        await open();
      case 'corrections':
        await open(corrections: true);
    }
  }

  @override
  Widget build(BuildContext context) {
    final colors = Theme.of(context).colorScheme;
    final status = widget.observed();
    final sample = widget.current()
        ? cellPixelPreview(status, widget.mapping, widget.id)
        : null;
    return PopScope(
      canPop: !busy,
      child: ControllerDialog(
        title:
            'Cell ${widget.id} · physical pixels ${first + 1}–${first + count}',
        icon: pixelDiagnosticIcon,
        closeEnabled: !busy,
        maxWidth: 600,
        actions: [
          TextButton.icon(
            onPressed: available
                ? () {
                    Navigator.pop(context);
                    widget.editMapping();
                  }
                : null,
            icon: const Icon(Icons.edit_outlined),
            label: const Text('Name and mapping'),
          ),
          TextButton.icon(
            onPressed: available ? () => open() : null,
            icon: const Icon(Icons.tune),
            label: const Text('Advanced diagnostics'),
          ),
        ],
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              sample?.description ?? 'No current RGB sample',
              style: TextStyle(color: colors.onSurfaceVariant),
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                for (var offset = 0; offset < count; offset++)
                  SizedBox(
                    width: 74,
                    child: Semantics(
                      label: 'Physical pixel ${first + offset + 1}',
                      selected: selected == first + offset,
                      child: InkWell(
                        key: ValueKey('physical-pixel-${first + offset + 1}'),
                        borderRadius: BorderRadius.circular(12),
                        onTap: available
                            ? () => setState(() => selected = first + offset)
                            : null,
                        onLongPress: available
                            ? () => pixelMenu(first + offset)
                            : null,
                        child: Container(
                          padding: const EdgeInsets.symmetric(vertical: 12),
                          decoration: BoxDecoration(
                            color: selected == first + offset
                                ? colors.primary.withValues(alpha: .12)
                                : colors.surfaceContainerHigh,
                            border: Border.all(
                              color: selected == first + offset
                                  ? colors.primary
                                  : colors.outlineVariant,
                            ),
                            borderRadius: BorderRadius.circular(12),
                          ),
                          child: Column(
                            children: [
                              Container(
                                width: 22,
                                height: 22,
                                decoration: BoxDecoration(
                                  shape: BoxShape.circle,
                                  color:
                                      sample == null ||
                                          offset >= sample.colors.length
                                      ? colors.surfaceContainerHighest
                                      : Color(
                                          0xff000000 | sample.colors[offset],
                                        ),
                                  border: Border.all(
                                    color: colors.outlineVariant,
                                  ),
                                ),
                                child:
                                    sample == null ||
                                        offset >= sample.colors.length
                                    ? Icon(
                                        Icons.question_mark,
                                        size: 14,
                                        color: colors.onSurfaceVariant,
                                      )
                                    : null,
                              ),
                              const SizedBox(height: 8),
                              Text(
                                '${first + offset + 1}',
                                style: const TextStyle(
                                  fontWeight: FontWeight.w600,
                                ),
                              ),
                            ],
                          ),
                        ),
                      ),
                    ),
                  ),
              ],
            ),
            const SizedBox(height: 8),
            Text(
              'Tap a number to control it; hold for solo and diagnostic actions.',
              style: Theme.of(context).textTheme.bodySmall
                  ?.copyWith(color: colors.onSurfaceVariant),
            ),
            if (status?.peer.has('pixel-corrections') == true)
              Padding(
                padding: const EdgeInsets.only(top: 4),
                child: Text(
                  'Addresses are before output correction rules.',
                  style: Theme.of(context).textTheme.bodySmall
                      ?.copyWith(color: colors.onSurfaceVariant),
                ),
              ),
            const SizedBox(height: 20),
            Text(
              'Test color · pixel ${selected + 1}',
              style: Theme.of(context).textTheme.titleSmall,
            ),
            const SizedBox(height: 10),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                for (final entry in [
                  ('White', 0xffffff),
                  ('Red', 0xff0000),
                  ('Green', 0x00ff00),
                  ('Blue', 0x0000ff),
                  ('Off', 0),
                ])
                  ChoiceChip(
                    label: Text(entry.$1),
                    avatar: Icon(
                      Icons.circle,
                      size: 14,
                      color: entry.$2 == 0
                          ? colors.onSurfaceVariant
                          : Color(0xff000000 | entry.$2),
                    ),
                    selected: color == entry.$2,
                    onSelected: available
                        ? (_) => setState(() {
                            color = entry.$2;
                            hex.text = color
                                .toRadixString(16)
                                .padLeft(6, '0')
                                .toUpperCase();
                          })
                        : null,
                  ),
              ],
            ),
            const SizedBox(height: 12),
            SizedBox(
              width: 180,
              child: TextField(
                controller: hex,
                enabled: available,
                inputFormatters: [
                  FilteringTextInputFormatter.allow(RegExp('[0-9a-fA-F]')),
                  LengthLimitingTextInputFormatter(6),
                ],
                decoration: InputDecoration(
                  labelText: 'Custom RGB',
                  prefixText: '#',
                  errorText: hex.text.length == 6
                      ? null
                      : 'Use six hexadecimal digits',
                ),
                onChanged: (value) => setState(() {
                  if (value.length == 6) color = int.parse(value, radix: 16);
                }),
              ),
            ),
            const SizedBox(height: 12),
            Wrap(
              spacing: 8,
              runSpacing: 8,
              children: [
                FilledButton.icon(
                  key: const ValueKey('pixel-apply-color'),
                  onPressed:
                      available &&
                          hex.text.length == 6 &&
                          widget.sendProbe != null
                      ? () => send(solo: !overlay)
                      : null,
                  icon: Icon(overlay ? Icons.layers_outlined : Icons.highlight),
                  label: Text(
                    overlay
                        ? 'Test pixel · keep others'
                        : 'Solo pixel · clear others',
                  ),
                ),
                OutlinedButton.icon(
                  onPressed: available ? () => open(group: true) : null,
                  icon: const Icon(pixelDiagnosticIcon),
                  label: const Text('Diagnose this group'),
                ),
                if (status?.peer.has('pixel-group-layout') == true)
                  TextButton.icon(
                    onPressed: available ? () => open(groups: true) : null,
                    icon: const Icon(Icons.format_list_numbered),
                    label: const Text('Pixels in this group'),
                  ),
              ],
            ),
            if (feedback != null)
              Padding(
                padding: const EdgeInsets.only(top: 12),
                child: Text(feedback!),
              ),
            if (!widget.current())
              Padding(
                padding: const EdgeInsets.only(top: 12),
                child: Text(
                  'Connection changed. Reopen the cell from the current board.',
                  style: TextStyle(color: colors.error),
                ),
              ),
          ],
        ),
      ),
    );
  }
}
