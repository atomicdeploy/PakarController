import 'dart:async';

import 'package:flutter/material.dart';

import 'api.dart';
import 'icons.dart';
import 'models.dart';
import 'pixel_diagnostic.dart';
import 'workspace_widgets.dart';

/// Uses the same Studio workspace components as settings/firmware/bridge.
/// No startup writes, automatic chase, hidden restore, or reconnect replay.
class PixelDiagnosticPage extends StatefulWidget {
  const PixelDiagnosticPage({
    super.key,
    required this.api,
    required this.observed,
    required this.busyChanged,
    this.onObserved,
    this.workspace,
    this.mapping,
  });
  final PakarApi api;
  final DeviceStatus? Function() observed;
  final void Function(bool) busyChanged;
  final void Function(DeviceStatus)? onObserved;
  final PixelDiagnosticWorkspace? workspace;
  final CellMap? Function()? mapping;
  @override
  State<PixelDiagnosticPage> createState() => _PixelDiagnosticPageState();
}

class _PixelDiagnosticPageState extends State<PixelDiagnosticPage> {
  late final workspace = widget.workspace ?? PixelDiagnosticWorkspace();
  PixelProbe get probe => workspace.probe;
  set probe(PixelProbe value) => workspace.probe = value;
  late final revision = widget.api.connectionRevision;
  late final board = widget.observed()?.json['board_id'];
  late final boot = widget.observed()?.json['boot_id'];
  final position = TextEditingController(text: '1');
  final colorText = TextEditingController();
  Timer? timer;
  bool working = false;
  String message =
      'Choose a position and colour, then Show. Nothing is sent automatically.';
  String? applied;
  bool allOff = false;
  @override
  void initState() {
    super.initState();
    // Eagerly capture the owner before navigation or network state can change.
    revision;
    board;
    boot;
    position.text = '${probe.first + 1}';
    timer = Timer.periodic(const Duration(milliseconds: 500), (_) {
      if (mounted) setState(() {});
    });
  }

  bool get authorized {
    final status = widget.observed();
    return mounted &&
        revision == widget.api.connectionRevision &&
        board != null &&
        boot != null &&
        status?.json['board_id'] == board &&
        status?.json['boot_id'] == boot &&
        supportsPixelDiagnostic(status) &&
        widget.api.appForeground &&
        status?.peer.has('command-ack') == true;
  }

  @override
  void dispose() {
    timer?.cancel();
    position.dispose();
    colorText.dispose();
    super.dispose();
  }

  Future<void> send(PixelProbe next, {bool off = false}) async {
    if (working || !authorized) return;
    FocusScope.of(context).unfocus();
    setState(() {
      working = true;
      message = off ? 'Clearing all 480 pixels…' : 'Showing ${next.range}…';
      applied = null;
    });
    widget.busyChanged(true);
    try {
      final actual = await widget.api.pixelDiagnostic(
        next,
        off: off,
        authorize: () => authorized,
      );
      if (mounted) widget.onObserved?.call(actual);
      if (mounted && authorized) {
        setState(() {
          probe = next;
          allOff = off;
          position.text = '${next.first + 1}';
          applied = off
              ? 'Last verified: all 480 pixels off'
              : 'Last verified: ${next.range} · #${next.hex}';
          message =
              'Board RGB readback matched. Please check the physical LEDs.';
        });
      }
    } catch (error) {
      if (mounted) {
        setState(() {
          applied = null;
          message = '$error No frame was replayed.';
          position.text = '${probe.first + 1}';
        });
      }
    } finally {
      if (mounted) {
        setState(() => working = false);
        widget.busyChanged(false);
      }
    }
  }

  void change(PixelProbe next) {
    if (applied != null && !allOff) {
      unawaited(send(next));
    } else {
      setState(() {
        probe = next;
        position.text = '${next.first + 1}';
      });
    }
  }

  PixelProbe? readPosition() {
    final index = int.tryParse(position.text.trim());
    if (index == null || index < 1 || index > diagnosticPixels) {
      setState(() => message = 'Enter a physical pixel number from 1 to 480.');
      return null;
    }
    return PixelProbe(index: index - 1, group: probe.group, rgb: probe.rgb);
  }

  Future<void> chooseColor() async {
    final text = colorText..text = probe.hex;
    var value = probe.rgb;
    final result = await showDialog<int>(
      context: context,
      builder: (context) => StatefulBuilder(
        builder: (context, update) => AlertDialog(
          scrollable: true,
          title: const Text('Diagnostic colour'),
          content: SizedBox(
            width: 340,
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                Container(
                  height: 44,
                  decoration: BoxDecoration(
                    color: Color(0xff000000 | value),
                    borderRadius: BorderRadius.circular(8),
                  ),
                ),
                const SizedBox(height: 16),
                TextField(
                  controller: text,
                  maxLength: 7,
                  decoration: const InputDecoration(
                    labelText: 'RGB hex',
                    hintText: 'FF0000',
                  ),
                  onChanged: (input) {
                    final hex = input.replaceFirst('#', '');
                    if (RegExp(r'^[0-9a-fA-F]{6}$').hasMatch(hex)) {
                      update(() => value = int.parse(hex, radix: 16));
                    }
                  },
                ),
                for (final channel in [(16, 'Red'), (8, 'Green'), (0, 'Blue')])
                  Row(
                    children: [
                      SizedBox(width: 50, child: Text(channel.$2)),
                      Expanded(
                        child: Slider(
                          value: ((value >> channel.$1) & 255).toDouble(),
                          max: 255,
                          divisions: 255,
                          onChanged: (v) => update(() {
                            value =
                                value & ~(255 << channel.$1) |
                                v.round() << channel.$1;
                            text.text = value
                                .toRadixString(16)
                                .padLeft(6, '0')
                                .toUpperCase();
                          }),
                        ),
                      ),
                    ],
                  ),
              ],
            ),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.pop(context),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () {
                final hex = text.text.replaceFirst('#', '');
                if (RegExp(r'^[0-9a-fA-F]{6}$').hasMatch(hex)) {
                  Navigator.pop(context, int.parse(hex, radix: 16));
                }
              },
              child: const Text('Use colour'),
            ),
          ],
        ),
      ),
    );
    if (result != null && mounted) {
      change(PixelProbe(index: probe.index, group: probe.group, rgb: result));
    }
  }

  @override
  Widget build(BuildContext context) {
    final enabled = authorized && !working;
    final colours = Theme.of(context).colorScheme;
    final map = widget.mapping?.call();
    final group = probe.first ~/ 6 + 1;
    final logical = map?.valid == true
        ? map!.physical.indexOf(group) + 1
        : null;
    final first = probe.index ~/ 6 * 6;
    return WorkspaceBody(
      children: [
        const WorkspaceHeader(
          title: 'Pixel diagnostics',
          icon: PakarIcons.circuitry,
          subtitle: 'Physical wire order · all other pixels cleared',
        ),
        WorkspaceSection(
          title: probe.group ? 'Six-pixel probe' : 'Single-pixel probe',
          icon: PakarIcons.gridFour,
          children: [
            SegmentedButton<bool>(
              segments: const [
                ButtonSegment(value: false, label: Text('1 pixel')),
                ButtonSegment(value: true, label: Text('6-pixel group')),
              ],
              selected: {probe.group},
              onSelectionChanged: enabled
                  ? (choice) => change(
                      PixelProbe(
                        index: probe.index,
                        group: choice.single,
                        rgb: probe.rgb,
                      ),
                    )
                  : null,
            ),
            const SizedBox(height: 16),
            Text(
              probe.range,
              style: Theme.of(context).textTheme.headlineMedium,
            ),
            Text(
              'Physical group $group of 80${logical != null ? ' · logical cell $logical' : ''}',
              style: TextStyle(color: colours.onSurfaceVariant),
            ),
            const SizedBox(height: 12),
            // Tap a sector to inspect any of the six physical pixels directly.
            Row(
              children: [
                for (var sector = 0; sector < 6; sector++)
                  Expanded(
                    child: Tooltip(
                      message: 'Physical pixel ${first + sector + 1}',
                      child: InkWell(
                        key: Key('pixel-sector-$sector'),
                        borderRadius: BorderRadius.circular(8),
                        onTap: enabled
                            ? () => send(
                                PixelProbe(
                                  index: first + sector,
                                  rgb: probe.rgb,
                                ),
                              )
                            : null,
                        child: Padding(
                          padding: const EdgeInsets.symmetric(vertical: 10),
                          child: Column(
                            children: [
                              AnimatedContainer(
                                duration: const Duration(milliseconds: 120),
                                height: 25,
                                width: 25,
                                decoration: BoxDecoration(
                                  shape: BoxShape.circle,
                                  color:
                                      !allOff &&
                                          (probe.group ||
                                              first + sector == probe.index)
                                      ? Color(0xff000000 | probe.rgb)
                                      : colours.surfaceContainerHigh,
                                  border: Border.all(color: colours.outline),
                                ),
                              ),
                              const SizedBox(height: 4),
                              Text(
                                '${first + sector + 1}',
                                style: Theme.of(context).textTheme.labelSmall,
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
            Row(
              children: [
                Expanded(
                  child: OutlinedButton.icon(
                    key: const Key('pixel-previous'),
                    onPressed: enabled && probe.canPrevious
                        ? () => send(probe.move(-1))
                        : null,
                    icon: const Icon(Icons.arrow_back),
                    label: Text('−${probe.count}'),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: FilledButton.icon(
                    key: const Key('pixel-next'),
                    onPressed: enabled && probe.canNext
                        ? () => send(probe.move(1))
                        : null,
                    icon: const Icon(Icons.arrow_forward),
                    label: Text('+${probe.count}'),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 14),
            Row(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Expanded(
                  child: TextField(
                    key: const Key('pixel-position'),
                    controller: position,
                    enabled: enabled,
                    style: TextStyle(color: colours.onSurface),
                    keyboardType: TextInputType.number,
                    decoration: const InputDecoration(
                      labelText: 'Physical pixel · 1–480',
                    ),
                    onSubmitted: enabled
                        ? (_) {
                            final next = readPosition();
                            if (next != null) send(next);
                          }
                        : null,
                  ),
                ),
                const SizedBox(width: 10),
                FilledButton(
                  key: const Key('pixel-show'),
                  onPressed: enabled
                      ? () {
                          final next = readPosition();
                          if (next != null) send(next);
                        }
                      : null,
                  child: const Text('Show'),
                ),
              ],
            ),
            const SizedBox(height: 14),
            Wrap(
              spacing: 6,
              runSpacing: 6,
              children: [
                for (final item in [
                  (0xffffff, 'White'),
                  (0xff0000, 'Red'),
                  (0x00ff00, 'Green'),
                  (0x0000ff, 'Blue'),
                ])
                  ChoiceChip(
                    label: Text(item.$2),
                    selected: probe.rgb == item.$1,
                    onSelected: enabled
                        ? (_) => change(
                            PixelProbe(
                              index: probe.index,
                              group: probe.group,
                              rgb: item.$1,
                            ),
                          )
                        : null,
                  ),
                ActionChip(
                  avatar: CircleAvatar(
                    backgroundColor: Color(0xff000000 | probe.rgb),
                  ),
                  label: Text('#${probe.hex}'),
                  onPressed: enabled ? chooseColor : null,
                ),
              ],
            ),
            const SizedBox(height: 12),
            OutlinedButton.icon(
              key: const Key('pixel-off'),
              icon: const Icon(PakarIcons.circle),
              label: const Text('All off · clear 480 pixels'),
              onPressed: enabled ? () => send(probe, off: true) : null,
            ),
            const SizedBox(height: 12),
            // Fixed height: pending/confirmed/error never shifts the controls.
            SizedBox(
              height: 4,
              child: working
                  ? const LinearProgressIndicator()
                  : const SizedBox(),
            ),
            const SizedBox(height: 10),
            if (!authorized)
              Text(
                'Waiting for this board connection. No command was queued.',
                style: TextStyle(color: colours.error),
              ),
            if (applied != null && authorized)
              Text(applied!, style: TextStyle(color: colours.secondary)),
            Text(message, style: Theme.of(context).textTheme.bodySmall),
          ],
        ),
        WorkspaceSection(
          title: 'Wire reference',
          icon: PakarIcons.circuitry,
          children: [
            Text(
              'Zero-based index ${probe.first}${probe.group ? '–${probe.last}' : ''} · step ${probe.count}',
            ),
            Text(
              'RGB #${probe.hex} · R ${probe.rgb >> 16 & 255} · G ${probe.rgb >> 8 & 255} · B ${probe.rgb & 255}',
            ),
            const SizedBox(height: 8),
            const Text(
              'Show clears every other pixel. After Show, colour and group changes apply immediately. All off keeps your cursor. Closing leaves the last frame on the board.',
            ),
            if (widget.observed()?.peer.has('pixel-diagnostic') != true)
              const Padding(
                padding: EdgeInsets.only(top: 8),
                child: Text(
                  'Older board firmware: WebSocket streaming is required. Update firmware for atomic diagnostics and HTTP fallback.',
                ),
              ),
          ],
        ),
      ],
    );
  }
}
