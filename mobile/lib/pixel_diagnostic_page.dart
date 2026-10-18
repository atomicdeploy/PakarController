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
  });
  final PakarApi api;
  final DeviceStatus? Function() observed;
  final void Function(bool) busyChanged;
  @override
  State<PixelDiagnosticPage> createState() => _PixelDiagnosticPageState();
}

class _PixelDiagnosticPageState extends State<PixelDiagnosticPage> {
  PixelProbe probe = const PixelProbe();
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
  @override
  void initState() {
    super.initState();
    // Eagerly capture the owner before navigation or network state can change.
    revision;
    board;
    boot;
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
      probe = next;
      position.text = '${next.first + 1}';
      message = 'Sending complete frame…';
      applied = null;
    });
    widget.busyChanged(true);
    try {
      await widget.api.pixelDiagnostic(
        next,
        off: off,
        authorize: () => authorized,
      );
      if (mounted && authorized) {
        setState(() {
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
        });
      }
    } finally {
      if (mounted) {
        setState(() => working = false);
        widget.busyChanged(false);
      }
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
      setState(
        () => probe = PixelProbe(
          index: probe.index,
          group: probe.group,
          rgb: result,
        ),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    final enabled = authorized && !working;
    return WorkspaceBody(
      children: [
        const WorkspaceHeader(
          title: 'Pixel diagnostics',
          icon: PakarIcons.circuitry,
          subtitle:
              'Physical wire order · 480 pixels · all other pixels cleared',
        ),
        WorkspaceSection(
          title: 'Position',
          icon: PakarIcons.gridFour,
          children: [
            SegmentedButton<bool>(
              segments: const [
                ButtonSegment(value: false, label: Text('1 pixel')),
                ButtonSegment(value: true, label: Text('6-pixel group')),
              ],
              selected: {probe.group},
              onSelectionChanged: enabled
                  ? (choice) => setState(() {
                      probe = PixelProbe(
                        index: probe.index,
                        group: choice.single,
                        rgb: probe.rgb,
                      );
                      position.text = '${probe.first + 1}';
                    })
                  : null,
            ),
            const SizedBox(height: 20),
            Text(
              probe.range,
              style: Theme.of(context).textTheme.headlineMedium,
            ),
            Text(
              'Wire index ${probe.first}${probe.group ? '–${probe.last}' : ''} (zero-based) · step ${probe.count}',
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: Theme.of(context).colorScheme.onSurfaceVariant,
              ),
            ),
            const SizedBox(height: 16),
            Row(
              children: [
                Expanded(
                  child: OutlinedButton.icon(
                    key: const Key('pixel-previous'),
                    onPressed: enabled && probe.canPrevious
                        ? () => send(probe.move(-1))
                        : null,
                    icon: const Icon(Icons.arrow_back),
                    label: const Text('Previous'),
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
                    label: const Text('Next'),
                  ),
                ),
              ],
            ),
            const SizedBox(height: 16),
            TextField(
              controller: position,
              enabled: enabled,
              keyboardType: TextInputType.number,
              decoration: const InputDecoration(
                labelText: 'Go to physical pixel',
                helperText: '1–480. Groups align to 1–6, 7–12, …, 475–480.',
              ),
              onSubmitted: enabled
                  ? (_) {
                      final next = readPosition();
                      if (next != null) send(next);
                    }
                  : null,
            ),
          ],
        ),
        WorkspaceSection(
          title: 'Output',
          icon: PakarIcons.lightning,
          children: [
            Wrap(
              spacing: 8,
              runSpacing: 8,
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
                        ? (_) => setState(
                            () => probe = PixelProbe(
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
            const SizedBox(height: 18),
            Wrap(
              spacing: 12,
              runSpacing: 12,
              children: [
                FilledButton.icon(
                  key: const Key('pixel-show'),
                  icon: const Icon(PakarIcons.play),
                  label: const Text('Show'),
                  onPressed: enabled
                      ? () {
                          final next = readPosition();
                          if (next != null) send(next);
                        }
                      : null,
                ),
                OutlinedButton.icon(
                  key: const Key('pixel-off'),
                  icon: const Icon(PakarIcons.circle),
                  label: const Text('All off'),
                  onPressed: enabled ? () => send(probe, off: true) : null,
                ),
              ],
            ),
            const SizedBox(height: 16),
            if (working) const LinearProgressIndicator(),
            if (!authorized)
              const Text(
                'This board connection changed or is unavailable. Reopen diagnostics after reconnecting.',
              ),
            if (applied != null && authorized)
              Text(
                applied!,
                style: TextStyle(
                  color: Theme.of(context).colorScheme.secondary,
                ),
              ),
            Text(message, style: Theme.of(context).textTheme.bodySmall),
            const SizedBox(height: 8),
            const Text(
              'Uses effects output without fades. Closing this page leaves the last frame on the board; use All off to clear it.',
            ),
          ],
        ),
      ],
    );
  }
}
